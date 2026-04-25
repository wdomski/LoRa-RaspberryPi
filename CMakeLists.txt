cmake_minimum_required(VERSION 3.10)
project(lora)

add_library(lora SHARED loralib.c)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

target_include_directories(lora PUBLIC ${Python3_INCLUDE_DIRS})

target_compile_definitions(lora PUBLIC PYTHONMODULE)

target_link_libraries(lora PUBLIC wiringPi ${Python3_LIBRARIES})

add_custom_command(TARGET lora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/lora/liblora.so ${BINARY_DIRECTORY}/liblora.so
    COMMENT "Copying liblora.so after build"
)