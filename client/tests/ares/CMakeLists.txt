# AresVPN Client - the executable tests of the session's rules (audit 2026-11-14 H-17).
#
# A project of its own, NOT a subdirectory of the client build: it compiles one QtCore-only unit
# (client/core/controllers/aresSessionLogic.cpp) and needs no conan, no OpenSSL, no QML and no
# branding preload, so it runs in seconds on any machine with a desktop Qt:
#
#   cmake -S client/tests/ares -B build-ares-tests -DCMAKE_PREFIX_PATH=<Qt>/msvc2022_64
#   cmake --build build-ares-tests --config Release
#   ctest --test-dir build-ares-tests -C Release --output-on-failure
cmake_minimum_required(VERSION 3.25.0 FATAL_ERROR)
project(AresSessionLogicTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

set(ARES_CLIENT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
add_executable(tst_aresSessionLogic
    tst_aresSessionLogic.cpp
    ${ARES_CLIENT_DIR}/core/controllers/aresSessionLogic.cpp
)
target_include_directories(tst_aresSessionLogic PRIVATE ${ARES_CLIENT_DIR})
target_link_libraries(tst_aresSessionLogic PRIVATE Qt6::Core Qt6::Test)

enable_testing()
add_test(NAME tst_aresSessionLogic COMMAND tst_aresSessionLogic)
