# Proto文件
set(SERVER_PROTO_FILES
    SnapshotMetadata.proto
    SnapshotStateMachine.proto
)

# Server库源代码
set(SERVER_SOURCES
    ClientService.cc
    Globals.cc
    Main.cc
    RaftConsensus.cc
    RaftService.cc
    ServerStats.cc
    StateMachine.cc
    Metrics.cc
    MetricsServer.cc
)

# Web管理界面源文件（如果启用）
if(LOGCABIN_ENABLE_WEBADMIN)
    set(SERVER_SOURCES ${SERVER_SOURCES}
    )
endif()

# 生成协议文件
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${SERVER_PROTO_FILES})

# 创建Server库
add_library(Server STATIC 
    ${SERVER_SOURCES}
    ${PROTO_SRCS}
)

# 编译选项
if(LOGCABIN_ENABLE_METRICS)
    target_compile_definitions(Server PRIVATE -DENABLE_METRICS)
endif()

if(LOGCABIN_ENABLE_TELEMETRY)
    target_compile_definitions(Server PRIVATE -DENABLE_TELEMETRY)
endif()

target_link_libraries(Server
    Storage
    Tree
    Client
    Protocol
    RPC
    Event
    Core
    ${PROTOBUF_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

# 主可执行文件 (注释掉，因为在根目录中已经定义)
# add_executable(LogCabin Main.cc)
# target_link_libraries(LogCabin ...)

# 测试
if(LOGCABIN_BUILD_TESTS)
    set(SERVER_TEST_SOURCES
        ClientServiceTest.cc
        RaftConsensusTest.cc
        ServerStatsTest.cc
        StateMachineTest.cc
    )
    
    # Web管理界面测试
    if(LOGCABIN_ENABLE_WEBADMIN)
        list(APPEND SERVER_TEST_SOURCES
        )
    endif()
    
    foreach(test_file ${SERVER_TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        target_link_libraries(${test_name} Server Storage Tree Client Protocol RPC Event Core gtest gtest_main)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
