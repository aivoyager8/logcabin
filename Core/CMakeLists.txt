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

# 处理 Protocol Buffers
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ProtoBufTest.proto)

# 创建 Core 库
add_library(Core STATIC ${CORE_SOURCES} ${PROTO_SRCS})
target_link_libraries(Core
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
        target_link_libraries(${test_name} Core gtest gtest_main)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
