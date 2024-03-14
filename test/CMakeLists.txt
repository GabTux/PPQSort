cmake_minimum_required(VERSION 3.14...3.22)

project(PPQSortTests LANGUAGES CXX)

# ---- Options ----

option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)
option(TEST_INSTALLED_VERSION "Test the version found by find_package" OFF)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        GIT_TAG v1.14.0
        OPTIONS "INSTALL_GTEST OFF" "gtest_force_shared_crt"
)

if(TEST_INSTALLED_VERSION)
  find_package(PPQSort REQUIRED)
else()
  CPMAddPackage(NAME PPQSort SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
endif()

# ---- Create binary ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(${PROJECT_NAME} ${sources})
target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock PPQSort::PPQSort)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)

# enable compiler warnings
if(NOT TEST_INSTALLED_VERSION)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(PPQSort INTERFACE -Wall -Wpedantic -Wextra -Werror)
  elseif(MSVC)
    target_compile_options(PPQSort INTERFACE /W4 /WX)
  endif()
endif()

# ---- Add PPQSortTests ----

enable_testing()

# Note: doctest and similar testing frameworks can automatically configure CMake tests. For other
# testing frameworks add the tests target instead: add_test(NAME ${PROJECT_NAME} COMMAND
# ${PROJECT_NAME})

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})

# ---- code coverage ----

if(ENABLE_TEST_COVERAGE)
  target_compile_options(PPQSort INTERFACE -O0 -g -fprofile-arcs -ftest-coverage)
  target_link_options(PPQSort INTERFACE -fprofile-arcs -ftest-coverage)
endif()
