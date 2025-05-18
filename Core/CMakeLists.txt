# 定义 Core 库
set(CORE_SOURCES
    Buffer.cc
    Checksum.cc
    ConditionVariable.cc
    Config.cc
    Debug.cc
    ProtoBuf.cc
    Random.cc
    RollingStat.cc
    StringUtil.cc
    ThreadId.cc
    Time.cc
    Util.cc
)

# Core has its own test protobuf file.
protobuf_generate_cpp(CORE_PROTO_SRCS CORE_PROTO_HDRS ProtoBufTest.proto)

# 创建 Core 库
add_library(Core STATIC ${CORE_SOURCES} ${CORE_PROTO_SRCS})

# Core needs to find its own generated headers and headers from Protocol.
target_include_directories(Core PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}"  # For Core's own generated ProtoBufTest.pb.h
    # Protocol will export its include directory for ServerStats.pb.h etc.
)

target_link_libraries(Core PUBLIC # Changed to PUBLIC if other modules depend on Core's specifics
    Protocol # Core depends on Protocol for ServerStats.pb.h
    ${PROTOBUF_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# 测试
if(LOGCABIN_BUILD_TESTS)
    set(CORE_TEST_SOURCES
        BufferTest.cc
        ChecksumTest.cc
        CompatHashTest.cc
        ConditionVariableTest.cc
        ConfigTest.cc
        DebugTest.cc
        ProtoBufTest.cc
        RandomTest.cc
        RollingStatTest.cc
        STLUtilTest.cc
        StringUtilTest.cc
        ThreadIdTest.cc
        TimeTest.cc
        UtilTest.cc
    )
    
    foreach(test_file ${CORE_TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        # Test executables also need to find generated headers.
        target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        target_link_libraries(${test_name} Core gtest gtest_main)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
