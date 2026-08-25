#include <random>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

class Asset{ // immutable market data for the underlying
    public: 
        Asset(double S0,double sigma):S0(S0),sigma(sigma){}
        const double S0;
        const double sigma;
};

class Option{ // immutable data for contract itself
    public:
        Option(double K, double T, int numSteps) :K(K),T(T),numSteps(numSteps) {}
        virtual double payoff(const std::vector<double>& path) const = 0;
        virtual ~Option() = default;
        const double K;
        const double T;
        const int numSteps;
};

class EuropeanOption : public Option {
    public:
        EuropeanOption(double K, double T): Option(K,T,1) {}
        double payoff(const std::vector<double>& path) const override {
            return std::max(path.back() - K, 0.0);
        }
};

class AsianOption : public Option{
    public:
        AsianOption(double K, double T, int numObservations): Option(K,T,numObservations) {}
        double payoff(const std::vector<double>& path) const override {
            double sum = 0.0;
            for (double s: path) sum += s;
            return std::max(sum / path.size() - K,0.0);
        }
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
    int numObservations = 12;

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
        else if (flag == "--observations") params.numObservations = std::stoi(value);
        else std::cerr << "Unknown flag: " << flag << std::endl;
    }
    return params;
}

int main(int argc, char* argv[]){
    SimulationParams params = parseArgs(argc, argv);

    Asset asset(params.S0, params.sigma);
    AsianOption option(params.K, params.T,params.numObservations);

    double closedFormPrice = BlackScholesPrice(asset, params.r, params.K, params.T);

    MonteCarloEngine engine(params.r, params.N);
    PricingResult monteCarloResult = engine.price(asset, option);

    double monteCarloPrice = monteCarloResult.price;
    double monteCarloError = monteCarloResult.standardError;

    std::cout << "Closed Form Price (European Call) " << closedFormPrice << std::endl;
    std::cout << "Monte Carlo Price (Asial Call) " << monteCarloPrice << std::endl;
    std::cout << "Monte Carlo Error " << monteCarloError << std::endl;
}