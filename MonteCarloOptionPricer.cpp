#include <random>
#include <iostream>
#include <cmath>
#include <algorithm>

class Asset{ // hold immutable asset data
    public: // will be used in other classes
        Asset(double S0,double sigma):S0(S0),sigma(sigma){}
        const double S0;
        const double sigma;
};

struct pricingResult{
    pricingResult(double price, double error):price(price),error(error){}
    double price;
    double error;
};

pricingResult MonteCarloPrice(Asset asset, double r, double K, double T, int N){
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
    return pricingResult(price,standard_error_discounted);
}

pricingResult MonteCarloPriceWithAntithetic(Asset asset, double r, double K, double T, int N){
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
    return pricingResult(price,standard_error_discounted);
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
    int N = 100000;
    Asset asset(S0,sigma);
    
    double monte_carlo_price = MonteCarloPrice(asset,r,K,T,N).price;
    double monte_carlo_error = MonteCarloPrice(asset,r,K,T,N).error;
    double closed_form_price = BlackScholesPrice(asset,r,K,T);
    double antithetic_price = MonteCarloPriceWithAntithetic(asset,r,K,T,N).price; //compute N pairs
    double antithetic_price_half = MonteCarloPriceWithAntithetic(asset,r,K,T,N/2).price; //compute N prices
    double antithetic_error = MonteCarloPriceWithAntithetic(asset,r,K,T,N).error; //compute N pairs
    double antithetic_error_half = MonteCarloPriceWithAntithetic(asset,r,K,T,N/2).error; //compute N prices

    std::cout << "Monte Carlo Price " << monte_carlo_price << ", error: "  <<(monte_carlo_error) << std::endl;
    std::cout << "Closed form Price " << closed_form_price << std::endl;
    std::cout << "Monte Carlo Anthetic N Pairs Price " << antithetic_price << ", error: " << (antithetic_error) << std::endl;
    std::cout << "Monte Carlo Anthetic N Prices Price " << antithetic_price_half << ", error: "  << (antithetic_error_half) << std::endl;
}