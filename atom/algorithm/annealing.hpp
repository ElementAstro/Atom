#ifndef ATOM_ALGORITHM_ANNEALING_HPP
#define ATOM_ALGORITHM_ANNEALING_HPP

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <future>
#include <limits>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

#ifdef USE_SIMD
#ifdef __x86_64__
#include <immintrin.h>
#elif __aarch64__
#include <arm_neon.h>
#endif
#endif

#ifdef ATOM_USE_BOOST
#include <boost/random.hpp>
#include <boost/thread.hpp>
#endif

#include "atom/log/loguru.hpp"

// Define a concept for a problem that Simulated Annealing can solve
template <typename ProblemType, typename SolutionType>
concept AnnealingProblem =
    requires(ProblemType problemInstance, SolutionType solutionInstance) {
        {
            problemInstance.energy(solutionInstance)
        } -> std::convertible_to<double>;
        {
            problemInstance.neighbor(solutionInstance)
        } -> std::same_as<SolutionType>;
        { problemInstance.randomSolution() } -> std::same_as<SolutionType>;
    };

// Different cooling strategies for temperature reduction
enum class AnnealingStrategy {
    LINEAR,
    EXPONENTIAL,
    LOGARITHMIC,
    GEOMETRIC,
    QUADRATIC,
    HYPERBOLIC,
    ADAPTIVE
};

// Simulated Annealing algorithm implementation
template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
class SimulatedAnnealing {
private:
    ProblemType& problem_instance_;
    std::function<double(int)> cooling_schedule_;
    int max_iterations_;
    double initial_temperature_;
    AnnealingStrategy cooling_strategy_;
    std::function<void(int, double, const SolutionType&)> progress_callback_;
    std::function<bool(int, double, const SolutionType&)> stop_condition_;
    std::atomic<bool> should_stop_{false};

    std::mutex best_mutex_;
    SolutionType best_solution_;
    double best_energy_ = std::numeric_limits<double>::max();

    static constexpr int K_DEFAULT_MAX_ITERATIONS = 1000;
    static constexpr double K_DEFAULT_INITIAL_TEMPERATURE = 100.0;
    double cooling_rate_ = 0.95;
    int restart_interval_ = 0;
    int current_restart_ = 0;
    std::atomic<int> total_restarts_{0};
    std::atomic<int> total_steps_{0};
    std::atomic<int> accepted_steps_{0};
    std::atomic<int> rejected_steps_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::vector<std::pair<int, double>> energy_history_;

    void optimizeThread();

    void restartOptimization() {
        std::lock_guard lock(best_mutex_);
        if (current_restart_ < restart_interval_) {
            current_restart_++;
            return;
        }

        LOG_F(INFO, "Performing restart optimization");
        auto newSolution = problem_instance_.randomSolution();
        double newEnergy = problem_instance_.energy(newSolution);

        if (newEnergy < best_energy_) {
            best_solution_ = newSolution;
            best_energy_ = newEnergy;
            total_restarts_++;
            current_restart_ = 0;
            LOG_F(INFO, "Restart found better solution with energy: {}",
                  best_energy_);
        }
    }

    void updateStatistics(int iteration, double energy) {
        total_steps_++;
        energy_history_.emplace_back(iteration, energy);

        // Keep history size manageable
        if (energy_history_.size() > 1000) {
            energy_history_.erase(energy_history_.begin());
        }
    }

