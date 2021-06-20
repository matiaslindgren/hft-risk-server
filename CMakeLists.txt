cmake_minimum_required(VERSION 3.8)
project(RiskService
  VERSION 1.0
  DESCRIPTION "High frequency trading risk server"
  LANGUAGES CXX)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}\
  -g \
  -O2 \
  -Wall \
  -pedantic \
  -Wextra \
  -fno-omit-frame-pointer \
  -fsanitize=address,undefined")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(risk-server src/tcp.cpp src/risk_service.cpp src/main.cpp)
add_executable(test src/tcp.cpp tests/main.cpp)

target_include_directories(risk-server PUBLIC include)
target_include_directories(test PUBLIC include)
