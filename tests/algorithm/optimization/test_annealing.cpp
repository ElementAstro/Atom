#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <vector>
#include "atom/algorithm/annealing.hpp"

using namespace testing;
using namespace std::chrono_literals;

class TestProblem {
public:
    explicit TestProblem(double target = 42.0) : target_(target) {}
    double energy(double x) const { return (x - target_) * (x - target_); }
    double neighbor(double x) const {
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        return x + dist(gen_);
    }
    double randomSolution() const {
        std::uniform_real_distribution<double> dist(-100.0, 100.0);
        return dist(gen_);
    }
    bool validate(double) const { return true; }

private:
    double target_;
    mutable std::mt19937 gen_{std::random_device{}()};
};

class MockProblem {
public:
    MOCK_METHOD(double, energy, (double), (const));
    MOCK_METHOD(double, neighbor, (double), (const));
    MOCK_METHOD(double, randomSolution, (), (const));
    MOCK_METHOD(bool, validate, (double), (const));
};

class TSPTest : public ::testing::Test {
protected:
    void SetUp() override {
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j) {
                cities_.emplace_back(i, j);
            }
        }
        tsp_ = std::make_unique<TSP>(cities_);
    }
    std::vector<std::pair<double, double>> cities_;
    std::unique_ptr<TSP> tsp_;
};

class SimulatedAnnealingTest : public ::testing::Test {
protected:
    void SetUp() override {
        problem_ = std::make_unique<TestProblem>();
        annealing_ = std::make_shared<SimulatedAnnealing<TestProblem, double>>(
            typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
                .setMaxIterations(100)
                .setInitialTemperature(100.0)
                .setCoolingStrategy(AnnealingStrategy::EXPONENTIAL)
                .build());
    }
    std::unique_ptr<TestProblem> problem_;
    std::shared_ptr<SimulatedAnnealing<TestProblem, double>> annealing_;
};

TEST(AnnealingConceptTest, TestProblemSatisfiesConcept) {
    static_assert(AnnealingProblem<TestProblem, double>,
                  "TestProblem doesn't meet AnnealingProblem requirements");
}

TEST_F(SimulatedAnnealingTest, BuilderPattern) {
    auto sa =
        typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
            .setMaxIterations(500)
            .setInitialTemperature(1000.0)
            .setCoolingRate(0.9)
            .setCoolingStrategy(AnnealingStrategy::LINEAR)
            .setRestartInterval(50)
            .build();
    EXPECT_NO_THROW(sa.optimize());
}

TEST_F(SimulatedAnnealingTest, CoolingSchedules) {
    std::vector<AnnealingStrategy> strategies = {
        AnnealingStrategy::LINEAR,      AnnealingStrategy::EXPONENTIAL,
        AnnealingStrategy::LOGARITHMIC, AnnealingStrategy::GEOMETRIC,
        AnnealingStrategy::QUADRATIC,   AnnealingStrategy::HYPERBOLIC,
        AnnealingStrategy::ADAPTIVE};
    for (const auto& strategy : strategies) {
        annealing_->setCoolingSchedule(strategy);
        EXPECT_NO_THROW(annealing_->optimize());
    }
}

TEST_F(SimulatedAnnealingTest, ConvergesToOptimalSolution) {
    double target_value = 42.0;
    auto problem = std::make_unique<TestProblem>(target_value);
    auto sa =
        typename SimulatedAnnealing<TestProblem, double>::Builder(*problem)
            .setMaxIterations(1000)
            .setInitialTemperature(100.0)
            .setCoolingRate(0.95)
            .build();
    double solution = sa.optimize();
    EXPECT_NEAR(solution, target_value, 0.1);
}

TEST_F(SimulatedAnnealingTest, ProgressCallback) {
    int callback_count = 0;
    annealing_->setProgressCallback(
        [&callback_count](int, double, double) { callback_count++; });
    annealing_->optimize();
    EXPECT_GT(callback_count, 0);
}

TEST_F(SimulatedAnnealingTest, StopCondition) {
    const int early_stop = 50;
    int stop_iteration = -1;
    annealing_->setStopCondition(
        [&stop_iteration](int iteration, double, double) {
            if (iteration >= early_stop) {
                stop_iteration = iteration;
                return true;
            }
            return false;
        });
    annealing_->optimize();
    EXPECT_EQ(stop_iteration, early_stop);
}

TEST_F(SimulatedAnnealingTest, ParallelOptimization) {
    std::vector<int> thread_counts = {1, 2, 4};
    for (int threads : thread_counts) {
        auto sa =
            typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
                .setMaxIterations(200)
                .build();
        double solution = sa.optimize(threads);
        EXPECT_NEAR(solution, 42.0, 1.0);
    }
}

TEST_F(SimulatedAnnealingTest, ExceptionHandling) {
    EXPECT_THROW(annealing_->setInitialTemperature(-10.0),
                 std::invalid_argument);
    EXPECT_THROW(annealing_->setCoolingRate(1.5), std::invalid_argument);
    EXPECT_THROW(annealing_->setCoolingRate(0.0), std::invalid_argument);
}

TEST_F(TSPTest, EnergyCalculation) {
    std::vector<int> path(25);
    std::iota(path.begin(), path.end(), 0);
    double energy = tsp_->energy(path);
    EXPECT_NEAR(energy, 24.0, 0.001);
}

