#include <random>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include <numeric>
#include <tuple>

class Asset{ // immutable market data for the underlying
    public:
        Asset(double S0,double sigma):S0(S0),sigma(sigma){}
        const double S0;
        const double sigma;
};

enum class putCall{put, call};

class Option{ // immutable data for contract itself
    public:
        Option(double K, double T, int numSteps, putCall mode = putCall::call)
            :K(K),T(T),numSteps(numSteps), mode(mode) {}
        virtual double payoff(const std::vector<double>& path) const = 0;
        virtual ~Option() = default;
        const double K;
        const double T;
        const int numSteps;
        const putCall mode;
};

struct Position {
    Position(const Option& option, double quantity): option(option),quantity(quantity){}
    const Option& option;
    double quantity; //positive = long, negative = short
};

class EuropeanOption : public Option {
    public:
        EuropeanOption(double K, double T, putCall mode): Option(K,T,1,mode) {}
        double payoff(const std::vector<double>& path) const override {
            if (mode == putCall::call) return std::max(path.back() - K, 0.0);
            else return std::max(K - path.back(), 0.0);
        }
};

class AsianOption : public Option{
    public:
        AsianOption(double K, double T, int numObservations, putCall mode): Option(K,T,numObservations,mode) {}
        double payoff(const std::vector<double>& path) const override {
            double sum = 0.0;
            for (double s: path) sum += s;
            double average = sum / path.size();
            if (mode == putCall::call) return std::max(average - K, 0.0);
            else return std::max(K - average, 0.0);
        }
};

enum class Direction { Up, Down };
enum class Activation { KnockIn, KnockOut };
class BarrierOption : public Option{
    public:
        BarrierOption(double K, double T, double H, Direction direction, Activation activation, int numObservations, putCall mode)
        : Option(K, T, numObservations, mode), H(H), direction(direction), activation(activation) {}
        double payoff(const std::vector<double>& path) const override{
            bool breached = false;
            for (double s: path){
                if (direction == Direction::Up   && s >= H) breached = true;
                if (direction == Direction::Down && s <= H) breached = true;
            }
            bool active = (activation == Activation::KnockIn) ? breached : !breached;
            double vanillaPayoff = (mode == putCall::call)
                ? std::max(path.back() - K, 0.0)
                : std::max(K - path.back(), 0.0);
            return active ? vanillaPayoff : 0.0;
        }
    private:
        double H;
        Direction direction;
        Activation activation;
};

struct PricingResult{
    PricingResult(double price, double standardError):price(price),standardError(standardError){}
    double price;
    double standardError;
};

struct RiskResult{
    RiskResult(double var, double cvar): var(var),cvar(cvar){}
    double var;
    double cvar;
};

class Portfolio : public Option{
    public:
        Portfolio(std::vector<Position> positions)
            : Option(0.0,positions.front().option.T,positions.front().option.numSteps),
            positions(std::move(positions))
        {
            for (const auto& pos: this -> positions) {
                if (pos.option.T != T || pos.option.numSteps != numSteps){
                    throw std::invalid_argument("Portfolio positions must share maturity and observation count");
                }
            }
        }

        double payoff(const std::vector<double>& path) const override{
            double total = 0.0;
            for (const auto& pos: positions){
                total += pos.quantity * pos.option.payoff(path);
            }
            return total;
        }
    private:
        std::vector<Position> positions;
};


