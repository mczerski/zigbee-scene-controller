# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Find nRF Connect SDK
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Define project
project(zigbee_scene_controller)

# Add source files
target_sources(app PRIVATE
  src/main.c
)

# Include directories
target_include_directories(app PRIVATE
  include
)

# Add custom boards to project if available
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/boards)
  zephyr_include_directories(boards)
endif()

# Linker options
zephyr_linker_sources(SECTIONS linkerfragments.ld)
