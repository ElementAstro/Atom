#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <vector>

#include "atom/algorithm/annealing.hpp"
#include "atom/log/loguru.hpp"

using namespace testing;
using namespace std::chrono_literals;

// Simple test problem that satisfies the AnnealingProblem concept
class TestProblem {
public:
    // Simple 1D optimization problem: find x where f(x) = (x - target)^2 is
    // minimized
    explicit TestProblem(double target = 42.0) : target_(target) {}

    // Energy function: quadratic distance from target
    double energy(double x) const { return (x - target_) * (x - target_); }

    // Generate a neighbor by adding some random noise
    double neighbor(double x) const {
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        return x + dist(gen_);
    }

    // Random solution between -100 and +100
    double randomSolution() const {
        std::uniform_real_distribution<double> dist(-100.0, 100.0);
        return dist(gen_);
    }

    // All solutions are valid
    bool validate(double) const { return true; }

private:
    double target_;
    mutable std::mt19937 gen_{std::random_device{}()};
};

// Mock problem for testing expected behavior
class MockProblem {
public:
    MOCK_METHOD(double, energy, (double), (const));
    MOCK_METHOD(double, neighbor, (double), (const));
    MOCK_METHOD(double, randomSolution, (), (const));
    MOCK_METHOD(bool, validate, (double), (const));
};

// Helper class for testing TSP implementation
class TSPTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple grid of cities
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

// Test fixture for the SimulatedAnnealing class
class SimulatedAnnealingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置日志级别为警告，减少测试期间的日志输出
        // 如果loguru未定义，可以注释掉这些行
        // loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;

        problem_ = std::make_unique<TestProblem>();

        // Create annealing instance using the builder
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

// Test the AnnealingProblem concept
TEST(AnnealingConceptTest, TestProblemSatisfiesConcept) {
    // If this compiles, TestProblem satisfies the AnnealingProblem concept
    static_assert(AnnealingProblem<TestProblem, double>,
                  "TestProblem doesn't meet AnnealingProblem requirements");
}

// Test the builder pattern
TEST_F(SimulatedAnnealingTest, BuilderPattern) {
    auto sa =
        typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
            .setMaxIterations(500)
            .setInitialTemperature(1000.0)
            .setCoolingRate(0.9)
            .setCoolingStrategy(AnnealingStrategy::LINEAR)
            .setRestartInterval(50)
            .build();

    // We can't directly test internal state, but we can check that it runs
    // without errors
    EXPECT_NO_THROW(sa.optimize());
}

// Test different cooling schedules
TEST_F(SimulatedAnnealingTest, CoolingSchedules) {
    // Test all cooling strategies
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

// Test that optimization finds the correct solution for TestProblem
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

    // The solution should be close to the target value
    EXPECT_NEAR(solution, target_value, 0.1);
}

// Test the progress callback
TEST_F(SimulatedAnnealingTest, ProgressCallback) {
    int callback_count = 0;
    annealing_->setProgressCallback(
        [&callback_count](int /*iteration*/, double /*energy*/,
                          double /*solution*/) { callback_count++; });

    annealing_->optimize();

    // The callback should have been called for each iteration
    EXPECT_GT(callback_count, 0);
}

// Test the stop condition
TEST_F(SimulatedAnnealingTest, StopCondition) {
    const int early_stop = 50;

    int stop_iteration = -1;
    annealing_->setStopCondition([&stop_iteration](int iteration,
                                                   double /*energy*/,
                                                   double /*solution*/) {
        if (iteration >= early_stop) {
            stop_iteration = iteration;
            return true;
        }
        return false;
    });

    annealing_->optimize();

    // The optimization should have stopped at the early_stop iteration
    EXPECT_EQ(stop_iteration, early_stop);
}

// Test parallel optimization with multiple threads
TEST_F(SimulatedAnnealingTest, ParallelOptimization) {
    // Test with different numbers of threads
    std::vector<int> thread_counts = {1, 2, 4};

    for (int threads : thread_counts) {
        auto sa =
            typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
                .setMaxIterations(200)
                .build();

        double solution = sa.optimize(threads);

        // The solution should be close to 42 regardless of thread count
        EXPECT_NEAR(solution, 42.0, 1.0);
    }
}

// Test exception handling for invalid parameters
TEST_F(SimulatedAnnealingTest, ExceptionHandling) {
    // Test with invalid temperature
    EXPECT_THROW(annealing_->setInitialTemperature(-10.0),
                 std::invalid_argument);

    // Test with invalid cooling rate
    EXPECT_THROW(annealing_->setCoolingRate(1.5), std::invalid_argument);
    EXPECT_THROW(annealing_->setCoolingRate(0.0), std::invalid_argument);
}

// Test TSP implementation
TEST_F(TSPTest, EnergyCalculation) {
    // Path visiting each city in order (0,0), (0,1), ... (4,4)
    std::vector<int> path(25);
    std::iota(path.begin(), path.end(), 0);

    double energy = tsp_->energy(path);

    // For a 5x5 grid, visiting row by row, the total distance should be:
    // 4 horizontal segments of length 1 per row = 20
    // 4 vertical segments of length 1 to move to next row = 4
    // Total = 24
    EXPECT_NEAR(energy, 24.0, 0.001);
}

TEST_F(TSPTest, NeighborGeneration) {
    // Path visiting each city in order
    std::vector<int> path(25);
    std::iota(path.begin(), path.end(), 0);

    std::vector<int> neighbor = tsp_->neighbor(path);

    // The neighbor should have the same length
    EXPECT_EQ(neighbor.size(), path.size());

    // The neighbor should differ by exactly two positions (a swap)
    int differences = 0;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] != neighbor[i]) {
            differences++;
        }
    }
    EXPECT_EQ(differences, 2);

    // The neighbor should contain the same elements
    std::vector<int> sorted_path = path;
    std::vector<int> sorted_neighbor = neighbor;
    std::sort(sorted_path.begin(), sorted_path.end());
    std::sort(sorted_neighbor.begin(), sorted_neighbor.end());
    EXPECT_EQ(sorted_path, sorted_neighbor);
}

