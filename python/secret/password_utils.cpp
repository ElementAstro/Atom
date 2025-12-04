#include "atom/secret/password_utils.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_password_utils(py::module& m) {
    // PasswordGenerator::CharacterSets
    auto character_sets =
        py::class_<atom::secret::PasswordGenerator::CharacterSets>(
            m, "CharacterSets",
            R"pbdoc(
        Character sets for password generation.

        Provides predefined character sets for different character types.
        )pbdoc");

    character_sets.def_readonly_static(
        "LOWERCASE", &atom::secret::PasswordGenerator::CharacterSets::LOWERCASE,
        "Lowercase letters (a-z)");
    character_sets.def_readonly_static(
        "UPPERCASE", &atom::secret::PasswordGenerator::CharacterSets::UPPERCASE,
        "Uppercase letters (A-Z)");
    character_sets.def_readonly_static(
        "DIGITS", &atom::secret::PasswordGenerator::CharacterSets::DIGITS,
        "Digits (0-9)");
    character_sets.def_readonly_static(
        "SPECIAL", &atom::secret::PasswordGenerator::CharacterSets::SPECIAL,
        "Special characters");
    character_sets.def_readonly_static(
        "AMBIGUOUS", &atom::secret::PasswordGenerator::CharacterSets::AMBIGUOUS,
        "Ambiguous characters (0, O, 1, l, I)");

    // PasswordGenerator::GenerationOptions
    py::class_<atom::secret::PasswordGenerator::GenerationOptions>(
        m, "GenerationOptions",
        R"pbdoc(
        Options for password generation.

        Configures various aspects of password generation including length,
        character types to include, and minimum requirements.

        Attributes:
            length (int): Password length (default: 16)
            include_lowercase (bool): Include lowercase letters (default: True)
            include_uppercase (bool): Include uppercase letters (default: True)
            include_digits (bool): Include digits (default: True)
            include_special (bool): Include special characters (default: True)
            exclude_ambiguous (bool): Exclude ambiguous characters (default: False)
            custom_characters (str): Custom character set
            min_lowercase (int): Minimum lowercase letters (default: 1)
            min_uppercase (int): Minimum uppercase letters (default: 1)
            min_digits (int): Minimum digits (default: 1)
            min_special (int): Minimum special characters (default: 1)
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_readwrite(
            "length",
            &atom::secret::PasswordGenerator::GenerationOptions::length,
            "Password length")
        .def_readwrite("include_lowercase",
                       &atom::secret::PasswordGenerator::GenerationOptions::
                           includeLowercase,
                       "Include lowercase letters")
        .def_readwrite("include_uppercase",
                       &atom::secret::PasswordGenerator::GenerationOptions::
                           includeUppercase,
                       "Include uppercase letters")
        .def_readwrite(
            "include_digits",
            &atom::secret::PasswordGenerator::GenerationOptions::includeDigits,
            "Include digits")
        .def_readwrite(
            "include_special",
            &atom::secret::PasswordGenerator::GenerationOptions::includeSpecial,
            "Include special characters")
        .def_readwrite("exclude_ambiguous",
                       &atom::secret::PasswordGenerator::GenerationOptions::
                           excludeAmbiguous,
                       "Exclude ambiguous characters")
        .def_readwrite("custom_characters",
                       &atom::secret::PasswordGenerator::GenerationOptions::
                           customCharacters,
                       "Custom character set")
        .def_readwrite(
            "min_lowercase",
            &atom::secret::PasswordGenerator::GenerationOptions::minLowercase,
            "Minimum lowercase letters")
        .def_readwrite(
            "min_uppercase",
            &atom::secret::PasswordGenerator::GenerationOptions::minUppercase,
            "Minimum uppercase letters")
        .def_readwrite(
            "min_digits",
            &atom::secret::PasswordGenerator::GenerationOptions::minDigits,
            "Minimum digits")
        .def_readwrite(
            "min_special",
            &atom::secret::PasswordGenerator::GenerationOptions::minSpecial,
            "Minimum special characters");

    // PasswordGenerator class
    py::class_<atom::secret::PasswordGenerator>(m, "PasswordGenerator",
                                                R"pbdoc(
        Password generation utilities.

        Provides static methods for generating secure passwords with various
        options and requirements.

        Example:
            >>> # Generate a default password
            >>> result = PasswordGenerator.generate_password()
            >>> if result.is_success():
            ...     print(f"Password: {result.value()}")
            >>>
            >>> # Generate with custom options
            >>> options = GenerationOptions()
            >>> options.length = 20
            >>> options.include_special = False
            >>> result = PasswordGenerator.generate_password(options)
            >>>
            >>> # Generate a memorable password
            >>> result = PasswordGenerator.generate_memorable_password(4, "-", True)
        )pbdoc")
        .def_static("generate_password",
                    py::overload_cast<>(
                        &atom::secret::PasswordGenerator::generatePassword),
                    R"pbdoc(
            Generates a secure password with default options.

            Returns:
                Result[str]: Generated password or error message.
            )pbdoc")
        .def_static(
            "generate_password",
            py::overload_cast<
                const atom::secret::PasswordGenerator::GenerationOptions&>(
                &atom::secret::PasswordGenerator::generatePassword),
            py::arg("options"),
            R"pbdoc(
            Generates a secure password with the specified options.

            Args:
                options: Password generation options.

            Returns:
                Result[str]: Generated password or error message.
            )pbdoc")
        .def_static(
            "generate_password",
            py::overload_cast<const atom::secret::PasswordManagerSettings&,
                              int>(
                &atom::secret::PasswordGenerator::generatePassword),
            py::arg("settings"), py::arg("length") = 0,
            R"pbdoc(
            Generates a password based on PasswordManagerSettings.

            Args:
                settings: Password manager settings.
                length: Desired password length (overrides settings if specified).

            Returns:
                Result[str]: Generated password or error message.
            )pbdoc")
        .def_static("generate_memorable_password",
                    &atom::secret::PasswordGenerator::generateMemorablePassword,
                    py::arg("word_count") = 4, py::arg("separator") = "-",
                    py::arg("include_numbers") = true,
                    R"pbdoc(
            Generates a memorable password using word lists.

            Args:
                word_count: Number of words to use (default: 4).
                separator: Separator between words (default: "-").
                include_numbers: Whether to include numbers (default: True).

            Returns:
                Result[str]: Generated password or error message.
            )pbdoc")
        .def_static("generate_pin",
                    &atom::secret::PasswordGenerator::generatePin,
                    py::arg("length") = 6,
                    R"pbdoc(
            Generates a PIN code.

            Args:
                length: PIN length (default: 6).

            Returns:
                Result[str]: Generated PIN or error message.
            )pbdoc");

    // PasswordValidator::AnalysisResult
    py::class_<atom::secret::PasswordValidator::AnalysisResult>(
        m, "AnalysisResult",
        R"pbdoc(
        Detailed password analysis results.

        Contains comprehensive information about password strength and
        characteristics.

        Attributes:
            strength (PasswordStrength): Overall password strength
            score (int): Numerical score (0-100)
            has_lowercase (bool): Contains lowercase letters
            has_uppercase (bool): Contains uppercase letters
            has_digits (bool): Contains digits
            has_special (bool): Contains special characters
            has_repeated_chars (bool): Contains repeated characters
            has_sequential_chars (bool): Contains sequential characters
            is_common_password (bool): Is a commonly used password
            suggestions (list[str]): Improvement suggestions
            entropy (float): Password entropy in bits
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_readwrite(
            "strength",
            &atom::secret::PasswordValidator::AnalysisResult::strength,
            "Overall password strength")
        .def_readwrite("score",
                       &atom::secret::PasswordValidator::AnalysisResult::score,
                       "Numerical score (0-100)")
        .def_readwrite(
            "has_lowercase",
            &atom::secret::PasswordValidator::AnalysisResult::hasLowercase,
            "Contains lowercase letters")
        .def_readwrite(
            "has_uppercase",
            &atom::secret::PasswordValidator::AnalysisResult::hasUppercase,
            "Contains uppercase letters")
        .def_readwrite(
            "has_digits",
            &atom::secret::PasswordValidator::AnalysisResult::hasDigits,
            "Contains digits")
        .def_readwrite(
            "has_special",
            &atom::secret::PasswordValidator::AnalysisResult::hasSpecial,
            "Contains special characters")
        .def_readwrite(
            "has_repeated_chars",
            &atom::secret::PasswordValidator::AnalysisResult::hasRepeatedChars,
            "Contains repeated characters")
        .def_readwrite("has_sequential_chars",
                       &atom::secret::PasswordValidator::AnalysisResult::
                           hasSequentialChars,
                       "Contains sequential characters")
        .def_readwrite(
            "is_common_password",
            &atom::secret::PasswordValidator::AnalysisResult::isCommonPassword,
            "Is a commonly used password")
        .def_readwrite(
            "suggestions",
            &atom::secret::PasswordValidator::AnalysisResult::suggestions,
            "Improvement suggestions")
        .def_readwrite(
            "entropy",
            &atom::secret::PasswordValidator::AnalysisResult::entropy,
            "Password entropy in bits");

    // PasswordValidator class
    py::class_<atom::secret::PasswordValidator>(m, "PasswordValidator",
                                                R"pbdoc(
        Password validation and strength assessment utilities.

        Provides static methods for analyzing password strength, validating
        passwords against requirements, and estimating crack time.

        Example:
            >>> # Analyze a password
            >>> analysis = PasswordValidator.analyze_password("MyPassword123!")
            >>> print(f"Strength: {analysis.strength}")
            >>> print(f"Score: {analysis.score}/100")
            >>> print(f"Entropy: {analysis.entropy} bits")
            >>> for suggestion in analysis.suggestions:
            ...     print(f"Suggestion: {suggestion}")
            >>>
            >>> # Validate against settings
            >>> settings = PasswordManagerSettings()
            >>> result = PasswordValidator.validate_password("MyPassword123!", settings)
            >>> if result.is_success():
            ...     print("Password is valid")
        )pbdoc")
        .def_static("analyze_password",
                    &atom::secret::PasswordValidator::analyzePassword,
                    py::arg("password"),
                    R"pbdoc(
            Analyzes password strength and characteristics.

            Args:
                password: Password to analyze.

            Returns:
                AnalysisResult: Detailed analysis results.
            )pbdoc")
        .def_static("validate_password",
                    &atom::secret::PasswordValidator::validatePassword,
                    py::arg("password"), py::arg("settings"),
                    R"pbdoc(
            Validates password against settings requirements.

            Args:
                password: Password to validate.
                settings: Password manager settings.

            Returns:
                Result[bool]: True if valid or error message.
            )pbdoc")
        .def_static("calculate_entropy",
                    &atom::secret::PasswordValidator::calculateEntropy,
                    py::arg("password"),
                    R"pbdoc(
            Calculates password entropy.

            Args:
                password: Password to analyze.

            Returns:
                float: Entropy in bits.
            )pbdoc")
        .def_static("is_common_password",
                    &atom::secret::PasswordValidator::isCommonPassword,
                    py::arg("password"),
                    R"pbdoc(
            Checks if password is in common password list.

            Args:
                password: Password to check.

            Returns:
                bool: True if password is common.
            )pbdoc")
        .def_static("estimate_crack_time",
                    &atom::secret::PasswordValidator::estimateCrackTime,
                    py::arg("password"), py::arg("guesses_per_second") = 1e9,
                    R"pbdoc(
            Estimates time to crack password.

            Args:
                password: Password to analyze.
                guesses_per_second: Guesses per second (default: 1 billion).

            Returns:
                float: Estimated crack time in seconds.
            )pbdoc");

    // SecureComparison class
    py::class_<atom::secret::SecureComparison>(m, "SecureComparison",
                                               R"pbdoc(
        Secure string comparison utilities.

        Provides constant-time comparison functions to prevent timing attacks.

        Example:
            >>> # Compare two strings securely
            >>> if SecureComparison.constant_time_equals("password1", "password2"):
            ...     print("Passwords match")
        )pbdoc")
        .def_static("constant_time_equals",
                    py::overload_cast<std::string_view, std::string_view>(
                        &atom::secret::SecureComparison::constantTimeEquals),
                    py::arg("a"), py::arg("b"),
                    R"pbdoc(
            Performs constant-time string comparison.

            Args:
                a: First string.
                b: Second string.

            Returns:
                bool: True if strings are equal.
            )pbdoc");
}