class MonteCarloEngine{ // Simulation Process
    public:
        MonteCarloEngine(double r, int numPairs,double jumpIntensity,double jumpMean, double jumpVol, unsigned int seed = std::random_device{}()):
         r(r), numPairs(numPairs),jumpIntensity(jumpIntensity),jumpMean(jumpMean),jumpVol(jumpVol), gen(seed){}
        std::tuple<PricingResult,RiskResult> price(const Asset& asset, const Option& option) const {

            double dt = option.T / option.numSteps;
            double vol = asset.sigma * sqrt(dt);
            double confidence = 0.95;
            double jumpCompensator = exp(jumpMean + 0.5*jumpVol*jumpVol) - 1.0;
            double drift = (r - 0.5*asset.sigma*asset.sigma - jumpIntensity*jumpCompensator) * dt;

            double sumPairAverage = 0.0;
            double sumPairAverageSquared = 0.0;

            std::normal_distribution<double> z_dist(0.0, 1.0);
            double poissonMean = (jumpIntensity > 0.0) ? jumpIntensity * dt : 1.0;
            std::poisson_distribution<int> n_dist(poissonMean);
            std::vector<double> payoffs;
            std::vector<double> pAndL;
            payoffs.reserve(2 * numPairs);
            pAndL.reserve(2 * numPairs);
            for (int i = 0; i < numPairs; i++) {
                std::vector<double> pathUp, pathDown;
                pathUp.reserve(option.numSteps);
                pathDown.reserve(option.numSteps);

                double S_up   = asset.S0;
                double S_down = asset.S0;

                for (int step = 0; step < option.numSteps; step++){
                    double Z = z_dist(gen);
                    int n = 0;
                    if (jumpIntensity > 0.0){
                        n = n_dist(gen);
                    }
                    double jumpSum = 0.0;
                    if (n>0){
                        double Zj = z_dist(gen);
                        jumpSum = n*jumpMean + sqrt(n)*jumpVol*Zj;
                    }
                    S_up *= exp(drift + vol*Z + jumpSum);
                    S_down *= exp(drift - vol*Z + jumpSum);
                    pathUp.push_back(S_up);
                    pathDown.push_back(S_down);
                }
                double upPayoff = option.payoff(pathUp);
                double downPayoff = option.payoff(pathDown);
                double pairAverage = (upPayoff + downPayoff) / 2.0;
                payoffs.push_back(upPayoff);
                payoffs.push_back(downPayoff);
                sumPairAverage += pairAverage;
                sumPairAverageSquared += pairAverage * pairAverage;
            }

            double meanPairAverage = sumPairAverage / numPairs;
            double meanPairAverageSquared = sumPairAverageSquared / numPairs;
            double payoffVariance = meanPairAverageSquared - meanPairAverage * meanPairAverage;

            double discount = discountFactor(option.T);
            double price = meanPairAverage * discount;
            double standardError = sqrt(payoffVariance / numPairs) * discount;

            for (double payoff : payoffs) {
                pAndL.push_back(discount * payoff - price);
            }

            // clear payoffs from memory
            payoffs.clear();
            payoffs.shrink_to_fit();

            size_t k = static_cast<size_t>((1.0-confidence)*pAndL.size());
            std::nth_element(pAndL.begin(),pAndL.begin()+k,pAndL.end());

            double tailSum = std::accumulate(pAndL.begin(),pAndL.begin()+k,0.0);
            double cvar = -(tailSum/k);

            return {PricingResult(price, standardError), RiskResult(-pAndL[k],cvar)};
        }

    private:
        double r;
        int numPairs;
        double jumpIntensity;
        double jumpMean;
        double jumpVol;
        mutable std::mt19937_64 gen;

        double discountFactor(double T) const {return exp(-r*T);}
};

double BlackScholesPrice(Asset asset, double r, double K, double T){
    double d1= (log(asset.S0/K) +(r+0.5*asset.sigma*asset.sigma)*T)/(asset.sigma*sqrt(T));
    double d2 = d1 - asset.sigma*sqrt(T);
    double C = asset.S0*(0.5*(1+std::erf(d1/sqrt(2)))) - K * exp(-r*T) *0.5*(1+std::erf(d2/sqrt(2)));
    return C;
}

struct SimulationParams { // default option parameters
    double S0 = 100;
    double K = 100;
    double r = 0.05;
    double sigma = 0.2;
    double T = 1;
    int N = 10000000;
    std::string optionType = "european";
    std::string side = "call";
    int numObservations = 12;
    double H = 80;
    std::string direction = "down";
    std::string activation = "out";
    double jumpIntensity = 0;
    double jumpMean = 0;
    double jumpVol = 0;
    std::string portfolioName = "spread";
    double K2 = 110;
    int quantity = 1;
};

SimulationParams parseArgs(int argc, char* argv[]) {
    SimulationParams params;
    for (int i = 1; i < argc; i++) {
        std::string flag = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "Missing value for flag " << flag << std::endl;
            break;
        }
        std::string value = argv[++i];

        if      (flag == "--spot")     params.S0    = std::stod(value);
        else if (flag == "--strike")   params.K     = std::stod(value);
        else if (flag == "--rate")     params.r     = std::stod(value);
        else if (flag == "--vol")      params.sigma = std::stod(value);
        else if (flag == "--maturity") params.T     = std::stod(value);
        else if (flag == "--trials")   params.N     = std::stoi(value);
        else if (flag == "--type")     params.optionType = value;
        else if (flag == "--side")     params.side  = value;
        else if (flag == "--observations") params.numObservations = std::stoi(value);
        else if (flag == "--barrier")    params.H = std::stod(value);
        else if (flag == "--direction")  params.direction = value;
        else if (flag == "--activation") params.activation = value;
        else if (flag == "--jump-intensity") params.jumpIntensity = std::stod(value);
        else if (flag == "--jump-mean") params.jumpMean = std::stod(value);
        else if (flag == "--jump-vol") params.jumpVol = std::stod(value);
        else if (flag == "--portfolio") params.portfolioName = value;
        else if (flag == "--strike2")   params.K2 = std::stod(value);
        else if (flag == "--quantity") params.quantity = std::stod(value);
        else std::cerr << "Unknown flag: " << flag << std::endl;
    }
    return params;
}

