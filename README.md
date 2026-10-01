# mcl

> **一个标准尺寸的窗口，窗口里是一整套 macOS 12 桌面。启动器内核完全自研。**

mcl 把 macOS 12 的桌面交互——顶部菜单栏、底部 Dock、自绘窗口、红黄绿交通灯、
拖拽与缩放——用 Qt 6 + QML **全自绘**实现在一个普通窗口里，同时内置 Minecraft
启动内核。目标平台是 Linux / Windows / macOS，因此不依赖 KDE 框架，也不需要
合成器的模糊特效。

```
┌──────────────────────────────────────────────────────┐
│  ◈  mcl   文件   显示   窗口   帮助        周三 10月1日 20:32 │  ← 毛玻璃菜单栏
├──────────────────────────────────────────────────────┤
│                                                      │
│   ┌────────────────────────────────┐                 │
│   │ ● ● ●        启动器             │  ← 自绘窗口     │
│   ├────────────────────────────────┤  交通灯 / 拖拽  │
│   │  生存 1.20.1   Fabric    [启动] │  / 缩放 / 最小化│
│   │  原版 1.21     Vanilla   [启动] │                 │
│   └────────────────────────────────┘                 │
│                                          ▣   ▣       │
│              [　▣　▣　▣　▣　]  ← Dock（悬浮 + 放大）   │
└──────────────────────────────────────────────────────┘
```

## 现状

**第一期：架构 + 骨架，已完成。** 现在就能跑起来看到上面这个东西：

- 桌面外壳：壁纸（内置随包图片，可一键切系统壁纸）、菜单栏、Dock、桌面图标
- 菜单栏与 Dock 是**真毛玻璃 + 半透明**：透出的是背后那块壁纸的模糊版本（菜单栏会跟着壁纸从左到右变色），且不依赖 shader；两者**固定深色材质**，不跟随窗口外观（对应 macOS 的「菜单栏和 Dock 使用深色」）
- 菜单栏右侧是自绘的状态图标：Spotlight / 控制中心 / Wi-Fi / 电池（无图标字体、无外部资源）
- 窗口系统：交通灯、拖动（越不过菜单栏）、双击缩放、三处缩放热区、最小化、多层柔和投影
- Dock：图标放大 + 按压反馈 + 悬浮名称 + 运行指示点 + 分隔线 + 废纸荓；关窗口不退出应用（macOS 语义）
- **右键菜单**：桌面空白（切壁纸 / 切深色模式）、Dock 图标（显示所有窗口 / 退出）、桌面图标（打开 / 显示简介）
- 左键：桌面图标单击选中、双击打开，点桌面空白取消选中
- **自研启动内核**：mcl 自己读官方版本清单 → 下载客户端与库 → 解压 natives → 拼 java 命令行 → 起进程，游戏 stdout 转发到界面。不依赖 PrismLauncher，也不依赖任何第三方解压/网络库（只用 Qt + zlib）
- 已实测：离线账户启动原版 **1.12.2，一路跑到纹理图集构建完成**
- **实例**：实例 = 名字 + 版本 + 加载器 + 自己的游戏目录 + **自己的图标**。可以新建、改名、删除、排序，钉到 Dock
  - 图标按加载器区分：原版 = 草方块，Forge = 铁砧，NeoForge = 狐狸，Fabric = 布料；整合包用它在 Modrinth 上设的那个
- **启动台**：从 Dock 里的启动台图标铺开（没有全局快捷键），全屏展示所有实例；左键启动、右键改名/钉到 Dock/删除、按住拖动排序、拖到底部 Dock 区域即可钉上去。展开时**菜单栏和 Dock 依然可见**
- 桌面右键 →「新建实例」可以直接按版本建一个
- **启动台右键实例**：启动 / 重命名 / **实例设置** / **打开实例文件夹** / 钉到 Dock / 删除
- **实例设置**：改名字、看图标/版本/加载器、**指定用哪个 Java**、管理这个实例的模组（启用 / 停用 / 移除 / 检查更新），点「添加模组…」直接跳到模组市场并绑好这个实例
- **日志**：Linux 下照旧写 `stderr`（终端里直接看）；**Windows** 上没有控制台，
  所以落 `%LOCALAPPDATA%/mcl/mcl.log`，并顺带接管 Qt/QML 自己的警告消息，
  单文件超过 2MB 转成 `.1`
- **下载器**：并发下载 + 逐个 SHA1 校验；**校验与写盘都在线程池里**（不占主线程），先写 `.part` 再改名
- **模组加载器**：新建实例菜单里每个版本各带 `+ Fabric` / `+ Forge` / `+ NeoForge` 三行
  - **Fabric**：向 Fabric Meta 取 profile，落成一份 `inheritsFrom` 原版的版本描述，启动时由内核合并。**已实测 1.20.1 + Loader 0.19.5 加载成功**
  - **Forge / NeoForge**：安装过程含给客户端打补丁、生成 patched jar、注入 processors，自己实现不现实，所以走**跑官方 installer**：确保原版就绪 → 下 installer → 跑它 → 捡它生成的版本描述。**已实测 1.20.1 + Forge 47.4.23 安装并启动成功**（ModLauncher / FML 均正常）
