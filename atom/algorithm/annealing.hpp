#ifndef ATOM_ALGORITHM_ANNEALING_HPP
#define ATOM_ALGORITHM_ANNEALING_HPP

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

#ifdef ATOM_USE_SIMD
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

#include "atom/error/exception.hpp"
#include "spdlog/spdlog.h"

template <typename ProblemType, typename SolutionType>
concept AnnealingProblem =
    requires(ProblemType problemInstance, SolutionType solutionInstance) {
        {
            problemInstance.energy(solutionInstance)
        } -> std::floating_point;  // 更精确的返回类型约束
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
    std::unique_ptr<std::vector<std::pair<int, double>>> energy_history_ =
        std::make_unique<std::vector<std::pair<int, double>>>();

    void optimizeThread();

    void restartOptimization() {
        std::lock_guard lock(best_mutex_);
        if (current_restart_ < restart_interval_) {
            current_restart_++;
            return;
        }

        spdlog::info("Performing restart optimization");
        auto newSolution = problem_instance_.randomSolution();
        double newEnergy = problem_instance_.energy(newSolution);

        if (newEnergy < best_energy_) {
            best_solution_ = newSolution;
            best_energy_ = newEnergy;
            total_restarts_++;
            current_restart_ = 0;
            spdlog::info("Restart found better solution with energy: {}",
                         best_energy_);
        }
    }

    void updateStatistics(int iteration, double energy) {
        total_steps_++;
        energy_history_->emplace_back(iteration, energy);

        // Keep history size manageable
        if (energy_history_->size() > 1000) {
            energy_history_->erase(energy_history_->begin());
        }
    }

    void checkpoint() {
        std::lock_guard lock(best_mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

        spdlog::info("Checkpoint at {} seconds:", elapsed.count());
        spdlog::info("  Best energy: {}", best_energy_);
        spdlog::info("  Total steps: {}", total_steps_.load());
        spdlog::info("  Accepted steps: {}", accepted_steps_.load());
        spdlog::info("  Rejected steps: {}", rejected_steps_.load());
        spdlog::info("  Restarts: {}", total_restarts_.load());
    }

    void resume() {
        std::lock_guard lock(best_mutex_);
        spdlog::info("Resuming optimization from checkpoint");
        spdlog::info("  Current best energy: {}", best_energy_);
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
        spdlog::info("Adaptive temperature adjustment. New cooling rate: {}",
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

    void setInitialTemperature(double temperature);

    void setCoolingRate(double rate);
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
    spdlog::info(
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
    spdlog::info("Setting cooling schedule to strategy: {}",
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
            spdlog::warn(
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
    spdlog::info("Progress callback has been set.");
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
void SimulatedAnnealing<ProblemType, SolutionType>::setStopCondition(
    std::function<bool(int, double, const SolutionType&)> condition) {
    stop_condition_ = condition;
    spdlog::info("Stop condition has been set.");
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
        spdlog::info("Thread {} started with initial energy: {}",
                     threadIdToString(), currentEnergy);

        {
            std::lock_guard lock(best_mutex_);
            if (currentEnergy < best_energy_) {
                best_solution_ = currentSolution;
                best_energy_ = currentEnergy;
                spdlog::info("New best energy found: {}", best_energy_);
            }
        }

        for (int iteration = 0;
             iteration < max_iterations_ && !should_stop_.load(); ++iteration) {
            double temperature = cooling_schedule_(iteration);
            if (temperature <= 0) {
                spdlog::warn(
                    "Temperature has reached zero or below at iteration {}.",
                    iteration);
                break;
            }

            auto neighborSolution = problem_instance_.neighbor(currentSolution);
            double neighborEnergy = problem_instance_.energy(neighborSolution);

            double energyDifference = neighborEnergy - currentEnergy;
            spdlog::info(
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
                spdlog::info(
                    "Solution accepted at iteration {} with energy: {}",
                    iteration, currentEnergy);

                std::lock_guard lock(best_mutex_);
                if (currentEnergy < best_energy_) {
                    best_solution_ = currentSolution;
                    best_energy_ = currentEnergy;
                    spdlog::info("New best energy updated to: {}",
                                 best_energy_);
                }
            } else {
                rejected_steps_++;
            }

            updateStatistics(iteration, currentEnergy);
            restartOptimization();

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
                    spdlog::error("Exception in progress_callback_: {}",
                                  e.what());
                }
            }

            if (stop_condition_ &&
                stop_condition_(iteration, currentEnergy, currentSolution)) {
                should_stop_.store(true);
                spdlog::info("Stop condition met at iteration {}.", iteration);
                break;
            }
        }
        spdlog::info("Thread {} completed optimization with best energy: {}",
                     threadIdToString(), best_energy_);
    } catch (const std::exception& e) {
        spdlog::error("Exception in optimizeThread: {}", e.what());
    }
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
auto SimulatedAnnealing<ProblemType, SolutionType>::optimize(int numThreads)
    -> SolutionType {
    try {
        spdlog::info("Starting optimization with {} threads.", numThreads);
        if (numThreads < 1) {
            spdlog::warn("Invalid number of threads ({}). Defaulting to 1.",
                         numThreads);
            numThreads = 1;
        }

        std::vector<std::jthread> threads;
        threads.reserve(numThreads);

        for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            threads.emplace_back([this]() { optimizeThread(); });
            spdlog::info("Launched optimization thread {}.", threadIndex + 1);
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in optimize: {}", e.what());
        throw;
    }

    spdlog::info("Optimization completed with best energy: {}", best_energy_);
    return best_solution_;
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
auto SimulatedAnnealing<ProblemType, SolutionType>::getBestEnergy() -> double {
    std::lock_guard lock(best_mutex_);
    return best_energy_;
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
void SimulatedAnnealing<ProblemType, SolutionType>::setInitialTemperature(
    double temperature) {
    if (temperature <= 0) {
        THROW_INVALID_ARGUMENT("Initial temperature must be positive");
    }
    initial_temperature_ = temperature;
    spdlog::info("Initial temperature set to: {}", temperature);
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
void SimulatedAnnealing<ProblemType, SolutionType>::setCoolingRate(
    double rate) {
    if (rate <= 0 || rate >= 1) {
        THROW_INVALID_ARGUMENT("Cooling rate must be between 0 and 1");
    }
    cooling_rate_ = rate;
    spdlog::info("Cooling rate set to: {}", rate);
}

inline TSP::TSP(const std::vector<std::pair<double, double>>& cities)
    : cities_(cities) {
    spdlog::info("TSP instance created with {} cities.", cities_.size());
}

inline auto TSP::energy(const std::vector<int>& solution) const -> double {
    double totalDistance = 0.0;
    size_t numCities = solution.size();

#ifdef ATOM_USE_SIMD
#ifdef __AVX2__
    // AVX2 implementation
    __m256d totalDistanceVec = _mm256_setzero_pd();

    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        __m256d v1 = _mm256_set_pd(0.0, 0.0, y1, x1);
        __m256d v2 = _mm256_set_pd(0.0, 0.0, y2, x2);
        __m256d diff = _mm256_sub_pd(v1, v2);
        __m256d squared = _mm256_mul_pd(diff, diff);

        // Extract x^2 and y^2
        __m128d low = _mm256_extractf128_pd(squared, 0);
        double dx_squared = _mm_cvtsd_f64(low);
        double dy_squared = _mm_cvtsd_f64(_mm_permute_pd(low, 1));

        // Calculate distance and add to total
        double distance = std::sqrt(dx_squared + dy_squared);
        totalDistance += distance;
    }

#elif defined(__ARM_NEON)
    // ARM NEON implementation
    float32x4_t totalDistanceVec = vdupq_n_f32(0.0f);

    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        float32x2_t p1 =
            vset_f32(static_cast<float>(x1), static_cast<float>(y1));
        float32x2_t p2 =
            vset_f32(static_cast<float>(x2), static_cast<float>(y2));

        float32x2_t diff = vsub_f32(p1, p2);
        float32x2_t squared = vmul_f32(diff, diff);

        // Sum x^2 + y^2 and take sqrt
        float sum = vget_lane_f32(vpadd_f32(squared, squared), 0);
        totalDistance += std::sqrt(static_cast<double>(sum));
    }

#else
    // Fallback SIMD implementation for other architectures
    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        double deltaX = x1 - x2;
        double deltaY = y1 - y2;
        totalDistance += std::sqrt(deltaX * deltaX + deltaY * deltaY);
    }
#endif
#else
    // Standard optimized implementation
    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        double deltaX = x1 - x2;
        double deltaY = y1 - y2;
        totalDistance += std::hypot(deltaX, deltaY);
    }
#endif

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
        spdlog::info(
            "Generated neighbor solution by swapping indices {} and {}.",
            index1, index2);
    } catch (const std::exception& e) {
        spdlog::error("Exception in TSP::neighbor: {}", e.what());
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
        spdlog::info("Generated random solution.");
    } catch (const std::exception& e) {
        spdlog::error("Exception in TSP::randomSolution: {}", e.what());
        throw;
    }
    return solution;
}

#endif  // ATOM_ALGORITHM_ANNEALING_HPP
