#include <random>
#include <iostream>
#include <cmath>
#include <algorithm>

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

PricingResult MonteCarloPrice(Asset asset, double r, double K, double T, int N){
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::normal_distribution<double> z_dist(0.0, 1.0);

    double average_payoff = 0;
    double average_payoff_squared = 0;
    for (int i = 0; i < N; i++){
        double Z = z_dist(gen);
        double S_t = asset.S0 * exp((r - 0.5 * asset.sigma*asset.sigma)*T + asset.sigma*sqrt(T)*Z);
        double payoff = std::max(S_t-K,0.0);
        average_payoff += payoff;
        double payoff_squared = payoff*payoff;
        average_payoff_squared += payoff_squared;
    }
    average_payoff /= N;
    average_payoff_squared /= N;
    double price = average_payoff * exp(-r*T);
    double variance = average_payoff_squared - average_payoff * average_payoff;
    double standard_error = sqrt(variance) / sqrt(N);
    double standard_error_discounted = standard_error * exp(-r*T);
    return PricingResult(price,standard_error_discounted);
}

PricingResult MonteCarloPriceWithAntithetic(Asset asset, double r, double K, double T, int N){
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::normal_distribution<double> z_dist(0.0, 1.0);

    double average_payoff = 0;
    double average_payoff_squared = 0;
    for (int i = 0; i < N; i++){
        double Z = z_dist(gen);
        double S_t = asset.S0 * exp((r - 0.5 * asset.sigma*asset.sigma)*T + asset.sigma*sqrt(T)*Z);
        double payoff_one = std::max(S_t-K,0.0);
        average_payoff += payoff_one;
        // compute antithetic
        S_t = asset.S0 * exp((r - 0.5 * asset.sigma*asset.sigma)*T - asset.sigma*sqrt(T)*Z);
        double payoff_two = std::max(S_t-K,0.0);
        average_payoff += payoff_two;
        average_payoff_squared += (payoff_one+payoff_two)*(payoff_one+payoff_two)/2; //payoffs are not independent
    }
    average_payoff /= 2*N;
    average_payoff_squared /= 2*N;
    double price = average_payoff * exp(-r*T);
    double variance = average_payoff_squared - average_payoff * average_payoff;
    double standard_error = sqrt(variance) / sqrt(N);
    double standard_error_discounted = standard_error * exp(-r*T);
    return PricingResult(price,standard_error_discounted);
}

double BlackScholesPrice(Asset asset, double r, double K, double T){
    double d1= (log(asset.S0/K) +(r+0.5*asset.sigma*asset.sigma)*T)/(asset.sigma*sqrt(T));
    double d2 = d1 - asset.sigma*sqrt(T);
    double C = asset.S0*(0.5*(1+std::erf(d1/sqrt(2)))) - K * exp(-r*T) *0.5*(1+std::erf(d2/sqrt(2)));
    return C;
}

int main(){
    double S0 = 100;
    double K = 100;
    double r = 0.05;
    double sigma = 0.2;
    double T = 1;
    int N = 10000000;
    Asset asset(S0,sigma);
    Option option(K,T);

    double closedFormPrice = BlackScholesPrice(asset,r,K,T);

    MonteCarloEngine engine(r,N);
    PricingResult monteCarloResult = engine.price(asset,option);

    double monteCarloPrice = monteCarloResult.price;
    double monteCarloError = monteCarloResult.standardError;
    
    std::cout << "Closed Form Price " << closedFormPrice << std::endl;
    std::cout << "Monte Carlo Price " << monteCarloPrice << std::endl;
    std::cout << "Monte Carlo Error " << monteCarloError << std::endl;
}