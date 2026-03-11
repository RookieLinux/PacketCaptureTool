# Packet Capture Tool

一个功能强大的网络数据包捕获与分析工具，专为自定义协议解析而设计。采用 Qt6 QML 构建现代化用户界面，C++ 实现高效后端逻辑，支持实时数据包捕获及基于 JSON 配置的多协议解析。

## 技术栈

- **UI 框架**: Qt 6.4+ (使用 FluentUI 风格组件)
- **后端语言**: C++17
- **抓包库**: PcapPlusPlus (3.0+)
- **构建系统**: CMake (3.16+)
- **测试框架**: Qt Test Framework

## 主要特性

- **实时捕获**: 支持通过 PcapPlusPlus 实时抓取网络接口数据包。
- **自定义协议解析**: 通过 JSON 配置文件定义私有协议字段、类型（Int, String, ByteArray 等）及字节序。
- **协议过滤**: 支持按传输层协议（UDP/TCP）及端口号进行过滤。
- **离线分析**: 支持加载 PCAP 文件进行后期分析，或将捕获结果保存为 PCAP。
- **现代化 UI**: 基于 FluentUI 设计，提供直观的数据包列表、十六进制视图及详细解析树。

## 项目结构

```text
.
├── 3rdlib/             # 第三方依赖库 (如 FluentUI)
├── cmake/              # CMake 辅助脚本
├── src/
│   ├── backend/        # C++ 后端核心逻辑
│   │   ├── PacketCaptureEngine  # 数据包捕获引擎
│   │   ├── FilterEngine         # 过滤与匹配逻辑
│   │   ├── ProtocolParser       # 自定义协议解析器
│   │   ├── ConfigurationLoader  # 协议配置加载 (JSON)
│   │   ├── CaptureController    # UI 与后端的桥接层
│   │   └── PacketModel          # QML 列表模型
│   └── ui/             # QML 前端界面
│       ├── MainWindow.qml      # 主界面
│       └── qml.qrc             # 资源清单
├── tests/              # 自动化单元测试
└── CMakeLists.txt      # 项目主构建文件
```

## 快速开始

### 依赖环境

- **Qt 6.4+**: 需要安装 Core, Gui, Qml, Quick, QuickControls2 模块。
- **PcapPlusPlus**:
  - **Linux**:
    ```bash
    sudo apt-get install libpcap-dev
    # 推荐从源码编译安装 PcapPlusPlus 以确保兼容性
    git clone https://github.com/seladb/PcapPlusPlus.git
    cd PcapPlusPlus && cmake -B build && cmake --build build --target install
    ```
  - **Windows**: 需要安装 Npcap SDK 及 WinPcap/Npcap 驱动，并在 CMake 中配置 PcapPlusPlus 路径。

### 构建与运行

```bash
# 克隆仓库
git clone https://github.com/RookieLinux/PacketCaptureTool
cd PacketCaptureTool

# 配置并构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 运行 (Linux 下捕获网卡数据需要 sudo 权限)
sudo ./build/PacketCaptureTool
```

## 使用指南

1. **加载配置**: 点击界面上的 "Load Config" 按钮，加载包含协议定义的 `.json` 文件。
2. **选择接口**: 在网卡下拉列表中选择要监听的网卡。
3. **开始抓包**: 点击 "Start" 开始实时捕获。
4. **分析详情**: 点击列表中捕获到的数据包，右侧将显示根据 JSON 配置解析出的详细字段。

## 单元测试

项目包含完善的测试覆盖：
```bash
cd build
ctest --output-on-failure
```

## 架构设计

系统遵循 **MVVM (Model-View-ViewModel)** 模式：
- **View (QML)**: 响应式 UI 展现。
- **ViewModel (CaptureController)**: 封装业务逻辑，通过 Qt 信号槽与 QML 交互。
- **Model**: 包括 `PacketModel` 数据源及底层 `Engine` 模块。

## 许可证

[MIT License](LICENSE)

