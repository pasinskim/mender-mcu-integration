cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(mender-mcu-integration)

target_sources(app PRIVATE src/main.c)

generate_inc_file_for_target(app "src/AmazonRootCA1.cer" "${ZEPHYR_BINARY_DIR}/include/generated/AmazonRootCA1.cer.inc")