void verifyPutCallParity(const Asset& asset, double r, double K, double T, int N,double jumpIntensity,double jumpMean,double jumpVol){
    MonteCarloEngine engine(r, N,jumpIntensity,jumpMean,jumpVol);
    EuropeanOption call(K, T, putCall::call);
    EuropeanOption put(K, T, putCall::put);


    auto [callResult, callRisk] = engine.price(asset, call);
    auto [putResult, putRisk]   = engine.price(asset, put);

    double lhs = callResult.price - putResult.price;
    double rhs = asset.S0 - K * exp(-r*T);
    double lhsError = sqrt(callResult.standardError*callResult.standardError
                          + putResult.standardError*putResult.standardError);
    double diff = std::abs(lhs - rhs);

    std::cout << "\n--- Put-call parity check (European, K=" << K << ", T=" << T << ") ---" << std::endl;
    std::cout << "Call price:        " << callResult.price << " +/- " << callResult.standardError << std::endl;
    std::cout << "Put price:         " << putResult.price  << " +/- " << putResult.standardError  << std::endl;
    std::cout << "C - P (simulated): " << lhs << " +/- " << lhsError << std::endl;
    std::cout << "S0 - K*e^(-rT):    " << rhs << std::endl;
    std::cout << "Difference:        " << diff << " (" << diff / lhsError << " combined std errors)" << std::endl;
    std::cout << (diff <= 3 * lhsError ? "PASS - within 3 standard errors" : "FAIL - outside 3 standard errors, investigate") << std::endl;
}

int main(int argc, char* argv[]){
    SimulationParams params = parseArgs(argc, argv);
    MonteCarloEngine engine(params.r, params.N,params.jumpIntensity,params.jumpMean,params.jumpVol);
    Asset asset(params.S0, params.sigma);

    if (params.optionType == "portfolio") {
        if (params.portfolioName == "spread") {
            EuropeanOption longCall(params.K, params.T, putCall::call);
            EuropeanOption shortCall(params.K2, params.T, putCall::call);
            Portfolio portfolio({ Position(longCall, 1.0), Position(shortCall, -1.0) });

            auto [result, risk] = engine.price(asset, portfolio);
            std::cout << "Portfolio (bull call spread) price " << result.price << " +/- " << result.standardError << std::endl;
            std::cout << "VaR (95%): " << risk.var << std::endl;
            std::cout << "CVaR (95%): " << risk.cvar << std::endl;

        } else if (params.portfolioName == "netzero") {
            EuropeanOption call(params.K, params.T, putCall::call);
            Portfolio portfolio({ Position(call, 1.0), Position(call, -1.0) });

            auto [result, risk] = engine.price(asset, portfolio);
            std::cout << "Portfolio (net-zero check) price " << result.price << " +/- " << result.standardError << std::endl;
            std::cout << "VaR (95%): " << risk.var << std::endl;
            std::cout << "CVaR (95%): " << risk.cvar << std::endl;

        } else {
            std::cerr << "Unknown portfolio: " << params.portfolioName << std::endl;
        }
        return 0;
    }

    putCall side = (params.side == "put") ? putCall::put : putCall::call;

    std::unique_ptr<Option> option;
    if (params.optionType == "asian"){
        option = std::make_unique<AsianOption>(params.K, params.T, params.numObservations, side);
    } else if(params.optionType == "barrier"){
        Direction dir = (params.direction == "up") ? Direction::Up : Direction::Down;
        Activation act = (params.activation == "in") ? Activation::KnockIn : Activation::KnockOut;
        option = std::make_unique<BarrierOption>(params.K, params.T, params.H, dir, act, params.numObservations, side);
    } else {
        option = std::make_unique<EuropeanOption>(params.K, params.T, side);
    }

    double closedFormPrice = BlackScholesPrice(asset, params.r, params.K, params.T);

    auto [monteCarloResult, riskResult] = engine.price(asset, *option);

    std::cout << "Closed Form Price (European Call) " << closedFormPrice << std::endl;
    std::cout << "Monte Carlo Price " << params.optionType << " " << params.side << " " << monteCarloResult.price << std::endl;
    std::cout << "Monte Carlo Error " << monteCarloResult.standardError << std::endl;
    std::cout << "VaR (95%): " << riskResult.var << std::endl;
    std::cout << "CVaR (95%): " << riskResult.cvar << std::endl;

    verifyPutCallParity(asset, params.r, params.K, params.T, params.N,params.jumpIntensity,params.jumpMean,params.jumpVol);
}
