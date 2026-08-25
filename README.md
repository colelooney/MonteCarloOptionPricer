# Monte Carlo Option Pricer

A C++ Monte Carlo option pricer built in stages, as a self-directed project applying and extending C++ skills toward quantitative finance / MFE preparation.

## Current State

**Stage 1 - Baseline (done):** European call priced via Monte Carlo under Geometric Brownian Motion, verified against the closed-form Black-Scholes price.

**Stage 2 - Variance reduction (done):** Antithetic variates reduce simulation noise for a given trial count.

**Stage 3 - Path-dependent payoffs (done):** The engine now simulates full price paths (not just a single terminal draw), and the option hierarchy has grown to a small class structure supporting three contract types, each in call or put form:

- `Asset` - immutable market data (`S0`, `sigma`)
- `Option` - abstract base holding immutable contract data (`K`, `T`, `numSteps`, `mode`); declares a pure virtual `payoff(const std::vector<double>& path)`
- `EuropeanOption` - payoff from the path's terminal value only (`numSteps = 1`)
- `AsianOption` - payoff from the average price across `numObservations` points along the path
- `BarrierOption` - payoff conditioned on whether the path ever crosses a barrier level `H`; supports all four variants (up/down × knock-in/knock-out) via `Direction` and `Activation` enums
- `PricingResult` - a price paired with its standard error
- `MonteCarloEngine` - owns the RNG, risk-free rate, and pair count; `price(asset, option)` steps through time (exact GBM transitions, no discretization error), builds antithetic path pairs, and returns a `PricingResult`
- `BlackScholesPrice` - free function, closed-form European reference used for verification
- `main` picks the concrete option type at runtime via `std::unique_ptr<Option>`, based on command-line flags

Verification for this stage combines several approaches, since not every option type has a closed form to check against: European against Black-Scholes across a range of strikes; Asian cross-checked against an independent from-scratch reimplementation and against its own reduction to the European case (`numObservations = 1`); barrier options checked via in-out parity (knock-in + knock-out price ≈ the equivalent vanilla price); and put-call parity (`C - P = S0 - K·e^(-rT)`) is now run automatically at the end of every program execution, regardless of which option was priced, as a standing correctness check.

## Next Steps

**Stage 4 - Tail risk / Pricing and Metric Extension:** Replace GBM with jump-diffusion (Merton model). Compute VaR and CVaR / Expected Shortfall from the simulated payoff distribution. Extend the barrier structure toward a full autocallable (multiple observation dates, an early-redemption trigger, a downside knock-in at maturity). Optionally calibrate to a real index.

**Stage 5 - Performance:** Profile the simulation loop; apply move semantics and cache-friendly data layout; avoid unnecessary per-trial copies (each pair currently allocates two path vectors).

**Combined target:** an autocallable-style barrier option pricer with jump-diffusion and Expected Shortfall reporting.

## Build

```
g++ -std=c++17 -O2 MonteCarloOptionPricer.cpp -o pricer
```

## Usage

Run with no flags to price a European call with the built-in defaults:

```
./pricer
```

Every parameter can be overridden individually via flags — anything you don't pass keeps its default:

| Flag | Meaning | Default |
|---|---|---|
| `--spot` | Initial asset price, `S0` | `100` |
| `--strike` | Option strike, `K` | `100` |
| `--rate` | Risk-free rate, `r` | `0.05` |
| `--vol` | Volatility, `sigma` | `0.2` |
| `--maturity` | Time to maturity in years, `T` | `1` |
| `--trials` | Number of antithetic pairs simulated | `10000000` |
| `--type` | `european`, `asian`, or `barrier` | `european` |
| `--side` | `call` or `put` | `call` |
| `--observations` | Path points to simulate — averaging dates for Asian, monitoring frequency for barrier | `12` |
| `--barrier` | Barrier level `H` (barrier options only) | `80` |
| `--direction` | `up` or `down` (barrier options only) | `down` |
| `--activation` | `in` (knock-in) or `out` (knock-out) (barrier options only) | `out` |

Examples:

```
./pricer --strike 110 --vol 0.25
./pricer --type asian --side put --observations 52
./pricer --type barrier --direction down --activation in --barrier 70 --side put
```

Every run ends with a put-call parity check on a fresh European call/put at the same spot, strike, rate, and maturity — a PASS/FAIL line comparing the simulated `C - P` against the exact identity `S0 - K·e^(-rT)`, independent of whatever option type was actually requested.

Notes: an unrecognized flag (e.g. a typo) prints a warning and is ignored rather than stopping the program; a flag with no value after it also warns and stops parsing further arguments; unrecognized values for `--type`, `--direction`, or `--activation` silently fall back to their defaults rather than erroring.
