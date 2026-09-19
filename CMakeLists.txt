cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project("zigbee-scene-controller")

include(cmake/GitVersion.cmake)
git_version_info(
  VERSION FW_VERSION
  MAJOR FW_VERSION_MAJOR
  MINOR FW_VERSION_MINOR
  PATCH FW_VERSION_PATCH
  COMMIT_HASH FW_COMMIT_HASH
  IS_DIRTY FW_IS_DIRTY
)

set(FW_VERSION_FULL "${FW_VERSION}+${FW_COMMIT_HASH}")
if(FW_IS_DIRTY)
  string(APPEND FW_VERSION_FULL "-d")
endif()

string(LENGTH "${FW_VERSION_FULL}" FW_VERSION_FULL_LEN)
if(FW_VERSION_FULL_LEN GREATER 16)
  message(WARNING "Version '${FW_VERSION_FULL}' is ${FW_VERSION_FULL_LEN} characters, "
                  "SWBuildID will be truncated to 16")
endif()

execute_process(
  COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR}
          show -s --format=%cd --date=format:%Y%m%d HEAD
  OUTPUT_VARIABLE FW_COMMIT_DATE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

message(STATUS "Application version: ${FW_VERSION_FULL} (${FW_COMMIT_DATE})")

target_compile_definitions(app PRIVATE
  FW_VERSION_MAJOR=${FW_VERSION_MAJOR}
  FW_VERSION_MINOR=${FW_VERSION_MINOR}
  FW_VERSION_PATCH=${FW_VERSION_PATCH}
  FW_VERSION_FULL="${FW_VERSION_FULL}"
  FW_COMMIT_DATE="${FW_COMMIT_DATE}"
)

target_sources(app PRIVATE
  src/main.c
  src/zigbee.c
  src/led.c
  src/battery.c
  src/button.c
  src/secret_buttons.c
)

if(NOT CONFIG_USB_DEVICE_STACK AND NOT CONFIG_USB_DEVICE_STACK_NEXT)
  target_sources(app PRIVATE src/usbd_reset.c)
endif()
