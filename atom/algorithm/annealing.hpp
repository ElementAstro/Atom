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
#include "atom/utils/random.hpp"
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

    /**
     * @brief The main optimization loop executed by each thread.
     * @param seed A unique seed for the thread's random number generator.
     */
    void optimizeThread(unsigned int seed);

    /**
     * @brief Restarts the optimization process, potentially with a new random
     * solution.
     */
    void restartOptimization() {
        // Only lock when updating best_solution_ and best_energy_
        double newEnergy = 0.0;
        SolutionType newSolution;
        bool found_better = false;
        if (current_restart_ < restart_interval_) {
            current_restart_++;
            return;
        }
        spdlog::info("Performing restart optimization");
        newSolution = problem_instance_.randomSolution();
        newEnergy = problem_instance_.energy(newSolution);
        {
            std::lock_guard lock(best_mutex_);
            if (newEnergy < best_energy_) {
                best_solution_ = newSolution;
                best_energy_ = newEnergy;
                total_restarts_++;
                current_restart_ = 0;
                found_better = true;
            }
        }
        if (found_better) {
            spdlog::info("Restart found better solution with energy: {}",
                         best_energy_);
        }
    }

    /**
     * @brief Updates internal statistics for the optimization process.
     * @param iteration The current iteration number.
     * @param energy The current energy of the solution.
     */
    void updateStatistics(int iteration, double energy) {
        total_steps_++;
        energy_history_->emplace_back(iteration, energy);

        // Keep history size manageable
        if (energy_history_->size() > 1000) {
            energy_history_->erase(energy_history_->begin());
        }
    }

    /**
     * @brief Logs a checkpoint of the current optimization progress.
     */
    void checkpoint() {
        double best_energy_snapshot;
        int total_steps_snapshot, accepted_steps_snapshot,
            rejected_steps_snapshot, total_restarts_snapshot;
        {
            std::lock_guard lock(best_mutex_);
            best_energy_snapshot = best_energy_;
            total_steps_snapshot = total_steps_.load();
            accepted_steps_snapshot = accepted_steps_.load();
            rejected_steps_snapshot = rejected_steps_.load();
            total_restarts_snapshot = total_restarts_.load();
        }
        auto now = std::chrono::steady_clock::now();
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        spdlog::info("Checkpoint at {} seconds:", elapsed.count());
        spdlog::info("  Best energy: {}", best_energy_snapshot);
        spdlog::info("  Total steps: {}", total_steps_snapshot);
        spdlog::info("  Accepted steps: {}", accepted_steps_snapshot);
        spdlog::info("  Rejected steps: {}", rejected_steps_snapshot);
        spdlog::info("  Restarts: {}", total_restarts_snapshot);
    }

    /**
     * @brief Resumes the optimization process from a previous state.
     */
    void resume() {
        double best_energy_snapshot;
        {
            std::lock_guard lock(best_mutex_);
            best_energy_snapshot = best_energy_;
        }
        spdlog::info("Resuming optimization from checkpoint");
        spdlog::info("  Current best energy: {}", best_energy_snapshot);
    }

    /**
     * @brief Adapts the temperature based on the acceptance rate for adaptive
     * cooling.
     * @param acceptance_rate The current acceptance rate of new solutions.
     */
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
    /**
     * @brief Builder class for constructing SimulatedAnnealing objects.
     */
    class Builder {
    public:
        /**
         * @brief Constructs a Builder with a reference to the problem instance.
         * @param problemInstance The problem instance to be optimized.
         */
        Builder(ProblemType& problemInstance)
            : problem_instance_(problemInstance) {}

        /**
         * @brief Sets the cooling strategy for the simulated annealing.
         * @param strategy The annealing strategy to use.
         * @return Reference to the Builder for chaining.
         */
        Builder& setCoolingStrategy(AnnealingStrategy strategy) {
            cooling_strategy_ = strategy;
            return *this;
        }

        /**
         * @brief Sets the maximum number of iterations for the simulated
         * annealing.
         * @param iterations The maximum number of iterations.
         * @return Reference to the Builder for chaining.
         */
        Builder& setMaxIterations(int iterations) {
            max_iterations_ = iterations;
            return *this;
        }

        /**
         * @brief Sets the initial temperature for the simulated annealing.
         * @param temperature The initial temperature.
         * @return Reference to the Builder for chaining.
         */
        Builder& setInitialTemperature(double temperature) {
            initial_temperature_ = temperature;
            return *this;
        }

        /**
         * @brief Sets the cooling rate for the simulated annealing.
         * @param rate The cooling rate.
         * @return Reference to the Builder for chaining.
         */
        Builder& setCoolingRate(double rate) {
            cooling_rate_ = rate;
            return *this;
        }

        /**
         * @brief Sets the restart interval for the simulated annealing.
         * @param interval The number of iterations after which to consider a
         * restart.
         * @return Reference to the Builder for chaining.
         */
        Builder& setRestartInterval(int interval) {
            restart_interval_ = interval;
            return *this;
        }

        /**
         * @brief Builds and returns a SimulatedAnnealing object.
         * @return A configured SimulatedAnnealing object.
         */
        SimulatedAnnealing build() { return SimulatedAnnealing(*this); }

        ProblemType& problem_instance_;
        AnnealingStrategy cooling_strategy_ = AnnealingStrategy::EXPONENTIAL;
        int max_iterations_ = K_DEFAULT_MAX_ITERATIONS;
        double initial_temperature_ = K_DEFAULT_INITIAL_TEMPERATURE;
        double cooling_rate_ = 0.95;
        int restart_interval_ = 0;
    };

    /**
     * @brief Constructs a SimulatedAnnealing object using a Builder.
     * @param builder The Builder object containing configuration.
     */
    explicit SimulatedAnnealing(const Builder& builder);

    /**
     * @brief Move constructor.
     */
    SimulatedAnnealing(SimulatedAnnealing&& other) noexcept;

    /**
     * @brief Move assignment operator.
     */
    SimulatedAnnealing& operator=(SimulatedAnnealing&& other) noexcept;

    /**
     * @brief Destructor - ensures proper cleanup of resources.
     */
    ~SimulatedAnnealing() = default;

    /**
     * @brief Sets the cooling schedule based on the specified strategy.
     * @param strategy The annealing strategy to use.
     */
    void setCoolingSchedule(AnnealingStrategy strategy);

    /**
     * @brief Sets a callback function to report progress during optimization.
     * @param callback The function to call with iteration, current energy, and
     * current solution.
     */
    void setProgressCallback(
        std::function<void(int, double, const SolutionType&)> callback);

    /**
     * @brief Sets a condition function to stop the optimization prematurely.
     * @param condition The function to call with iteration, current energy, and
     * current solution. Returns true to stop, false to continue.
     */
    void setStopCondition(
        std::function<bool(int, double, const SolutionType&)> condition);

    /**
     * @brief Starts the optimization process.
     * @param numThreads The number of threads to use for parallel optimization.
     * @return The best solution found.
     */
    [[nodiscard]] auto optimize(int numThreads = 1) -> SolutionType;

    /**
     * @brief Retrieves the energy of the best solution found so far.
     * @return The best energy.
     */
    [[nodiscard]] auto getBestEnergy() -> double;

    /**
     * @brief Sets the initial temperature for the annealing process.
     * @param temperature The initial temperature.
     * @throws std::invalid_argument If temperature is not positive.
     */
    void setInitialTemperature(double temperature);

    /**
     * @brief Sets the cooling rate for the annealing process.
     * @param rate The cooling rate.
     * @throws std::invalid_argument If rate is not between 0 and 1.
     */
    void setCoolingRate(double rate);
};

