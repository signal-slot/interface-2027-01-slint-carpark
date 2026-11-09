cmake_minimum_required(VERSION 3.20.0)

# Slint の場所. west update が置く <workspace>/slint を ../../slint で指す.
set(SLINT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../slint" CACHE PATH "Slint checkout")
include(${SLINT_DIR}/demos/zephyr-common/slint-zephyr-app.cmake)

slint_zephyr_conf()
find_package(Zephyr)
project(carpark LANGUAGES CXX)
slint_zephyr_app(../ui/carpark.slint main.cpp)
