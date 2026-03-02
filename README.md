# TinyWebServer-Learning 🚀

基于 Linux 的 C++11 高并发 HTTP 服务器学习项目

## 📋 项目简介

实现了一个支持高并发的 HTTP/1.1 Web 服务器，核心特性包括：

- ✅ **HTTP协议**：支持 GET/POST 请求，静态资源访问
- ✅ **高并发模型**：Reactor + 线程池 + epoll (ET模式)
- ✅ **线程池监控**：每5秒打印队列长度和线程数（验证调优）
- ✅ **epoll事件日志**：实时打印事件，验证ET模式行为
- ✅ **性能验证**：wrk压测验证线程数=16最优（11547 QPS）

## 🏗️ 架构图
┌─────────────────────────────────────────┐
│           主线程 (Main Thread)           │
│  ┌─────────┐    ┌─────────────────────┐ │
│  │ Listen  │───→│   epoll_wait()      │ │
│  │ Socket  │    │   (ET模式)           │ │
│  └─────────┘    └─────────────────────┘ │
│                          ↓              │
│                   ┌─────────────┐        │
│                   │ 新连接/可读  │        │
│                   └─────────────┘        │
└─────────────────────────────────────────┘
↓
┌─────────────────────────────────────────┐
│         线程池 (Thread Pool)            │
│  ┌────────┐ ┌────────┐ ┌────────┐      │
│  │ Worker │ │ Worker │ │ Worker │ ...  │
│  │ Thread │ │ Thread │ │ Thread │      │
│  └────────┘ └────────┘ └────────┘      │
│       ↓          ↓          ↓           │
│  HTTP解析 → 业务处理 → 生成响应          │
└─────────────────────────────────────────┘
plain
复制

## 🛠️ 技术栈

| 模块 | 技术 | 说明 |
|------|------|------|
| 语言标准 | C++11 | 智能指针、lambda、auto |
| 并发模型 | 线程池 | 固定线程数（实验验证16线程最优） |
| IO多路复用 | epoll | ET模式 + EPOLLONESHOT |
| 同步机制 | sem信号量 | 精准唤醒，避免虚假唤醒 |
| 构建工具 | Makefile | 一键编译 |

## 🚀 快速开始

```bash
# 克隆
git clone https://github.com/FSSLI/TinyWebServer-Learning.git
cd TinyWebServer-Learning

# 编译
make clean && make

# 运行（需要root权限绑定端口）
sudo ./server

# 测试
curl http://localhost:9006/index.html
✨ 我的改进点
1. 线程池调优验证（核心亮点）
实验数据：8核16线程CPU，wrk压测1MB文件
关键发现：16线程QPS 11547最优，32线程下降至10834
结论：线程数=逻辑处理器数时最优，过多上下文切换反而下降
代码：添加线程池监控日志，每5秒输出queue长度和线程数
bash
复制
# 启动后日志输出示例
[14:30:15] [INFO] WebServer started, threadpool: 16 threads
[14:30:20] [THREADPOOL] queue=0/10000 threads=16
[14:30:25] [THREADPOOL] queue=3/10000 threads=16  # 压测时队列堆积
2. epoll ET模式验证
实现：epoll事件日志，实时打印EPOLLIN/EPOLLOUT
验证：ET模式只在状态变化时触发，HTTP长连接IN/OUT交替
plain
复制
[14:30:35] [EPOLL] LISTEN fd=12 events=IN (new connection)
[14:30:35] [EPOLL] CLIENT fd=16 events=IN   # 读请求
[14:30:35] [EPOLL] CLIENT fd=16 events=OUT  # 写响应
[14:30:36] [EPOLL] CLIENT fd=16 events=IN   # Keep-Alive下一个请求
📊 性能测试
bash
复制
# wrk压测命令
wrk -t16 -c100 -d10s http://127.0.0.1:9006/1m.bin
表格
线程数	QPS	吞吐量	说明
8	10731	8.00 MB/s	线程不足，CPU未充分利用
16	11547	8.61 MB/s	最优，匹配逻辑处理器数
32	10834	8.08 MB/s	上下文切换开销，性能下降
环境：WSL2，8核16线程，1MB静态文件
📝 项目结构
plain
复制
TinyWebServer-Learning/
├── src/              # 源代码
│   ├── main.cpp      # 程序入口
│   ├── webserver.cpp # 服务器核心（含监控日志）
│   └── ...
├── include/          # 头文件
│   └── threadpool/   # 线程池实现
├── docs/             # 文档与截图
├── resources/        # 静态资源
└── README.md
📚 参考资料
TinyWebServer 原项目
《Linux高性能服务器编程》- 游双
🎯 学习记录
2024.02.23 项目启动，完成基础编译运行
2024.02.25 线程池深入学习，理解sem信号量 vs condition_variable
2024.02.28 添加线程池监控与epoll事件日志，验证性能调优
2024.03.01 整理代码，编写完整文档，GitHub发布
EOF