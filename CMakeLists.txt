cmake_minimum_required(VERSION 3.0)
project(Kasa)

if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE "Release")
endif()


set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS "-Wall -Wextra -O2 -std=c++17")

add_executable(kasa kasa.cc)
