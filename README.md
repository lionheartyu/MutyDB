# MutyDB

## 端口说明
- **epoll**: 端口 `2048`
- **io_uring**: 端口 `1024`
- **ntyco**: 端口 `9096`
- **muduo**: 端口 `8080`

## 快速开始

1. **环境准备**  
   安装liburing库,sudo apt-get install liburing-dev
   在 `Kmuduo` 目录下执行以下命令，一键部署并编译所需环境：
   ```bash
   ./autobuild.sh
   ```

2. **配置说明**  
   - 在 `kvstore.h` 文件中，通过宏开关选择启用 `ntyco` 协程库、`muduo` 网络库以及 `io_uring`。
   - **注意：** 使用 `muduo` 网络库时，请在 `kmuduo_entry.cc` 文件中，将服务器 IP 地址设置为您本机的 IP 地址。

3. **编译项目**  
   在项目根目录下执行：
   ```bash
   make
   ```
   即可一键编译，快速体验。

## 备注

- 请根据实际需求选择合适的网络库和协程库。
- 如遇到编译或运行问题，请查阅相关文档或提交 issue。
