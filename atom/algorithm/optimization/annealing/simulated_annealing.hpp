#ifndef ATOM_ALGORITHM_OPTIMIZATION_SIMULATED_ANNEALING_HPP
#define ATOM_ALGORITHM_OPTIMIZATION_SIMULATED_ANNEALING_HPP

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

#include "annealing_concept.hpp"
#include "cooling_schedule.hpp"

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

    // Copy constructor
    SimulatedAnnealing(const SimulatedAnnealing& other);

    // Move constructor
    SimulatedAnnealing(SimulatedAnnealing&& other) noexcept;

    // Copy assignment operator
    SimulatedAnnealing& operator=(const SimulatedAnnealing& other);

    // Move assignment operator
    SimulatedAnnealing& operator=(SimulatedAnnealing&& other) noexcept;

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

// Copy constructor implementation
template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
SimulatedAnnealing<ProblemType, SolutionType>::SimulatedAnnealing(
    const SimulatedAnnealing& other)
    : problem_instance_(other.problem_instance_),
      cooling_schedule_(other.cooling_schedule_),
      max_iterations_(other.max_iterations_),
      initial_temperature_(other.initial_temperature_),
      cooling_strategy_(other.cooling_strategy_),
      progress_callback_(other.progress_callback_),
      stop_condition_(other.stop_condition_),
      should_stop_(other.should_stop_.load()),
      best_solution_(other.best_solution_),
      best_energy_(other.best_energy_),
      cooling_rate_(other.cooling_rate_),
      restart_interval_(other.restart_interval_),
      current_restart_(other.current_restart_),
      total_restarts_(other.total_restarts_.load()),
      total_steps_(other.total_steps_.load()),
      accepted_steps_(other.accepted_steps_.load()),
      rejected_steps_(other.rejected_steps_.load()),
      start_time_(other.start_time_),
      energy_history_(std::make_unique<std::vector<std::pair<int, double>>>(
          *other.energy_history_)) {}

// Move constructor implementation
template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
SimulatedAnnealing<ProblemType, SolutionType>::SimulatedAnnealing(
    SimulatedAnnealing&& other) noexcept
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
      energy_history_(std::move(other.energy_history_)) {}

// Copy assignment operator implementation
template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
SimulatedAnnealing<ProblemType, SolutionType>&
SimulatedAnnealing<ProblemType, SolutionType>::operator=(
    const SimulatedAnnealing& other) {
    if (this != &other) {
        problem_instance_ = other.problem_instance_;
        cooling_schedule_ = other.cooling_schedule_;
        max_iterations_ = other.max_iterations_;
        initial_temperature_ = other.initial_temperature_;
        cooling_strategy_ = other.cooling_strategy_;
        progress_callback_ = other.progress_callback_;
        stop_condition_ = other.stop_condition_;
        should_stop_ = other.should_stop_.load();
        best_solution_ = other.best_solution_;
        best_energy_ = other.best_energy_;
        cooling_rate_ = other.cooling_rate_;
        restart_interval_ = other.restart_interval_;
        current_restart_ = other.current_restart_;
        total_restarts_ = other.total_restarts_.load();
        total_steps_ = other.total_steps_.load();
        accepted_steps_ = other.accepted_steps_.load();
        rejected_steps_ = other.rejected_steps_.load();
        start_time_ = other.start_time_;
        energy_history_ = std::make_unique<std::vector<std::pair<int, double>>>(
            *other.energy_history_);
    }
    return *this;
}

// Move assignment operator implementation
template <typename ProblemType, typename SolutionType>
    requires AnnealingProblem<ProblemType, SolutionType>
SimulatedAnnealing<ProblemType, SolutionType>&
SimulatedAnnealing<ProblemType, SolutionType>::operator=(
    SimulatedAnnealing&& other) noexcept {
    if (this != &other) {
        problem_instance_ = other.problem_instance_;
        cooling_schedule_ = std::move(other.cooling_schedule_);
        max_iterations_ = other.max_iterations_;
        initial_temperature_ = other.initial_temperature_;
        cooling_strategy_ = other.cooling_strategy_;
        progress_callback_ = std::move(other.progress_callback_);
        stop_condition_ = std::move(other.stop_condition_);
        should_stop_ = other.should_stop_.load();
        best_solution_ = std::move(other.best_solution_);
        best_energy_ = other.best_energy_;
        cooling_rate_ = other.cooling_rate_;
        restart_interval_ = other.restart_interval_;
        current_restart_ = other.current_restart_;
        total_restarts_ = other.total_restarts_.load();
        total_steps_ = other.total_steps_.load();
        accepted_steps_ = other.accepted_steps_.load();
        rejected_steps_ = other.rejected_steps_.load();
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
    cooling_schedule_ = createCoolingSchedule(
        strategy, initial_temperature_, max_iterations_, cooling_rate_);
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

#endif  // ATOM_ALGORITHM_OPTIMIZATION_SIMULATED_ANNEALING_HPP
