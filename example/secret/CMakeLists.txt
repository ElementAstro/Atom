# CMakeLists.txt for Atom-Secret Examples This project is licensed under the
# terms of the GPL3 license.
#
# Project Name: Atom-Secret-Examples Description: Example programs for Atom
# Secret Management Library Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)

# Option to build all secret examples
option(ATOM_EXAMPLE_SECRET_BUILD_ALL "Build all secret examples" ON)

# Add subdirectories for modular examples
add_subdirectory(core)
add_subdirectory(crypto)
add_subdirectory(password)
add_subdirectory(otp)
add_subdirectory(storage)
add_subdirectory(manager)
add_subdirectory(serialization)
