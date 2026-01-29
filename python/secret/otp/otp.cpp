/*
 * otp.cpp
 *
 * Python bindings for OTP module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/secret/otp/hotp.hpp"
#include "atom/secret/otp/totp.hpp"

namespace py = pybind11;
using namespace atom::secret;

void bind_otp(py::module& m) {
    // TotpConfig struct
    py::class_<TotpConfig>(m, "TotpConfig",
                           R"pbdoc(
        Configuration for TOTP generation.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("secret", &TotpConfig::secret)
        .def_readwrite("digits", &TotpConfig::digits)
        .def_readwrite("period", &TotpConfig::period)
        .def_readwrite("algorithm", &TotpConfig::algorithm)
        .def_readwrite("issuer", &TotpConfig::issuer)
        .def_readwrite("account_name", &TotpConfig::accountName);

    // Totp class
    py::class_<Totp>(m, "Totp",
                     R"pbdoc(
        Time-based One-Time Password (TOTP) utilities.
        )pbdoc")
        .def_static("generate",
                    py::overload_cast<const TotpConfig&>(&Totp::generate),
                    py::arg("config"), "Generates a TOTP code")
        .def_static(
            "generate",
            py::overload_cast<const TotpConfig&, uint64_t>(&Totp::generate),
            py::arg("config"), py::arg("time"),
            "Generates a TOTP code for a specific time")
        .def_static("verify", &Totp::verify, py::arg("config"), py::arg("code"),
                    py::arg("window") = 0, "Verifies a TOTP code")
        .def_static("generate_secret", &Totp::generateSecret,
                    py::arg("length") = 20, "Generates a random secret")
        .def_static("generate_uri", &Totp::generateUri, py::arg("config"),
                    "Generates an OTPAuth URI")
        .def_static("parse_uri", &Totp::parseUri, py::arg("uri"),
                    "Parses an OTPAuth URI");

    // HotpConfig struct
    py::class_<HotpConfig>(m, "HotpConfig",
                           R"pbdoc(
        Configuration for HOTP generation.
        )pbdoc")
        .def(py::init<>())
        .def(py::init<const std::string&, uint64_t, int>(), py::arg("secret"),
             py::arg("counter") = 0, py::arg("digits") = 6)
        .def_readwrite("secret", &HotpConfig::secret)
        .def_readwrite("counter", &HotpConfig::counter)
        .def_readwrite("digits", &HotpConfig::digits);

    // Hotp class
    py::class_<Hotp>(m, "Hotp",
                     R"pbdoc(
        HMAC-based One-Time Password (HOTP) utilities.
        )pbdoc")
        .def_static("generate", &Hotp::generate, py::arg("config"),
                    "Generates an HOTP code")
        .def_static("verify", &Hotp::verify, py::arg("config"), py::arg("code"),
                    "Verifies an HOTP code")
        .def_static("verify_with_resync", &Hotp::verifyWithResync,
                    py::arg("config"), py::arg("code"),
                    py::arg("lookahead") = 10,
                    "Verifies an HOTP code with resync")
        .def_static("generate_and_increment", &Hotp::generateAndIncrement,
                    py::arg("config"),
                    "Generates an HOTP code and increments the counter");

    // Base32 class
    py::class_<Base32>(m, "Base32",
                       R"pbdoc(
        Base32 encoding/decoding utilities.
        )pbdoc")
        .def_static("encode", &Base32::encode, py::arg("data"),
                    "Encodes data to Base32")
        .def_static("decode", &Base32::decode, py::arg("encoded"),
                    "Decodes Base32 data");
}
