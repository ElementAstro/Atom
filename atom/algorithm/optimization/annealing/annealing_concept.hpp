#ifndef ATOM_ALGORITHM_OPTIMIZATION_ANNEALING_CONCEPT_HPP
#define ATOM_ALGORITHM_OPTIMIZATION_ANNEALING_CONCEPT_HPP

#include <concepts>

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

#endif  // ATOM_ALGORITHM_OPTIMIZATION_ANNEALING_CONCEPT_HPP
