#include <random>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <string>

class Asset{ // immutable market data for the underlying
    public: 
        Asset(double S0,double sigma):S0(S0),sigma(sigma){}
        const double S0;
        const double sigma;
};

class Option{ // immutable data for contract itself
    public:
        Option(double K, double T) :K(K),T(T){}
        double payoff(double terminalPrice) const {
            return std::max(terminalPrice - K,0.0);
        }
        const double K;
        const double T;
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
            std::normal_distribution<double> z_dist(0.0, 1.0);

            double sumPairAverage = 0.0;
            double sumPairAverageSquared = 0.0;

            for (int i = 0; i < numPairs; i++) {
                double Z = z_dist(gen);

                double S_up   = asset.S0 * exp((r - 0.5*asset.sigma*asset.sigma)*option.T + asset.sigma*sqrt(option.T)*Z);
                double S_down = asset.S0 * exp((r - 0.5*asset.sigma*asset.sigma)*option.T - asset.sigma*sqrt(option.T)*Z);

                double pairAverage = (option.payoff(S_up) + option.payoff(S_down)) / 2.0;
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

struct SimulationParams {
    double S0 = 100;
    double K = 100;
    double r = 0.05;
    double sigma = 0.2;
    double T = 1;
    int N = 10000000;
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
        else std::cerr << "Unknown flag: " << flag << std::endl;
    }
    return params;
}

int main(int argc, char* argv[]){
    SimulationParams params = parseArgs(argc, argv);

    Asset asset(params.S0, params.sigma);
    Option option(params.K, params.T);

    double closedFormPrice = BlackScholesPrice(asset, params.r, params.K, params.T);

    MonteCarloEngine engine(params.r, params.N);
    PricingResult monteCarloResult = engine.price(asset, option);

    double monteCarloPrice = monteCarloResult.price;
    double monteCarloError = monteCarloResult.standardError;

    std::cout << "Closed Form Price " << closedFormPrice << std::endl;
    std::cout << "Monte Carlo Price " << monteCarloPrice << std::endl;
    std::cout << "Monte Carlo Error " << monteCarloError << std::endl;
}