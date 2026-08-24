# Monte Carlo Option Pricer

A C++ Monte Carlo option pricer built in stages, as a self-directed project applying and extending C++ skills toward quantitative finance / MFE preparation.

## Current State

**Stage 1 — Baseline (done):** European call priced via Monte Carlo under Geometric Brownian Motion, verified against the closed-form Black-Scholes price.

**Stage 2 — Variance reduction (done):** Antithetic variates reduce simulation noise for a given trial count, refactored into a small class structure:

- `Asset` — immutable market data (`S0`, `sigma`)
- `Option` — immutable contract data (`K`, `T`), owns its own `payoff()`
- `PricingResult` — a price paired with its standard error
- `MonteCarloEngine` — owns the RNG, risk-free rate, and pair count; `price(asset, option)` runs the antithetic simulation and returns a `PricingResult`
- `BlackScholesPrice` — free function, closed-form reference used for verification

Verified against Black-Scholes across a range of strikes (deep ITM to deep OTM) — Monte Carlo price consistently lands within 1-2 standard errors of the closed-form price.

## Next Steps

**Stage 3 — Path-dependent payoffs:** Move beyond a single terminal-price draw to full price-path simulation (Euler-Maruyama discretization of GBM). Implement an Asian (average-price) or barrier (knock-in/knock-out) option. Requires extending `Option::payoff()` to accept a full path rather than a single terminal value.

**Stage 4 — Tail risk / Pricing and Metric Extension:** Replace GBM with jump-diffusion (Merton model). Compute VaR and CVaR / Expected Shortfall from the simulated payoff distribution. Add a barrier/autocallable-style payoff structure. Optionally calibrate to a real index.

**Stage 5 — Performance:** Profile the simulation loop; apply move semantics and cache-friendly data layout; avoid unnecessary per-trial copies.

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

Examples:

```
./pricer --strike 110 --vol 0.25
./pricer --spot 95 --strike 100 --maturity 0.5 --trials 2000000
```

Notes: an unrecognized flag (e.g. a typo) prints a warning and is ignored rather than stopping the program; a flag with no value after it also warns and stops parsing further arguments.
