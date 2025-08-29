# ================================
# 修正后的调试版Dockerfile
# ================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 安装基础依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libmysqlclient-dev \
    libssl-dev \
    tzdata \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# 1. 创建并进入构建目录
# WORKDIR 会自动创建目录，并让后续指令在此目录下执行
WORKDIR /app/build

# 2. 运行 CMake (独立一步，出错会立刻停在这里)
# 日志会清晰地显示 CMake 的所有输出
RUN cmake ..

# 3. 运行 Make (独立一步，出错会立刻停在这里)
# 如果上面成功，这里出错，说明是 C++ 编译问题
RUN make server

# 将工作目录切换回 /app，方便后续操作（如果需要）
WORKDIR /app

# 暴露端口
EXPOSE 9006

# 创建用户和设置权限
RUN groupadd -r webserver && useradd -r -g webserver webserver
RUN mkdir -p logs && chown -R webserver:webserver /app

# 环境变量
ENV DB_HOST=mysql
ENV DB_USER=root  
ENV DB_PASSWORD=webserver123
ENV DB_NAME=webserver
ENV SERVER_PORT=9006

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD netcat -z localhost 9006 || exit 1

# 切换用户
USER webserver

# 启动命令
CMD ["./build/server", "-p", "9006", "-l", "1", "-m", "0", "-o", "1", "-s", "10", "-t", "8", "-c", "1", "-a", "1"]