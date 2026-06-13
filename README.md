# MeteorPush 消息推送系统

高性能 IM 推送系统，基于 C++17 实现，参考 B 站 goim 三层架构设计。

支持**单聊**、**聊天室**、**视频弹幕**、**全局广播**四种消息场景，单聊 QPS 达到 **50,000**。

## 架构

```
                           ┌─────────────┐
                           │  Web Client  │
                           └──────┬───────┘
                                  │ WebSocket (9000)
                                  ▼
┌──────────────────────────────────────────────────────────────┐
│                     Comet 接入层                              │
│  WebSocket 连接管理 · 帧解析 · 心跳检测 · 本地房间路由         │
└────────────┬──────────────────────────────────┬──────────────┘
             │ gRPC 双向流 (9100)                │ gRPC (9105)
             ▼                                   ▲
┌────────────────────────┐          ┌────────────┴─────────────┐
│     Logic 逻辑层        │          │        Job 消费层         │
│  鉴权 · 路由 · 限流     │──Kafka──→│  推送(PushHandler)        │
│  序号分配 · HTTP API    │          │  持久化(PersistHandler)   │
└────────────────────────┘          └──────────────────────────┘
     │          │                            │
     ▼          ▼                            ▼
  Redis      Kafka                        MySQL
```

## 核心特性

- **三层解耦架构**：Comet/Logic/Job 独立部署、独立扩缩容
- **gRPC 双向流**：Comet↔Logic 持久连接，消除逐条 RPC 开销，QPS 翻倍
- **Kafka 双消费组**：推送和持久化解耦，互不影响
- **本地路由缓存**：`shared_mutex` 读写锁，1 秒 TTL，减少 Redis 查询
- **手写 WebSocket**：基于 Muduo，完整实现 RFC 6455 握手 + 帧解析
- **Redis 滑动窗口限流**：Sorted Set + MULTI 事务

## 性能数据

| 优化阶段 | 单聊 QPS | 关键改动 |
|---------|---------|---------|
| 基线（同步 MySQL） | 8,330 | — |
| + 本地缓存 + shared_mutex | 16,656 | 路由查询走内存 |
| + gRPC 双向流 | 33,322 | 持久流替代 Unary RPC |
| + 200 并发连接 | **~50,000** | 充分利用多线程 |

| 场景 | QPS |
|-----|-----|
| 房间聊天（100 用户） | 20,030 |
| HTTP API（4 线程） | 5,710 |

## 技术栈

| 组件 | 技术 |
|------|------|
| 语言 | C++17 |
| 网络框架 | Muduo (Reactor, one loop per thread) |
| RPC | gRPC (Unary + 双向流 + 客户端流) |
| 消息队列 | Apache Kafka (librdkafka) |
| 缓存 | Redis (hiredis) |
| 数据库 | MySQL (libmysqlclient) |
| 序列化 | Protocol Buffers 3 |
| JSON | nlohmann/json |
| 构建 | CMake 3.14+ |

## 目录结构

```
MeteorPush/
├── comet/          # 接入层：WebSocket 连接管理、gRPC 服务端
├── logic/          # 逻辑层：gRPC 服务、HTTP API、Redis 路由
├── job/            # 消费层：Kafka 消费、推送、持久化
├── common/         # 公共组件：连接池、线程池、Kafka 封装
├── proto/          # Protocol Buffers 定义
├── muduo/          # Muduo 网络库（内置，自动编译）
├── web_demo/       # Web 演示前端
├── load_test/      # 压测工具
├── sql/            # 数据库 Schema
├── conf/           # 配置文件
└── cmake/          # CMake 查找模块
```

## 端口一览

