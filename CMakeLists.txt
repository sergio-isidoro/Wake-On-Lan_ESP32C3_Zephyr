# SPDX-License-Identifier: Apache-2.0

# Define the minimum CMake version required by the Zephyr SDK
cmake_minimum_required(VERSION 3.20.0)

set(ESP_FLASH_BEFORE default_reset)
set(ESP_FLASH_AFTER hard_reset)

# Locate and load the Zephyr build system package
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Define the project name
project(Wake-On-Lan_ESP32C3_Zephyr)

# Source File Discovery: Automatically gathers all '.c' files in 'src'.
# This allows for a modular project structure without manual file listing.
FILE(GLOB app_sources src/*.c)

# Application Target: Links the discovered source files.
target_sources(app PRIVATE ${app_sources})

# Include Directory Mapping: Tells the compiler where to look for .h files.
# This enables the use of #include "filename.h" for files inside 'inc'.
target_include_directories(app PRIVATE inc)