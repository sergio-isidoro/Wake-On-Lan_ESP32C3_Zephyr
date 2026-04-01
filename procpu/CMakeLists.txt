cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(wol_procpu)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

target_include_directories(app PRIVATE
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/../shared
)
