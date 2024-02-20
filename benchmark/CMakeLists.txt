cmake_minimum_required(VERSION 3.14...3.22)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

project(PPQSortBenchmark LANGUAGES CXX)

include(../cmake/tools.cmake)
include(../cmake/CPM.cmake)

CPMAddPackage(
        NAME benchmark
        GITHUB_REPOSITORY google/benchmark
        VERSION 1.8.3
        OPTIONS "BENCHMARK_ENABLE_TESTING Off"
)

CPMAddPackage(
        NAME fast_matrix_market
        GITHUB_REPOSITORY alugowski/fast_matrix_market
        VERSION 1.7.6
)

CPMAddPackage(
        NAME Boost
        VERSION 1.84.0
        URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
        URL_HASH SHA256=2e64e5d79a738d0fa6fb546c6e5c2bd28f88d268a2a080546f74e5ff98f29d0e
        OPTIONS "BOOST_ENABLE_CMAKE ON" "BOOST_INCLUDE_LIBRARIES sort"
)

if(TEST_INSTALLED_VERSION)
  find_package(PPQsort REQUIRED)
else()
  CPMAddPackage(NAME PPQsort SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
endif()

# ---- Download matrix benchmark data ----
function(get_matrix_data FILE_URL FILE_NAME)
  set(TARGET_FILE_NAME ${FILE_NAME}.mtx)
  if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_FILE_NAME})
    message(STATUS "${FILE_NAME} already present, using cached version")
    return()
  endif()
  message(STATUS "Downloading ${FILE_NAME} from ${FILE_URL}")
  file(DOWNLOAD ${FILE_URL} ${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}.tar.gz
    SHOW_PROGRESS
    STATUS DOWNLOAD_RESULT
    LOG DOWNLOAD_LOG)
  list(GET DOWNLOAD_RESULT 0 DOWNLOAD_RESULT_CODE)
  list(GET DOWNLOAD_RESULT 1 DOWNLOAD_RESULT_STRING)
  if(NOT DOWNLOAD_RESULT_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to download ${FILE_NAME}: ${DOWNLOAD_RESULT_STRING}")
  endif()
  message(STATUS "Extracting ${FILE_NAME}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xzf ${FILE_NAME}.tar.gz
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      RESULT_VARIABLE EXTRACT_RESULT
      OUTPUT_VARIABLE EXTRACT_OUTPUT
      ERROR_VARIABLE EXTRACT_ERROR)
  if(NOT EXTRACT_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to extract ${FILE_NAME}: ${EXTRACT_ERROR}")
  endif()
  message(STATUS "Extracted ${FILE_NAME}")
  # move the mtx file to the root of the directory
  file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}/${TARGET_FILE_NAME} ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_FILE_NAME})
  file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}.tar.gz)
  file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME}/)
endfunction()

get_matrix_data("http://sparse-files.engr.tamu.edu/MM/Schenk_AFE/af_shell10.tar.gz" "af_shell10")
get_matrix_data("https://suitesparse-collection-website.herokuapp.com/MM/vanHeukelum/cage15.tar.gz" "cage15")
get_matrix_data("https://suitesparse-collection-website.herokuapp.com/MM/Lee/fem_hifreq_circuit.tar.gz" "fem_hifreq_circuit")

# also possible to use CPMAddPackage, but it will not remove the archive after extraction
#CPMAddPackage("http://sparse-files.engr.tamu.edu/MM/Schenk_AFE/af_shell10.tar.gz")
#CPMAddPackage("https://suitesparse-collection-website.herokuapp.com/MM/vanHeukelum/cage15.tar.gz")
#CPMAddPackage("https://suitesparse-collection-website.herokuapp.com/MM/Lee/fem_hifreq_circuit.tar.gz")

# ---- Create binary ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(${PROJECT_NAME} ${sources})

find_package(OpenMP)
target_link_libraries(${PROJECT_NAME} benchmark::benchmark OpenMP::OpenMP_CXX fast_matrix_market::fast_matrix_market Boost::sort PPQSort::PPQSort)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)