    void checkpoint() {
        std::lock_guard lock(best_mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

        LOG_F(INFO, "Checkpoint at {} seconds:", elapsed.count());
        LOG_F(INFO, "  Best energy: {}", best_energy_);
        LOG_F(INFO, "  Total steps: {}", total_steps_.load());
        LOG_F(INFO, "  Accepted steps: {}", accepted_steps_.load());
        LOG_F(INFO, "  Rejected steps: {}", rejected_steps_.load());
        LOG_F(INFO, "  Restarts: {}", total_restarts_.load());
    }

    void resume() {
        std::lock_guard lock(best_mutex_);
        LOG_F(INFO, "Resuming optimization from checkpoint");
        LOG_F(INFO, "  Current best energy: {}", best_energy_);
    }

    void adaptTemperature(double acceptance_rate) {
        if (cooling_strategy_ != AnnealingStrategy::ADAPTIVE) {
            return;
        }

        // Adjust temperature based on acceptance rate
        const double target_acceptance = 0.44;  // Optimal acceptance rate
        if (acceptance_rate > target_acceptance) {
            cooling_rate_ *= 0.99;  // Slow down cooling
        } else {
            cooling_rate_ *= 1.01;  // Speed up cooling
        }

        // Keep cooling rate within reasonable bounds
        cooling_rate_ = std::clamp(cooling_rate_, 0.8, 0.999);
        LOG_F(INFO, "Adaptive temperature adjustment. New cooling rate: {}",
              cooling_rate_);
    }

public:
    class Builder {
    public:
        Builder(ProblemType& problemInstance)
            : problem_instance_(problemInstance) {}

        Builder& setCoolingStrategy(AnnealingStrategy strategy) {
            cooling_strategy_ = strategy;
            return *this;
        }

        Builder& setMaxIterations(int iterations) {
            max_iterations_ = iterations;
            return *this;
        }

        Builder& setInitialTemperature(double temperature) {
            initial_temperature_ = temperature;
            return *this;
        }

        Builder& setCoolingRate(double rate) {
            cooling_rate_ = rate;
            return *this;
        }

        Builder& setRestartInterval(int interval) {
            restart_interval_ = interval;
            return *this;
        }

        SimulatedAnnealing build() { return SimulatedAnnealing(*this); }

    private:
        ProblemType& problem_instance_;
        AnnealingStrategy cooling_strategy_ = AnnealingStrategy::EXPONENTIAL;
        int max_iterations_ = K_DEFAULT_MAX_ITERATIONS;
        double initial_temperature_ = K_DEFAULT_INITIAL_TEMPERATURE;
        double cooling_rate_ = 0.95;
        int restart_interval_ = 0;
    };

    explicit SimulatedAnnealing(const Builder& builder);

    void setCoolingSchedule(AnnealingStrategy strategy);

    void setProgressCallback(
        std::function<void(int, double, const SolutionType&)> callback);

    void setStopCondition(
        std::function<bool(int, double, const SolutionType&)> condition);

    auto optimize(int numThreads = 1) -> SolutionType;

    [[nodiscard]] auto getBestEnergy() -> double;
};

// Example TSP (Traveling Salesman Problem) implementation
class TSP {
private:
    std::vector<std::pair<double, double>> cities_;

public:
    explicit TSP(const std::vector<std::pair<double, double>>& cities);

    [[nodiscard]] auto energy(const std::vector<int>& solution) const -> double;

    [[nodiscard]] static auto neighbor(const std::vector<int>& solution)
        -> std::vector<int>;