TEST_F(TSPTest, RandomSolutionGeneration) {
    std::vector<int> solution = tsp_->randomSolution();

    // The solution should have the correct length
    EXPECT_EQ(solution.size(), cities_.size());

    // The solution should be a permutation of the city indices
    std::vector<int> expected(25);
    std::iota(expected.begin(), expected.end(), 0);

    std::vector<int> sorted_solution = solution;
    std::sort(sorted_solution.begin(), sorted_solution.end());
    EXPECT_EQ(sorted_solution, expected);
}

/*
// Test TSP optimization using SimulatedAnnealing
TEST_F(TSPTest, TSPOptimization) {
    auto sa = typename SimulatedAnnealing<TSP, std::vector<int>>::Builder(*tsp_)
                  .setMaxIterations(1000)
                  .setInitialTemperature(1000.0)
                  .setCoolingRate(0.98)
                  .build();

    // Record initial solution energy
    auto initial_solution = tsp_->randomSolution();
    double initial_energy = tsp_->energy(initial_solution);

    // Optimize
    auto solution = sa.optimize();
    double final_energy = tsp_->energy(solution);

    // The optimized solution should have lower energy
    EXPECT_LT(final_energy, initial_energy);
}
*/

// Performance test
TEST_F(SimulatedAnnealingTest, PerformanceMeasurement) {
    auto start_time = std::chrono::high_resolution_clock::now();

    annealing_->optimize(4);  // Use 4 threads for parallel performance

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time)
                        .count();

    std::cout << "Optimization completed in " << duration << " ms" << std::endl;

    // No specific assertion, just measuring performance
}

// Test with mock to verify expected calls
TEST(SimulatedAnnealingMockTest, VerifyCallPattern) {
    using ::testing::_;
    using ::testing::AtLeast;
    using ::testing::Return;

    // Setup mock
    MockProblem mock;
    ON_CALL(mock, validate(_)).WillByDefault(Return(true));
    ON_CALL(mock, randomSolution()).WillByDefault(Return(0.0));
    ON_CALL(mock, energy(_)).WillByDefault(Return(100.0));
    ON_CALL(mock, neighbor(_)).WillByDefault(Return(0.0));

    // Expectations
    EXPECT_CALL(mock, randomSolution()).Times(AtLeast(1));
    EXPECT_CALL(mock, energy(_)).Times(AtLeast(1));

    // Run a minimal optimization that should still call the key methods
    auto sa = typename SimulatedAnnealing<MockProblem, double>::Builder(mock)
                  .setMaxIterations(10)
                  .build();

    sa.optimize();
}

// Integration test with an actual optimization problem
TEST(IntegrationTest, OptimizeRealProblem) {
    // Create a challenging TestProblem
    TestProblem problem(-273.15);  // Unusual target value

    // Configure annealing with tuned parameters
    auto sa = typename SimulatedAnnealing<TestProblem, double>::Builder(problem)
                  .setMaxIterations(2000)
                  .setInitialTemperature(500.0)
                  .setCoolingRate(0.997)
                  .setRestartInterval(200)
                  .setCoolingStrategy(AnnealingStrategy::ADAPTIVE)
                  .build();

    // Track progress with a callback
    std::vector<double> energy_history;
    sa.setProgressCallback(
        [&energy_history](int iteration, double energy, double /*solution*/) {
            if (iteration % 100 == 0) {
                energy_history.push_back(energy);
            }
        });

    // Optimize with multiple threads
    double solution = sa.optimize(2);

    // Output convergence information
    std::cout << "Convergence history (every 100 iterations):" << std::endl;
    for (size_t i = 0; i < energy_history.size(); ++i) {
        std::cout << "Iteration " << i * 100 << ": " << energy_history[i]
                  << std::endl;
    }

    std::cout << "Final solution: " << solution << ", target: -273.15"
              << std::endl;
    std::cout << "Final energy: " << sa.getBestEnergy() << std::endl;

    // Solution should be reasonably close to target
    EXPECT_NEAR(solution, -273.15, 1.0);
}

// Test adaptive temperature adjustment
TEST_F(SimulatedAnnealingTest, AdaptiveTemperature) {
    auto sa =
        typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
            .setCoolingStrategy(AnnealingStrategy::ADAPTIVE)
            .setMaxIterations(500)
            .build();

    double solution = sa.optimize();

    // Check that solution is reasonable
    EXPECT_NEAR(solution, 42.0, 1.0);
}

// Test restart mechanism
TEST_F(SimulatedAnnealingTest, RestartMechanism) {
    auto sa =
        typename SimulatedAnnealing<TestProblem, double>::Builder(*problem_)
            .setRestartInterval(20)  // Frequent restarts
            .setMaxIterations(500)
            .build();

    double solution = sa.optimize();

    // Check that solution is reasonable despite restarts
    EXPECT_NEAR(solution, 42.0, 1.0);
}

// Test with a problem that has multiple local minima
class MultiModalProblem {
public:
    // Function with multiple minima: f(x) = sin(x) * exp(-0.01*x^2)
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
            .setCoolingRate(0.998)  // Slow cooling to explore more
            .build();

    double solution = sa.optimize();
    double energy = problem.energy(solution);

    // The global minimum is near x = 0
    // We're checking that the energy is close to the theoretical minimum
    double global_min_energy = -1.0;  // Approximately the value at x=0
    EXPECT_NEAR(energy, global_min_energy, 0.1);
}
