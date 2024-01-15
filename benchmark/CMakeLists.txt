cmake_minimum_required(VERSION 3.14...3.22)

project(PPQSortBenchmark LANGUAGES CXX)

include(../cmake/tools.cmake)
include(../cmake/CPM.cmake)

CPMAddPackage(
        NAME benchmark
        GITHUB_REPOSITORY google/benchmark
        VERSION 1.8.3
        OPTIONS "BENCHMARK_ENABLE_TESTING Off"
)


if(TEST_INSTALLED_VERSION)
  find_package(PPQsort REQUIRED)
else()
  CPMAddPackage(NAME PPQsort SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
endif()
# ---- Create binary ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(${PROJECT_NAME} ${sources})

find_package(OpenMP)
target_link_libraries(${PROJECT_NAME} benchmark::benchmark OpenMP::OpenMP_CXX PPQSort::PPQSort)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)