| 组件            | 协议/用途                 | 默认端口 | 配置项                                                   |
| --------------- | ------------------------- | -------- | -------------------------------------------------------- |
| Logic gRPC      | gRPC 业务入口             | 9100     | `logic.conf: listen_port`                                |
| Logic HTTP      | 登录/发送消息 HTTP API    | 9101     | `logic.conf: http_port`                                  |
| Comet WebSocket | 用户长连接                | 9000     | `comet.conf: listen_port`                                |
| Comet gRPC      | Job 下行到 Comet          | 9105     | `comet.conf: comet_grpc_port`；`job.conf: comet_targets` |
| Job             | Kafka 消费，向 Comet 推送 | 无监听   | -                                                        |
| Web Demo        | 静态页面访问              | 9010     | `--port` 启动参数                                        |

- `comet.conf: logic_grpc_target` 必须指向 Logic 的 gRPC 地址（默认 `127.0.0.1:9100`）。
- 如果调整端口，请同时修改相互引用的配置项（如 `logic_grpc_target`、`comet_targets`），以免连不上。

---

## 环境依赖与安装

目标平台：Ubuntu 20.04 / 22.04

### 1. 必备工具

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config git curl
```

### 2. 系统库（C++ 编译依赖）

```bash
# Protobuf + gRPC
sudo apt install -y protobuf-compiler libprotobuf-dev
sudo apt install -y libgrpc++-dev protobuf-compiler-grpc

# Kafka 客户端 SDK
sudo apt install -y librdkafka-dev

# Redis 客户端 SDK
sudo apt install -y libhiredis-dev

# MySQL 客户端 SDK
sudo apt install -y libmysqlclient-dev

# JSON 库
sudo apt install -y nlohmann-json3-dev

# OpenSSL / 压缩
sudo apt install -y libssl-dev zlib1g-dev
```

> **说明**：仓库自带 muduo 源码，通过顶层 CMakeLists 自动编译，无需系统安装 muduo。
> 若发行版的 gRPC 包缺失，可选用源码编译（官方 quickstart），再通过 `cmake -DgRPC_DIR=<path>` 指向安装前缀。

验证安装：

```bash
pkg-config --cflags --libs grpc++ grpc
# 应输出类似: -lgrpc++ -lgrpc
```

### 3. 安装 Kafka（KRaft 模式，无需 Zookeeper）

#### 3.1 安装 Java 环境

Kafka 3.7.0 推荐使用 Java 17：

```bash
sudo apt install -y openjdk-17-jdk
java -version
```

设置 `JAVA_HOME`：

```bash
# 查找 Java 安装路径
sudo update-alternatives --config java
# 示例输出: /usr/lib/jvm/java-17-openjdk-amd64/bin/java

# 编辑 ~/.bashrc，添加：
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH=$JAVA_HOME/bin:$PATH

# 生效
source ~/.bashrc
echo $JAVA_HOME
```

#### 3.2 下载并配置 Kafka

```bash
wget https://mirrors.huaweicloud.com/apache/kafka/3.7.0/kafka_2.13-3.7.0.tgz
tar -xzf kafka_2.13-3.7.0.tgz
cd kafka_2.13-3.7.0

# 生成集群 ID（KRaft 模式）
bin/kafka-storage.sh random-uuid
# 复制输出的 UUID

# 配置 KRaft 模式
cp config/kraft/server.properties config/kraft/server-local.properties
# 编辑 server-local.properties，确保：
#   node.id=1
#   controller.quorum.voters=1@localhost:9093
#   listeners=PLAINTEXT://0.0.0.0:9092
#   advertised.listeners=PLAINTEXT://localhost:9092
#   log.dirs=/tmp/kraft-combined-logs

# 格式化存储
bin/kafka-storage.sh format -t <你的UUID> -c config/kraft/server-local.properties

# 启动 Kafka
bin/kafka-server-start.sh -daemon config/kraft/server-local.properties
```

#### 3.3 创建 Topic

Kafka **不会由程序自动创建主题**，请先创建：

```bash
# 推送主通道
bin/kafka-topics.sh --create \
  --bootstrap-server 127.0.0.1:9092 \
  --replication-factor 1 \
  --partitions 3 \
  --topic push_to_comet

