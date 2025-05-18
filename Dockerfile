FROM ubuntu:22.04

# 安装构建依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    python3-dev \
    scons \
    libprotobuf-dev \
    protobuf-compiler \
    libcrypto++-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# 创建日志目录和用户
RUN useradd -r -s /bin/false logcabin && \
    mkdir -p /var/log/logcabin /var/lib/logcabin && \
    chown -R logcabin:logcabin /var/log/logcabin /var/lib/logcabin

# 设置工作目录
WORKDIR /logcabin

# 复制源代码
COPY . .

# 构建项目
RUN scons BUILDTYPE=RELEASE

# 创建配置目录
RUN mkdir -p /etc/logcabin && \
    cp sample.conf /etc/logcabin/logcabin.conf

# 暴露默认端口
EXPOSE 5254

# 设置数据卷
VOLUME ["/var/log/logcabin", "/var/lib/logcabin", "/etc/logcabin"]

# 设置入口点
ENTRYPOINT ["/logcabin/build/LogCabin", "--config", "/etc/logcabin/logcabin.conf"]