// Example TSP (Traveling Salesman Problem) implementation
class TSP {
private:
    std::vector<std::pair<double, double>> cities_;

public:
    /**
     * @brief Constructs a TSP problem instance with a given set of cities.
     * @param cities A vector of (x, y) coordinates for each city.
     */
    explicit TSP(const std::vector<std::pair<double, double>>& cities);

    /**
     * @brief Calculates the total distance (energy) of a given TSP solution.
     * @param solution A permutation of city indices representing the tour.
     * @return The total distance of the tour.
     */
    [[nodiscard]] auto energy(const std::vector<int>& solution) const -> double;

    /**
     * @brief Generates a neighboring solution by swapping two random cities.
     * @param solution The current TSP solution.
     * @return A new neighboring TSP solution.
     */
    [[nodiscard]] static auto neighbor(const std::vector<int>& solution)
        -> std::vector<int>;

    /**
     * @brief Generates a random initial TSP solution (a shuffled tour).
     * @return A random TSP solution.
     */
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
SimulatedAnnealing<ProblemType, SolutionType>::SimulatedAnnealing(SimulatedAnnealing&& other) noexcept
    : problem_instance_(other.problem_instance_),
      cooling_schedule_(std::move(other.cooling_schedule_)),
      max_iterations_(other.max_iterations_),
      initial_temperature_(other.initial_temperature_),
      cooling_strategy_(other.cooling_strategy_),
      progress_callback_(std::move(other.progress_callback_)),
      stop_condition_(std::move(other.stop_condition_)),
      should_stop_(other.should_stop_.load()),
      best_solution_(std::move(other.best_solution_)),
      best_energy_(other.best_energy_),
      cooling_rate_(other.cooling_rate_),
      restart_interval_(other.restart_interval_),
      current_restart_(other.current_restart_),
      total_restarts_(other.total_restarts_.load()),
      total_steps_(other.total_steps_.load()),
      accepted_steps_(other.accepted_steps_.load()),
      rejected_steps_(other.rejected_steps_.load()),
      start_time_(other.start_time_),
      energy_history_(std::move(other.energy_history_)) {
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
SimulatedAnnealing<ProblemType, SolutionType>& SimulatedAnnealing<ProblemType, SolutionType>::operator=(SimulatedAnnealing&& other) noexcept {
    if (this != &other) {
        problem_instance_ = other.problem_instance_;
        cooling_schedule_ = std::move(other.cooling_schedule_);
        max_iterations_ = other.max_iterations_;
        initial_temperature_ = other.initial_temperature_;
        cooling_strategy_ = other.cooling_strategy_;
        progress_callback_ = std::move(other.progress_callback_);
        stop_condition_ = std::move(other.stop_condition_);
        should_stop_.store(other.should_stop_.load());
        best_solution_ = std::move(other.best_solution_);
        best_energy_ = other.best_energy_;
        cooling_rate_ = other.cooling_rate_;
        restart_interval_ = other.restart_interval_;
        current_restart_ = other.current_restart_;
        total_restarts_.store(other.total_restarts_.load());
        total_steps_.store(other.total_steps_.load());
        accepted_steps_.store(other.accepted_steps_.load());
        rejected_steps_.store(other.rejected_steps_.load());
        start_time_ = other.start_time_;
        energy_history_ = std::move(other.energy_history_);
    }
    return *this;
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
            cooling_schedule_ = [initial_temp = initial_temperature_,
                                cooling_rate = cooling_rate_](int iteration) {
                return initial_temp * std::pow(cooling_rate, iteration);
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
void SimulatedAnnealing<ProblemType, SolutionType>::optimizeThread(
    unsigned int seed) {
    try {
        std::mt19937 generator(seed);
        std::uniform_real_distribution<double> distribution(0.0, 1.0);

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
            spdlog::info("Iteration {}: cooling_rate_={}, initial_temperature_={}, temperature={}",
                        iteration, cooling_rate_, initial_temperature_, temperature);
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
    spdlog::info("Starting optimization with {} threads.", numThreads);
    if (numThreads < 1) {
        spdlog::warn("Invalid number of threads ({}). Defaulting to 1.",
                     numThreads);
        numThreads = 1;
    }

    std::vector<std::jthread> threads;
    threads.reserve(numThreads);

    try {
        std::random_device rd;  // Use a single random_device for seeding
        for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            // Generate a unique seed for each thread
            unsigned int seed =
                rd() ^ (static_cast<unsigned int>(
                            std::chrono::high_resolution_clock::now()
                                .time_since_epoch()
                                .count()) +
                        threadIndex);
            // Use explicit capture to avoid potential issues with 'this' pointer
            threads.emplace_back([this, seed]() {
                try {
                    optimizeThread(seed);
                } catch (const std::exception& e) {
                    spdlog::error("Exception in thread: {}", e.what());
                } catch (...) {
                    spdlog::error("Unknown exception in thread");
                }
            });
            spdlog::info("Launched optimization thread {}.", threadIndex + 1);
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // Reset the stop flag for potential future use
        should_stop_.store(false);

    } catch (const std::exception& e) {
        spdlog::error("Exception in optimize: {}", e.what());

        // Signal all threads to stop and ensure proper cleanup
        should_stop_.store(true);

        // Ensure all threads are properly joined even in case of exception
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // Reset the stop flag
        should_stop_.store(false);
        throw;
    }

    spdlog::info("Optimization completed with best energy: {}", best_energy_);

    // Return a copy of the best solution with proper locking
    std::lock_guard lock(best_mutex_);
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
        // Use atom::utils::Random for random number generation
        atom::utils::Random<std::mt19937, std::uniform_int_distribution<int>>
            rand_gen(0, static_cast<int>(solution.size()) - 1);

        int index1 = rand_gen();
        int index2 = rand_gen();
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
        // Use atom::utils::Random for random number generation
        std::random_device rd;
        std::mt19937 generator(rd());
        std::ranges::shuffle(solution, generator);
        spdlog::info("Generated random solution.");
    } catch (const std::exception& e) {
        spdlog::error("Exception in TSP::randomSolution: {}", e.what());
        throw;
    }
    return solution;
}

#endif  // ATOM_ALGORITHM_ANNEALING_HPP
