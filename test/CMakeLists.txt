\
# CMakeLists.txt for the test directory

# Find all test source files
file(GLOB_RECURSE TEST_SOURCES *.cc *.h)

# Add a test executable for each test file that is not a header
# This is a simplified approach; a more robust solution might involve
# naming executables based on the test file name or grouping tests.

set(LOGCABIN_TEST_SOURCES)
# Gather actual source files (not headers or mocks)
# This is a heuristic and might need adjustment based on actual file naming conventions
file(GLOB MAIN_TEST_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*Test.cc")
file(GLOB MOCK_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*Mock.cc")


# Add LogCabin library for linking
# Ensure logcabin_lib is defined in the parent scope (main CMakeLists.txt)
if(NOT TARGET logcabin_lib)
    message(WARNING "logcabin_lib target not found. Tests might not link correctly.")
endif()

# Add gtest
# Ensure GTest::GTest and GTest::Main are available
# This is typically handled in the root CMakeLists.txt
if(NOT TARGET GTest::GTest OR NOT TARGET GTest::Main)
    message(WARNING "GTest targets not found. Tests might not build or link correctly.")
endif()

foreach(test_file ${MAIN_TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file} ${MOCK_FILES})
    target_link_libraries(${test_name} PRIVATE logcabin_lib GTest::GTest GTest::Main Threads::Threads)
    add_test(NAME ${test_name} COMMAND ${test_name})

    # Add include directories for the test executable
    target_include_directories(${test_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR} # For generated protobuf headers
        ${GTEST_INCLUDE_DIRS} # If gtest is found via find_package
    )
    # Add compile definitions if needed, e.g., for protobuf
    target_compile_definitions(${test_name} PRIVATE DEBUG LOGCABIN_GTEST_HEADER="/root/codes/logcabin/gtest/include/gtest/gtest.h")

endforeach()

message(STATUS "Configured tests in test directory.")

