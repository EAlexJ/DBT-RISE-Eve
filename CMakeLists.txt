cmake_minimum_required(VERSION 3.24)
project(eve_sim VERSION 1.0 LANGUAGES C CXX)
cmake_policy(SET CMP0169 OLD)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMMON_SOURCES src/main.cpp src/core.cpp)
set(COMMON_INCLUDE_DIR src/)

add_subdirectory(DBT-RISE-Core)
find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(eve_sim_interp ${COMMON_SOURCES} src/interp/vm.cpp)
target_include_directories(eve_sim_interp PRIVATE ${COMMON_INCLUDE_DIR} src/interp/)
target_link_libraries(eve_sim_interp PRIVATE ${CONAN_LIBS} dbt-rise-core Boost::program_options)

add_executable(asm8 EXCLUDE_FROM_ALL src/eve_assembler/asm8.c)
target_compile_options(asm8 PRIVATE -std=c99 -Wall -O2 -fsigned-char)