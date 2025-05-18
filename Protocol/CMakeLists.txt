# Protocol库
set(PROTO_FILES
    Protocol.proto
    ServerStats.proto
    ServerControl.proto
)

# 生成协议文件
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# 创建Protocol库
add_library(Protocol STATIC ${PROTO_SRCS})
target_link_libraries(Protocol
    ${PROTOBUF_LIBRARIES}
    Core
)

# 添加协议头文件路径
target_include_directories(Protocol PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)