- **下载源**：默认走 **BMCLAPI**，可选 **FastMCMirror** 或官方；用 `MCL_MIRROR=official` / `MCL_MIRROR=fastmcmirror` 切换
- **模组市场 / 整合包市场**：Dock 里的两个应用，数据来自 Modrinth（走 MCIM 镜像）。
  - 模组市场：搜索模组，可按**游戏版本 / 加载器**筛选；「装到…」选一个实例，或从实例设置进来直接装进那个实例
  - **装模组会自动把必需的依赖一起拉下来**（Modrinth 声明的 `required` 依赖，含依赖的依赖；`optional` / `incompatible` 不碰）
  - **资源包市场 / 光影市场**：同样来自 Modrinth，分别装到实例的 `resourcepacks/` 与 `shaderpacks/`
  - 整合包市场：搜索整合包 →「安装」→ 自动按它声明的加载器建实例、下载全部内容、解压 overrides
- **账户**（都在苹果标记的下拉菜单里）：
  - **微软账户**：走设备码流程 —— 启动器把 8 位码**自动复制到剪贴板**并**自己弹出浏览器**，
    切过去粘贴即可；码和网址也会留在账户窗口里，随时能重新打开或再复制一次
  - **离线账户**：填个名字即可（UUID 按 Mojang 的约定算）
  - 没登录也能启动，那就用离线身份；登录后自动改用真实凭据（`accessToken` / `userType: msa`）
- **Java 交给你自己挑**：不做 Java 自动下载 —— 自己选厂商、自己管版本本来就是玩 MC 的基本功。
  启动器会扫出本机所有 Java（带厂商，如 `Java 21 (Temurin)`），实例设置里可以为每个实例指定用哪个；
  不指定就按版本要求自动挑一个，挑不到或版本偏低会在日志里说清楚
分期计划见 [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md#6-分期)。

## 依赖

- CMake ≥ 3.21
- Qt ≥ 6.5（Core, Gui, Qml, Quick, QuickControls2）
- 打 `.deb` 需要 `dpkg-deb`；重新生成图标位图需要 `rsvg-convert`（librsvg2-bin）

## 构建

### Windows

Windows 包由 CI 出（`.github/workflows/build.yml`）：推到 GitHub 后，
windows runner 上编译 → 离屏跑一帧（截图存在才算 QML 真加载起来了）→
`cpack` 出 NSIS 安装包，产物在 Actions 的 artifact 里。

本地交叉编译是不行的 —— 这个仓库的构建环境里没有 mingw 工具链，
而且 Qt 的 Windows 版需要配套的编译器才能链接。

本机构建也可以，需要自行准备 MSVC + Qt 6.8 和 zlib（zlib 走 CMake 的 FetchContent 自动拉）。

## 构建与运行

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/mcl
```

`--windowed` 用系统窗口边框（方便调试），`--dark` 以深色外观启动，
`--shot out.png` 渲染一帧后退出（冒烟测试用），
`--launch <版本号>` 跳过鼠标操作直接装并启动某个版本（调试用，比如 `--launch 1.21`）。

内核会把版本、库、资源装到 `~/.local/share/mcl/`（标准 `.minecraft` 布局，
可以和官方启动器共用一份）。开发时如果想跳过几百 MB 的资源下载、只验证启动链路，
可以设 `MCL_SKIP_ASSETS=1`。

## 打包

### Debian / Ubuntu

```bash
./make-deb.sh                     # → build-pkg/mcl_<版本>_<架构>.deb
sudo apt install ./build-pkg/mcl_0.4.0_amd64.deb
```

包只装 mcl 本体，Qt6 运行库与 QML 模块由依赖自动补齐。**不依赖 prismlauncher 二进制** ——
这是 mcl 和 PMCL 的根本区别。

### Windows / macOS

CMake 已配好 `WIN32_EXECUTABLE` / `MACOSX_BUNDLE`，直接构建即可
（Windows 出 `.exe` 无控制台；macOS 出 `mcl.app`）。

## 目录

```
mcl/
├── src/
│   ├── main.cpp              装配：settings / desktop / kernel / engine
│   ├── shell/                桌面外壳：外观设置、毛玻璃底材、壁纸、窗口模型、桌面控制器
│   └── kernel/               启动内核接口 + 占位实现
├── qml/                      界面（全自绘，无 shader）
├── assets/                   内置素材（随包打进二进制）
│   ├── background.jpg        桌面壁纸
│   ├── apple-logo.png        菜单栏左上角的 logo（已预处理成纯白 + 保留 alpha）
│   └── source/               原始素材
├── icons/                    应用图标与线性图标
├── docs/ARCHITECTURE.md      架构说明（**先读这个**）
├── packaging/                deb 维护者脚本、图标生成脚本
└── make-deb.sh
```

## 与 PMCL 的关系

PMCL（`../PMCL`）是"PrismLauncher 的前端"：扫它的实例、调它的命令行。

**mcl 是独立应用**：自带桌面外壳，内核**完全自研** —— 自己读官方版本清单、
并发下载并逐个校验 SHA1、解压 natives、拼 java 命令行、拉起游戏进程。
只依赖 Qt 和 zlib，**不依赖 PrismLauncher，也不调用它的二进制**。
（最初打算复用 PrismLauncher 的启动核心源码，调研后发现关键路径上离不开
`libarchive` / `tomlplusplus` 这些依赖，于是改为自研。）

PMCL 保留原样，可继续使用。

## 许可

mcl 自身代码随项目发布。内核接入 PrismLauncher 源码时需遵循其 GPL-3.0
（`launcher/` 主体为 GPL-3.0-only；`libraries/launcher` 为 GPL-3.0 with linking exception）。
