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

// 优化AnnealingProblem概念，使用更精确的约束
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
        {
            problemInstance.validate(solutionInstance)
        } -> std::same_as<bool>;  // 添加验证方法
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

// 在 optimize 方法中添加更完善的异常处理
template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
auto SimulatedAnnealing<ProblemType, SolutionType>::optimize(int numThreads)
    -> SolutionType {
    try {
        LOG_F(INFO, "Starting optimization with {} threads.", numThreads);
        if (numThreads < 1) {
            LOG_F(WARNING, "Invalid number of threads ({}). Defaulting to 1.",
                  numThreads);
            numThreads = 1;
        }

        std::vector<std::jthread> threads;  // 使用C++20的std::jthread
        threads.reserve(numThreads);

        for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            threads.emplace_back([this](const std::stop_token& stopToken) {
                optimizeThread(stopToken);
            });
            LOG_F(INFO, "Launched optimization thread {}.", threadIndex + 1);
        }

        // std::jthread析构时会自动join，无需手动join
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in optimize: {}", e.what());
        throw;  // 重新抛出异常以便上层处理
    }

    LOG_F(INFO, "Optimization completed with best energy: {}", best_energy_);
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
        throw std::invalid_argument("Initial temperature must be positive");
    }
    initial_temperature_ = temperature;
    LOG_F(INFO, "Initial temperature set to: {}", temperature);
}

template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
void SimulatedAnnealing<ProblemType, SolutionType>::setCoolingRate(
    double rate) {
    if (rate <= 0 || rate >= 1) {
        throw std::invalid_argument("Cooling rate must be between 0 and 1");
    }
    cooling_rate_ = rate;
    LOG_F(INFO, "Cooling rate set to: {}", rate);
}

// TSP class implementation
inline TSP::TSP(const std::vector<std::pair<double, double>>& cities)
    : cities_(cities) {
    LOG_F(INFO, "TSP instance created with %zu cities.", cities_.size());
}

// 优化TSP::energy方法中的SIMD代码
inline auto TSP::energy(const std::vector<int>& solution) const -> double {
    double totalDistance = 0.0;
    size_t numCities = solution.size();

#ifdef USE_SIMD
#ifdef __AVX2__
    // AVX2 实现
    __m256d totalDistanceVec = _mm256_setzero_pd();
    // ... AVX2 代码 ...

#elif defined(__ARM_NEON)
    // ARM NEON 实现
    float32x4_t totalDistanceVec = vdupq_n_f32(0.0f);
    // ... ARM NEON 代码 ...

#else
// 针对其他架构的兼容性SIMD实现
#endif
#else
    // 普通实现，已优化循环结构
    for (size_t i = 0; i < numCities; ++i) {
        size_t nextCity = (i + 1) % numCities;

        auto [x1, y1] = cities_[solution[i]];
        auto [x2, y2] = cities_[solution[nextCity]];

        double deltaX = x1 - x2;
        double deltaY = y1 - y2;
        totalDistance +=
            std::hypot(deltaX, deltaY);  // 使用std::hypot代替手动计算
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
