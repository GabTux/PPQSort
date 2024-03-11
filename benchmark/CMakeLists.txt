cmake_minimum_required(VERSION 3.14...3.22)

project(PPQSortBenchmark LANGUAGES CXX)

include(../cmake/tools.cmake)
include(../cmake/CPM.cmake)

if(TEST_INSTALLED_VERSION)
  find_package(PPQsort REQUIRED)
else()
  CPMAddPackage(NAME PPQsort SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
endif()

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

CPMAddPackage(
        NAME oneTBB
        GITHUB_REPOSITORY oneapi-src/oneTBB
        VERSION 2021.12.0-rc2
        OPTIONS "TBB_TEST OFF"
)

CPMAddPackage(
        NAME poolSTL
        GITHUB_REPOSITORY alugowski/poolSTL
        VERSION 0.3.5
        DOWNLOAD_ONLY
)

CPMAddPackage(
        NAME CCCL
        GITHUB_REPOSITORY NVIDIA/cccl
        VERSION 2.3.1
)

# disable some options now, to just include the library but do not build it
# we will include the header files and link it properly after
# no release tags, using latest commit hash
CPMAddPackage(
        NAME IPS4o
        GITHUB_REPOSITORY ips4o/ips4o
        GIT_TAG d7a74abb47c1913ddc6d6eeca68e59c747ddb693
        OPTIONS "IPS4O_OPTIMIZE_FOR_NATIVE OFF" "IPS4O_DISABLE_PARALLEL ON" "IPS4O_USE_MCX16 OFF"
)
target_link_libraries(ips4o INTERFACE TBB::tbb atomic)
set_target_properties(ips4o PROPERTIES CXX_STANDARD 14)

# no release tags, using latest commit hash
CPMAddPackage(
        NAME cpp11sort
        GITLAB_REPOSITORY daniel.langr/cpp11sort
        GIT_TAG 0aa1607cf6906dd201247d864abd80c9cf123872
        DOWNLOAD_ONLY
)
include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${cpp11sort_SOURCE_DIR}/include>)

# no release tags, using latest commit hash
CPMAddPackage(
        NAME AQsort
        GITHUB_REPOSITORY DanielLangr/AQsort
        GIT_TAG 2e73e4854c72b8a7a2065cdf5da3f506eaa79d14
        DOWNLOAD_ONLY
)
include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${AQsort_SOURCE_DIR}/include>)

# MPQsort is single header library, so it is downloaded directly, without cloning whole repository
# no release tags, using latest commit hash from Jan 27, 2024
message(STATUS "Adding MPQsort")
FetchContent_Declare(
        MPQsort
        URL https://raw.githubusercontent.com/voronond/MPQsort/eaeda34d19b24fffc04af45e929a1ce769b72a4d/include/mpqsort/mpqsort.h
        URL_HASH SHA256=ace123ad88808743ac714b116d59c964b45a67170a224d201407ff67bccd8647
        DOWNLOAD_NO_EXTRACT TRUE
        DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/mpqsort
)
FetchContent_MakeAvailable(MPQsort)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/mpqsort)

# add openmp
find_package(OpenMP REQUIRED)


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
target_link_libraries(${PROJECT_NAME}
        benchmark::benchmark
        OpenMP::OpenMP_CXX
        fast_matrix_market::fast_matrix_market
        Boost::sort
        TBB::tbb
        poolSTL::poolSTL
        CCCL::CCCL
        ips4o
        PPQSort::PPQSort)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
