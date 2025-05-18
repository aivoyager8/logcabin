# Storage库源代码
set(STORAGE_SOURCES
    SegmentedLog.cc
    SimpleFileLog.cc
    SnapshotFile.cc
    SnapshotMetadata.proto
    Tool.cc
    LogFactory.cc
    Memory.cc
    ModuleInit.cc
)

# 如果定义了LSM存储，添加相关源文件
if(LOGCABIN_ENABLE_LSM_STORAGE)
    list(APPEND STORAGE_SOURCES
        LSMStorage.cc
        LSMStorage.h
    )
endif()

# 生成协议文件
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS SnapshotMetadata.proto)

# 创建Storage库
add_library(Storage STATIC 
    ${STORAGE_SOURCES}
    ${PROTO_SRCS}
)

target_link_libraries(Storage
    Core
    ${PROTOBUF_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
)

# 存储工具可执行文件
add_executable(StorageTool Tool.cc)
target_link_libraries(StorageTool 
    Storage 
    Core
    ${PROTOBUF_LIBRARIES}
    ${CRYPTOPP_LIBRARIES}
)
set_target_properties(StorageTool PROPERTIES OUTPUT_NAME "Tool")

# 测试
if(LOGCABIN_BUILD_TESTS)
    set(STORAGE_TEST_SOURCES
        LogFactoryTest.cc
        MemoryTest.cc
        ModuleInitTest.cc
        SegmentedLogTest.cc
        SimpleFileLogTest.cc
        SnapshotFileTest.cc
    )
    
    # 如果LSM存储启用，添加相应测试
    if(LOGCABIN_ENABLE_LSM_STORAGE)
        list(APPEND STORAGE_TEST_SOURCES
            LSMStorageTest.cc
        )
    endif()
    
    foreach(test_file ${STORAGE_TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        target_link_libraries(${test_name} Storage Core gtest gtest_main)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