TEST_F(TSPTest, NeighborGeneration) {
    std::vector<int> path(25);
    std::iota(path.begin(), path.end(), 0);
    std::vector<int> neighbor = tsp_->neighbor(path);
    EXPECT_EQ(neighbor.size(), path.size());
    int differences = 0;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] != neighbor[i]) {
            differences++;
        }
    }
    EXPECT_EQ(differences, 2);
    std::vector<int> sorted_path = path;
    std::vector<int> sorted_neighbor = neighbor;
    std::sort(sorted_path.begin(), sorted_path.end());
    std::sort(sorted_neighbor.begin(), sorted_neighbor.end());
    EXPECT_EQ(sorted_path, sorted_neighbor);
}

TEST_F(TSPTest, RandomSolutionGeneration) {
    std::vector<int> solution = tsp_->randomSolution();
    EXPECT_EQ(solution.size(), cities_.size());
    std::vector<int> expected(25);
    std::iota(expected.begin(), expected.end(), 0);
    std::vector<int> sorted_solution = solution;
    std::sort(sorted_solution.begin(), sorted_solution.end());
    EXPECT_EQ(sorted_solution, expected);
}

/*
TEST_F(TSPTest, TSPOptimization) {
    auto sa = typename SimulatedAnnealing<TSP, std::vector<int>>::Builder(*tsp_)
                  .setMaxIterations(1000)
                  .setInitialTemperature(1000.0)
                  .setCoolingRate(0.98)
                  .build();
    auto initial_solution = tsp_->randomSolution();
    double initial_energy = tsp_->energy(initial_solution);
    auto solution = sa.optimize();
    double final_energy = tsp_->energy(solution);
    EXPECT_LT(final_energy, initial_energy);
}
*/

TEST_F(SimulatedAnnealingTest, PerformanceMeasurement) {
    auto start_time = std::chrono::high_resolution_clock::now();
    annealing_->optimize(4);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time)
                        .count();
    spdlog::info("Optimization completed in {} ms", duration);
}

TEST(SimulatedAnnealingMockTest, VerifyCallPattern) {
    using ::testing::_;
    using ::testing::AtLeast;
    using ::testing::Return;
    MockProblem mock;
    ON_CALL(mock, validate(_)).WillByDefault(Return(true));
    ON_CALL(mock, randomSolution()).WillByDefault(Return(0.0));
    ON_CALL(mock, energy(_)).WillByDefault(Return(100.0));
    ON_CALL(mock, neighbor(_)).WillByDefault(Return(0.0));
    EXPECT_CALL(mock, randomSolution()).Times(AtLeast(1));
    EXPECT_CALL(mock, energy(_)).Times(AtLeast(1));
    auto sa = typename SimulatedAnnealing<MockProblem, double>::Builder(mock)
                  .setMaxIterations(10)
                  .build();
    sa.optimize();
}

TEST(IntegrationTest, OptimizeRealProblem) {
    TestProblem problem(-273.15);
    auto sa = typename SimulatedAnnealing<TestProblem, double>::Builder(problem)
                  .setMaxIterations(2000)
                  .setInitialTemperature(500.0)
                  .setCoolingRate(0.997)
                  .setRestartInterval(200)
                  .setCoolingStrategy(AnnealingStrategy::ADAPTIVE)
                  .build();
    std::vector<double> energy_history;
    sa.setProgressCallback(
        [&energy_history](int iteration, double energy, double) {
            if (iteration % 100 == 0) {
                energy_history.push_back(energy);
            }
        });
    double solution = sa.optimize(2);
    spdlog::info("Convergence history (every 100 iterations):");
    for (size_t i = 0; i < energy_history.size(); ++i) {
        spdlog::info("Iteration {}: {}", i * 100, energy_history[i]);
    }
    spdlog::info("Final solution: {}, target: -273.15", solution);
    spdlog::info("Final energy: {}", sa.getBestEnergy());
    EXPECT_NEAR(solution, -273.15, 1.0);
}

TEST_F(SimulatedAnnealingTest, AdaptiveTemperature) {
    auto sa =
        typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
            .setCoolingStrategy(AnnealingStrategy::ADAPTIVE)
            .setMaxIterations(500)
            .build();
    double solution = sa.optimize();
    EXPECT_NEAR(solution, 42.0, 1.0);
}

TEST_F(SimulatedAnnealingTest, RestartMechanism) {
    auto sa =
        typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
            .setRestartInterval(20)
            .setMaxIterations(500)
            .build();
    double solution = sa.optimize();
    EXPECT_NEAR(solution, 42.0, 1.0);
}

class MultiModalProblem {
public:
    double energy(double x) const {
        return -std::sin(x) * std::exp(-0.01 * x * x);
    }
    double neighbor(double x) const {
        std::uniform_real_distribution<double> dist(-0.5, 0.5);
        return x + dist(gen_);
    }
    double randomSolution() const {
        std::uniform_real_distribution<double> dist(-10.0, 10.0);
        return dist(gen_);
    }
    bool validate(double) const { return true; }

private:
    mutable std::mt19937 gen_{std::random_device{}()};
};

TEST(MultiModalTest, EscapeLocalMinima) {
    MultiModalProblem problem;
    auto sa =
        typename SimulatedAnnealing<MultiModalProblem, double>::Builder(problem)
            .setMaxIterations(1000)
            .setInitialTemperature(10.0)
            .setCoolingRate(0.998)
            .build();
    double solution = sa.optimize();
    double energy = problem.energy(solution);
    double global_min_energy = -1.0;
    EXPECT_NEAR(energy, global_min_energy, 0.1);
}