# 广播任务通道
bin/kafka-topics.sh --create \
  --bootstrap-server 127.0.0.1:9092 \
  --replication-factor 1 \
  --partitions 3 \
  --topic broadcast_task
```

验证：

```bash
bin/kafka-topics.sh --list --bootstrap-server 127.0.0.1:9092
```

### 4. 安装 MySQL 并初始化

```bash
# 安装 MySQL Server（如未安装）
sudo apt install -y mysql-server
sudo systemctl start mysql

# 初始化数据库
mysql -u root -p < sql/schema.sql
```

### 5. 安装 Redis

```bash
sudo apt install -y redis-server
sudo systemctl start redis-server
```

---

## 编译

```bash
cd MeteorPush
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

编译输出在 `build/` 目录下：

| 可执行文件 | 路径 |
|-----------|------|
| logic_server | `build/logic/logic_server` |
| comet_server | `build/comet/comet_server` |
| job_server | `build/job/job_server` |
| web_demo_server | `build/web_demo/web_demo_server`（若启用 `BUILD_WEB_DEMO`） |
| ws_client_cli | `build/load_test/ws_client_cli` |
| load_tester | `build/load_test/load_tester` |

如需关闭 Web Demo：`cmake .. -DBUILD_WEB_DEMO=OFF`

---

## 运行指引

### 配置

- 默认配置位于 `conf/logic.conf`、`conf/comet.conf`、`conf/job.conf`。
- 重点字段：
  - `logic.conf`：`listen_port`(gRPC)、`http_port`(HTTP)、Kafka/Redis/MySQL 连接信息。
  - `comet.conf`：`listen_port`(WS)、`comet_grpc_port`、`logic_grpc_target`。
  - `job.conf`：`kafka_*` 以及 `comet_targets`（`名称=地址:端口`）。
- 每个可执行文件均支持 `--config <path>` 指定配置文件。

### 启动顺序

```bash
# 确保 Redis、MySQL、Kafka 已启动

cd MeteorPush/build

# 依次启动（分别在不同终端）
./logic/logic_server --config ../conf/logic.conf
./comet/comet_server --config ../conf/comet.conf
./job/job_server   --config ../conf/job.conf

# 可选：启动 Web Demo
./web_demo/web_demo_server --port 9010 --doc-root ../web_demo/static
# 访问 http://localhost:9010/index.html
```

- 多个 Comet 实例可复用同一配置文件，只需调整 `comet_id`、`listen_port`、`comet_grpc_port` 并在 `job.conf` 的 `comet_targets` 中一一列出。
- 若端口占用，可修改对应配置，再按端口表保持互相引用的一致性。

---

## 常见问题

### 依赖缺失

若 CMake 报告库未找到，请确认相关 `-dev` 包已安装，并检查 `CMAKE_PREFIX_PATH` / `PKG_CONFIG_PATH`。

### protoc 版本不匹配

CMake 报错：

```
Protobuf compiler version 3.12.4 doesn't match library version 3.19.4
```

解决方法：

```bash
# 查看多版本 protoc
whereis protoc
/usr/bin/protoc --version
/usr/local/bin/protoc --version

# 指定高版本优先
export PATH=/usr/local/bin:$PATH
protoc --version  # 确认切换成功
```

### 端口冲突 / 互相引用错误

- `logic.conf: listen_port` 用于 gRPC，`http_port` 用于 HTTP 接口；
- `comet.conf: listen_port` 用于 WebSocket，`comet_grpc_port` 用于向 Job 暴露的 gRPC；
- `comet.conf: logic_grpc_target` 必须指向 Logic 的 gRPC 地址；
- `job.conf: comet_targets` 中的端口必须与对应 Comet 的 `comet_grpc_port` 一致。

### Kafka 主题不存在

启动 `job_server` 前请先创建 `push_to_comet`、`broadcast_task`，或启用集群自动创建。

