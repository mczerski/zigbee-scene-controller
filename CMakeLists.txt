cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project("zigbee-scene-controller")

target_sources(app PRIVATE
  src/main.c
  src/zigbee.c
  src/led.c
  src/battery.c
  src/button.c
  src/secret_buttons.c
)
