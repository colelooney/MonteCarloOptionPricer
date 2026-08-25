#include <random>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>

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

class MonteCarloEngine{ // Simulation Process
    public:
        MonteCarloEngine(double r, int numPairs, unsigned int seed = std::random_device{}()): r(r), numPairs(numPairs), gen(seed){}
        PricingResult price(const Asset& asset, const Option& option) const {

            double dt = option.T / option.numSteps;
            double drift = (r - 0.5*asset.sigma*asset.sigma) * dt;
            double vol = asset.sigma * sqrt(dt);

            double sumPairAverage = 0.0;
            double sumPairAverageSquared = 0.0;

            std::normal_distribution<double> z_dist(0.0, 1.0);

            for (int i = 0; i < numPairs; i++) {
                std::vector<double> pathUp, pathDown;
                pathUp.reserve(option.numSteps);
                pathDown.reserve(option.numSteps);

                double S_up   = asset.S0;
                double S_down = asset.S0;

                for (int step = 0; step < option.numSteps; step++){
                    double Z = z_dist(gen);
                    S_up *= exp(drift + vol*Z);
                    S_down *= exp(drift - vol*Z);
                    pathUp.push_back(S_up);
                    pathDown.push_back(S_down);
                }

                double pairAverage = (option.payoff(pathUp) + option.payoff(pathDown)) / 2.0;
                sumPairAverage += pairAverage;
                sumPairAverageSquared += pairAverage * pairAverage;
            }

            double meanPairAverage = sumPairAverage / numPairs;
            double meanPairAverageSquared = sumPairAverageSquared / numPairs;
            double payoffVariance = meanPairAverageSquared - meanPairAverage * meanPairAverage;

            double discount = discountFactor(option.T);
            double price = meanPairAverage * discount;
            double standardError = sqrt(payoffVariance / numPairs) * discount;

            return PricingResult(price, standardError);
        }

    private:
        double r;
        int numPairs;
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
        else std::cerr << "Unknown flag: " << flag << std::endl;
    }
    return params;
}

void verifyPutCallParity(const Asset& asset, double r, double K, double T, int N){
    MonteCarloEngine engine(r, N);
    EuropeanOption call(K, T, putCall::call);
    EuropeanOption put(K, T, putCall::put);

    PricingResult callResult = engine.price(asset, call);
    PricingResult putResult  = engine.price(asset, put);

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

    Asset asset(params.S0, params.sigma);
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

    MonteCarloEngine engine(params.r, params.N);
    PricingResult monteCarloResult = engine.price(asset, *option);

    std::cout << "Closed Form Price (European Call) " << closedFormPrice << std::endl;
    std::cout << "Monte Carlo Price " << params.optionType << " " << params.side << " " << monteCarloResult.price << std::endl;
    std::cout << "Monte Carlo Error " << monteCarloResult.standardError << std::endl;

    verifyPutCallParity(asset, params.r, params.K, params.T, params.N);
}
