# 定义 RPC 库
set(RPC_SOURCES
    Address.cc
    ClientRPC.cc
    ClientSession.cc
    OpaqueServerRPC.cc
    Server.cc
    ServerRPC.cc
    ServiceMock.cc
    TLS.cc    # TLS 支持
)

# 编译选项
if(NOT LOGCABIN_ENABLE_TLS)
    add_compile_definitions(DISABLE_TLS)
endif()

# 创建 RPC 库
add_library(RPC STATIC ${RPC_SOURCES})
target_link_libraries(RPC 
    Core 
    Event
    ${OPENSSL_LIBRARIES}
)

# 测试
if(LOGCABIN_BUILD_TESTS)
    set(RPC_TEST_SOURCES
        AddressTest.cc
        ClientRPCTest.cc
        ClientSessionTest.cc
        OpaqueServerRPCTest.cc
        ServerRPCTest.cc
        ServerTest.cc
        TLSTest.cc
    )
    
    foreach(test_file ${RPC_TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        target_link_libraries(${test_name} RPC Event Core gtest gtest_main)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