    [[nodiscard]] auto randomSolution() const -> std::vector<int>;
};

// SimulatedAnnealing class implementation
template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
SimulatedAnnealing<ProblemType, SolutionType>::SimulatedAnnealing(
    const Builder& builder)
    : problem_instance_(builder.problem_instance_),
      max_iterations_(builder.max_iterations_),
      initial_temperature_(builder.initial_temperature_),
      cooling_strategy_(builder.cooling_strategy_),
      cooling_rate_(builder.cooling_rate_),
      restart_interval_(builder.restart_interval_) {
    LOG_F(INFO,
          "SimulatedAnnealing initialized with max_iterations: {}, "
          "initial_temperature: {}, cooling_strategy: {}, cooling_rate: {}",
          max_iterations_, initial_temperature_,
          static_cast<int>(cooling_strategy_), cooling_rate_);
    setCoolingSchedule(cooling_strategy_);
    start_time_ = std::chrono::steady_clock::now();
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
void SimulatedAnnealing<ProblemType, SolutionType>::setCoolingSchedule(
    AnnealingStrategy strategy) {
    cooling_strategy_ = strategy;
    LOG_F(INFO, "Setting cooling schedule to strategy: {}",
          static_cast<int>(strategy));
    switch (cooling_strategy_) {
        case AnnealingStrategy::LINEAR:
            cooling_schedule_ = [this](int iteration) {
                return initial_temperature_ *
                       (1 - static_cast<double>(iteration) / max_iterations_);
            };
            break;
        case AnnealingStrategy::EXPONENTIAL:
            cooling_schedule_ = [this](int iteration) {
                return initial_temperature_ *
                       std::pow(cooling_rate_, iteration);
            };
            break;
        case AnnealingStrategy::LOGARITHMIC:
            cooling_schedule_ = [this](int iteration) {
                if (iteration == 0)
                    return initial_temperature_;
                return initial_temperature_ / std::log(iteration + 2);
            };
            break;
        case AnnealingStrategy::GEOMETRIC:
            cooling_schedule_ = [this](int iteration) {
                return initial_temperature_ / (1 + cooling_rate_ * iteration);
            };
            break;
        case AnnealingStrategy::QUADRATIC:
            cooling_schedule_ = [this](int iteration) {
                return initial_temperature_ /
                       (1 + cooling_rate_ * iteration * iteration);
            };
            break;
        case AnnealingStrategy::HYPERBOLIC:
            cooling_schedule_ = [this](int iteration) {
                return initial_temperature_ /
                       (1 + cooling_rate_ * std::sqrt(iteration));
            };
            break;
        case AnnealingStrategy::ADAPTIVE:
            cooling_schedule_ = [this](int iteration) {
                return initial_temperature_ *
                       std::pow(cooling_rate_, iteration);
            };
            break;
        default:
            LOG_F(WARNING,
                  "Unknown cooling strategy. Defaulting to EXPONENTIAL.");
            cooling_schedule_ = [this](int iteration) {
                return initial_temperature_ *
                       std::pow(cooling_rate_, iteration);
            };
            break;
    }
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
void SimulatedAnnealing<ProblemType, SolutionType>::setProgressCallback(
    std::function<void(int, double, const SolutionType&)> callback) {
    progress_callback_ = callback;
    LOG_F(INFO, "Progress callback has been set.");
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
void SimulatedAnnealing<ProblemType, SolutionType>::setStopCondition(
    std::function<bool(int, double, const SolutionType&)> condition) {
    stop_condition_ = condition;
    LOG_F(INFO, "Stop condition has been set.");
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
void SimulatedAnnealing<ProblemType, SolutionType>::optimizeThread() {
    try {
#ifdef ATOM_USE_BOOST
        boost::random::random_device randomDevice;
        boost::random::mt19937 generator(randomDevice());
        boost::random::uniform_real_distribution<double> distribution(0.0, 1.0);
#else
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::uniform_real_distribution<double> distribution(0.0, 1.0);
#endif

        auto threadIdToString = [] {
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            return oss.str();
        };

        auto currentSolution = problem_instance_.randomSolution();
        double currentEnergy = problem_instance_.energy(currentSolution);
        LOG_F(INFO, "Thread {} started with initial energy: {}",
              threadIdToString(), currentEnergy);

        {
            std::lock_guard lock(best_mutex_);
            if (currentEnergy < best_energy_) {
                best_solution_ = currentSolution;
                best_energy_ = currentEnergy;
                LOG_F(INFO, "New best energy found: {}", best_energy_);
            }
        }

        for (int iteration = 0;
             iteration < max_iterations_ && !should_stop_.load(); ++iteration) {
            double temperature = cooling_schedule_(iteration);
            if (temperature <= 0) {
                LOG_F(WARNING,
                      "Temperature has reached zero or below at iteration {}.",
                      iteration);
                break;
            }

            auto neighborSolution = problem_instance_.neighbor(currentSolution);
            double neighborEnergy = problem_instance_.energy(neighborSolution);

            double energyDifference = neighborEnergy - currentEnergy;
            LOG_F(INFO,
                  "Iteration {}: Current Energy = {}, Neighbor Energy = "
                  "{}, Energy Difference = {}, Temperature = {}",
                  iteration, currentEnergy, neighborEnergy, energyDifference,
                  temperature);

            [[maybe_unused]] bool accepted = false;
            if (energyDifference < 0 ||
                distribution(generator) <
                    std::exp(-energyDifference / temperature)) {
                currentSolution = std::move(neighborSolution);
                currentEnergy = neighborEnergy;
                accepted = true;
                accepted_steps_++;
                LOG_F(INFO, "Solution accepted at iteration {} with energy: {}",
                      iteration, currentEnergy);

                std::lock_guard lock(best_mutex_);
                if (currentEnergy < best_energy_) {
                    best_solution_ = currentSolution;
                    best_energy_ = currentEnergy;
                    LOG_F(INFO, "New best energy updated to: {}", best_energy_);
                }
            } else {
                rejected_steps_++;
            }

            // Update statistics and check for restart
            updateStatistics(iteration, currentEnergy);
            restartOptimization();

            // Adapt temperature if using adaptive strategy
            if (total_steps_ > 0) {
                double acceptance_rate =
                    static_cast<double>(accepted_steps_) / total_steps_;
                adaptTemperature(acceptance_rate);
            }

            if (progress_callback_) {
                try {
                    progress_callback_(iteration, currentEnergy,
                                       currentSolution);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in progress_callback_: {}",
                          e.what());
                }
            }

            if (stop_condition_ &&
                stop_condition_(iteration, currentEnergy, currentSolution)) {
                should_stop_.store(true);
                LOG_F(INFO, "Stop condition met at iteration {}.", iteration);
                break;
            }
        }
        LOG_F(INFO, "Thread {} completed optimization with best energy: {}",
              threadIdToString(), best_energy_);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in optimizeThread: {}", e.what());
    }
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
auto SimulatedAnnealing<ProblemType, SolutionType>::optimize(int numThreads)
    -> SolutionType {
    LOG_F(INFO, "Starting optimization with {} threads.", numThreads);
    if (numThreads < 1) {
        LOG_F(WARNING, "Invalid number of threads ({}). Defaulting to 1.",
              numThreads);
        numThreads = 1;
    }

#ifdef ATOM_USE_BOOST
    std::vector<boost::thread> threads;
    for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
        threads.emplace_back([this]() { optimizeThread(); });
        LOG_F(INFO, "Launched optimization thread {}.", threadIndex + 1);
    }

    for (auto& thread : threads) {
        try {
            thread.join();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in optimization thread: {}", e.what());
        }
    }
#else
    std::vector<std::future<void>> futures;
    futures.reserve(numThreads);
    for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
        futures.emplace_back(
            std::async(std::launch::async, [this]() { optimizeThread(); }));
        LOG_F(INFO, "Launched optimization thread {}.", threadIndex + 1);
    }

    for (auto& future : futures) {
        try {
            future.wait();
            future.get();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in optimization thread: {}", e.what());
        }
    }
#endif

    LOG_F(INFO, "Optimization completed with best energy: {}", best_energy_);
    return best_solution_;
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
auto SimulatedAnnealing<ProblemType, SolutionType>::getBestEnergy() -> double {
    std::lock_guard lock(best_mutex_);
    return best_energy_;
}

// TSP class implementation
inline TSP::TSP(const std::vector<std::pair<double, double>>& cities)
    : cities_(cities) {
    LOG_F(INFO, "TSP instance created with %zu cities.", cities_.size());
}

inline auto TSP::energy(const std::vector<int>& solution) const -> double {
    double totalDistance = 0.0;
    size_t numCities = solution.size();

#ifdef USE_SIMD
    __m256d totalDistanceVec = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 3 < numCities; i += 4) {
        __m256d x1 = _mm256_set_pd(
            cities_[solution[i]].first, cities_[solution[i + 1]].first,
            cities_[solution[i + 2]].first, cities_[solution[i + 3]].first);
        __m256d y1 = _mm256_set_pd(
            cities_[solution[i]].second, cities_[solution[i + 1]].second,
            cities_[solution[i + 2]].second, cities_[solution[i + 3]].second);

        __m256d x2 =
            _mm256_set_pd(cities_[solution[(i + 1) % numCities]].first,
                          cities_[solution[(i + 2) % numCities]].first,
                          cities_[solution[(i + 3) % numCities]].first,
                          cities_[solution[(i + 4) % numCities]].first);
        __m256d y2 =
            _mm256_set_pd(cities_[solution[(i + 1) % numCities]].second,
                          cities_[solution[(i + 2) % numCities]].second,
                          cities_[solution[(i + 3) % numCities]].second,
                          cities_[solution[(i + 4) % numCities]].second);

        __m256d deltaX = _mm256_sub_pd(x1, x2);
        __m256d deltaY = _mm256_sub_pd(y1, y2);

        __m256d distance = _mm256_sqrt_pd(_mm256_add_pd(
            _mm256_mul_pd(deltaX, deltaX), _mm256_mul_pd(deltaY, deltaY)));
        totalDistanceVec = _mm256_add_pd(totalDistanceVec, distance);
    }

    // Horizontal addition to sum up the total distance in vector
    double distances[4];
    _mm256_storeu_pd(distances, totalDistanceVec);
    for (double d : distances) {
        totalDistance += d;
    }
#endif

    // Handle leftover cities that couldn't be processed in sets of 4
    for (size_t index = numCities - numCities % 4; index < numCities; ++index) {
        auto [x1, y1] = cities_[solution[index]];
        auto [x2, y2] = cities_[solution[(index + 1) % numCities]];
        double deltaX = x1 - x2;
        double deltaY = y1 - y2;
        totalDistance += std::sqrt(deltaX * deltaX + deltaY * deltaY);
    }

    LOG_F(INFO, "Computed energy (total distance): {}", totalDistance);
    return totalDistance;
}

inline auto TSP::neighbor(const std::vector<int>& solution)
    -> std::vector<int> {
    std::vector<int> newSolution = solution;
    try {
#ifdef ATOM_USE_BOOST
        boost::random::random_device randomDevice;
        boost::random::mt19937 generator(randomDevice());
        boost::random::uniform_int_distribution<int> distribution(
            0, static_cast<int>(solution.size()) - 1);
#else
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::uniform_int_distribution<int> distribution(
            0, static_cast<int>(solution.size()) - 1);
#endif
        int index1 = distribution(generator);
        int index2 = distribution(generator);
        std::swap(newSolution[index1], newSolution[index2]);
        LOG_F(INFO,
              "Generated neighbor solution by swapping indices {} and {}.",
              index1, index2);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in TSP::neighbor: {}", e.what());
        throw;
    }
    return newSolution;
}

inline auto TSP::randomSolution() const -> std::vector<int> {
    std::vector<int> solution(cities_.size());
    std::iota(solution.begin(), solution.end(), 0);
    try {
#ifdef ATOM_USE_BOOST
        boost::random::random_device randomDevice;
        boost::random::mt19937 generator(randomDevice());
        boost::range::random_shuffle(solution, generator);
#else
        std::random_device randomDevice;
        std::mt19937 generator(randomDevice());
        std::ranges::shuffle(solution, generator);
#endif
        LOG_F(INFO, "Generated random solution.");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in TSP::randomSolution: {}", e.what());
        throw;
    }
    return solution;
}

#endif  // ATOM_ALGORITHM_ANNEALING_HPP
