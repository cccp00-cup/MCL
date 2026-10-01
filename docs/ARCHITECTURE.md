# mcl 架构

> mcl = **一个标准尺寸的窗口，窗口内部自绘一整套 macOS 12 桌面，并内置 Minecraft 启动内核。**
>
> 交互按 macOS 12 还原（菜单栏 / Dock / 自绘窗口 / 交通灯 / 拖拽 / 缩放），
> 代码保持跨平台（Linux / Windows / macOS），内核**复用 PrismLauncher 的启动核心源码，
> 但不依赖也不调用它的二进制**。

本文是第一期（骨架）定下来的架构。骨架已经能跑：窗口内可见 macOS 12 桌面，
可以点 Dock 开窗口、拖标题栏移动、绿灯最大化、黄灯最小化、红灯关闭、双击标题栏缩放。

---

## 1. 分层

```
┌──────────────────────────────────────────────────────────────┐
│  shell（桌面外壳）                                            │
│  壁纸 · 菜单栏 · Dock · 自绘窗口 · 窗口管理器 · 外观设置        │
│  C++: ShellSettings / GlassProvider / DesktopController /     │
│       WindowModel        QML: Desktop / ShellMenuBar / Dock / │
│       DockIcon / McWindow / DropMenu / Theme                  │
├──────────────────────────────────────────────────────────────┤
│  apps（桌面里的应用，即窗口内容）                              │
│  QML: AppLauncher / AppConsole / AppSettings / AppAbout       │
│  只跟 McKernel 抽象打交道，不认识 PrismLauncher               │
├──────────────────────────────────────────────────────────────┤
│  kernel（启动内核）                                           │
│  C++: McKernel（接口） / StubKernel（占位） /                  │
│       后续 PrismKernel（接 PrismLauncher 源码）               │
└──────────────────────────────────────────────────────────────┘
```

**依赖方向单向向下**：apps 依赖 kernel 的接口，shell 不依赖 kernel，kernel 不依赖任何界面代码。
`src/kernel/McKernel.h` 里不出现任何 Prism 类型 —— 内核怎么演进，界面层都不用改。

---

## 2. 目录结构

```
mcl/
├── CMakeLists.txt          # Qt6 + QML 模块 + 安装 + CPack DEB
├── make-deb.sh             # 一键出 .deb
├── mcl.desktop
├── packaging/
│   ├── debian/postinst     # 安装后刷新 desktop / 图标缓存（容错）
│   └── update-icons.sh     # 从 icons/app.svg 重生成多尺寸 PNG
├── icons/                  # app.svg（应用图标）+ 线性图标
├── docs/ARCHITECTURE.md    # 本文
├── src/
│   ├── main.cpp            # 装配：settings / desktop / kernel / engine
│   ├── shell/
│   │   ├── ShellSettings.*      # 外观参数 + QSettings 持久化
│   │   ├── GlassProvider.*      # image://mcl/wallpaper|glass|noise|glow
│   │   ├── Wallpaper.*          # 跨平台系统壁纸探测（搬自 PMCL）
│   │   ├── WindowModel.*        # 窗口列表（QAbstractListModel）
│   │   └── DesktopController.*  # 时钟 / Dock 应用 / 窗口生命周期
│   └── kernel/
│       ├── McKernel.h           # 内核接口（无 Prism 类型）
│       └── StubKernel.*         # 第一期占位实现
└── qml/
    ├── Theme.qml           # 设计令牌（singleton）
    ├── Desktop.qml         # 根 Window，组装 5 层
    ├── ShellMenuBar.qml    # 菜单栏
    ├── DropMenu.qml        # 下拉菜单
    ├── Dock.qml            # Dock 容器 + 放大计算
    ├── DockIcon.qml        # 单个 Dock 图标
    ├── McWindow.qml        # 自绘窗口 + 交通灯 + 拖拽 + 缩放
    └── App*.qml            # 各应用内容
```

---

## 3. 关键设计

### 3.1 毛玻璃不依赖合成器，也不需要 shader

`GlassProvider` 在 C++ 侧用 `QPainter` 把壁纸**一次性**处理成两张位图并缓存：

| URL | 内容 | 用途 |
| --- | --- | --- |
| `image://mcl/wallpaper/<key>` | 清晰壁纸（内置随包图片兜底） | 桌面背景 |
| `image://mcl/glass/<key>` | 模糊 + 调色 + 暗角 + 颗粒 | 菜单栏 / Dock / 窗口材质 |
| `image://mcl/glassrect/<x>/<y>/<w>/<h>/<radius>/<dw>/<dh>/<key>` | 从上一张里裁一块并切圆角 | Dock / 菜单栏（圆角必须在 C++ 侧做，见下） |
| `image://mcl/noise/<size>` | 可平铺颗粒 | 磨砂质感叠加 |
| `image://mcl/glow/<size>` | 径向柔光 | 悬浮高光 |

内置素材（`assets/`）都是**预处理过的**，运行时不做图像加工：

- `background.jpg` —— 原图是 2560×1440 的 AVIF（Qt 默认解不了），转成 q90 JPEG 后 184KB；
  做成 JPG 而非 PNG 是因为它是照片类图像（4.6 万色），PNG 要 808KB 而压缩痕迹会被模糊吃掉。
- `apple-logo.png` —— 原图 803×985 带 alpha 但本体是灰色，处理成**保留 alpha、RGB 推满纯白**，
  于是它和菜单栏其他文字同色，切深浅模式都不用换资源。

**关键在于"按桌面坐标裁切"**：菜单栏和 Dock 把整张 `glass` 图按桌面尺寸摆好，
各自只露出自己那一条 —— 于是它们显示的就是**背后那块壁纸的模糊版本**，
和壁纸切换、窗口移动天然对齐。代价是一张 1600×1000 的图，收益是
**不依赖 KWin/Mutter/DWM 的模糊特效，Windows 与 macOS 上行为一致**。

在这层之上再叠**一张很薄的深色色调**（`Theme.chromeTint`，alpha 0.34），
所以菜单栏和 Dock 是**半透明的** —— 能透出背后壁纸的颜色（菜单栏从左到右跟着壁纸
从紫到橙），而不是一条死白的条。透明度就是这一个数字：调大更实、调小更透。

**这两个部件固定深色，不跟随窗口的深浅模式** —— 对应 macOS 的「菜单栏和 Dock 使用深色」。
好处是无论壁纸多亮、窗口是浅色还是深色，文字（固定浅色）的对比度都稳定。
窗口、桌面图标仍然跟随 `shellSettings.darkMode`，两套外观相互独立。

**Dock 的圆角是 C++ 侧裁出来的**（`image://mcl/glassrect/<x>/<y>/<w>/<h>/<radius>/…`），
不是在 QML 里套 `radius`：`Rectangle.radius` 不影响子项裁剪（`clip` 只裁矩形），
在 QML 里给容器加圆角，子项会把四个角重新填成直角。这条路顺带只取需要的区域，
也避开了把整张 1600×1000 底图缩放到桌面尺寸造成的模糊。

`<key>` 是 `ShellSettings::appearanceKey()`（壁纸路径 + 模糊/暗度/饱和度/颗粒）。
它只用来改 URL，从而让 Qt 的 `Image` 缓存失效；真实参数由 provider 从 settings 读。

### 3.2 窗口不是真窗口

`McWindow` 是桌面内部自绘的一块矩形，状态全部记在 `WindowModel`：

```
id / appId / title / geometry / restoreGeometry / minimized / maximized / z / focused
```

好处：

- 层级、层级提升（`raise`）、最小化全都自己说了算，不受窗口管理器干扰
- 几何状态天然可序列化 → 后续"重开恢复上次桌面布局"只需存一份 model
- 跨平台一致（不依赖各平台窗口管理器行为）

`WindowModel` 是 `QAbstractListModel`，**只对变化的行发 `dataChanged`**。
这点很重要：如果改用 `QVariantList` 属性，每次拖动都会让 `Repeater` 整体重建 delegate，
拖拽会被打断。

与 macOS 一致的交互（已在 `McWindow.qml` 实现）：

| 操作 | 行为 |
| --- | --- |
| 拖标题栏 | 移动窗口，`posY` 被 `minTop`（菜单栏下沿）夹住，越不过菜单栏 |
| 双击标题栏 | 缩放（最大化到工作区，避开菜单栏与 Dock） |
| 红灯 | 关窗口，**但应用仍在 Dock 上亮着**（macOS 语义）—— 再点 Dock 图标会重开窗口 |
| 黄灯 | 最小化（淡出，Dock 图标保留运行点） |
| 绿灯 | 最大化 / 还原 |
| 右下角 / 右边缘 / 下边缘 | 缩放 |

拖动期间位置由窗口自己持有（局部 `posX/posY`），**松手才写回 model**
（`geometryCommitted` 信号）—— 避免拖动时每帧写 model 造成绑定回环。

### 3.3 启动内核（自研）

内核是 **mcl 自己的**：不依赖 PrismLauncher，也不把启动转交给别的程序。

一开始的方案是复用 PrismLauncher 的启动核心源码，但调研后发现它需要 `libarchive`、
`tomlplusplus`、`cmark`、`libqrencode`、`Qt6 NetworkAuth` 等一批系统库，其中前两个
（解压、配置解析）在关键路径上裁不掉。于是改成自研 —— 只依赖 Qt 与 zlib，
跨平台也更干净。

**整条链路**（都在 `src/kernel/`）：

```
version_manifest_v2.json   ← 官方清单（900+ 个版本）
        ↓ 选定版本
versions/<id>/<id>.json    ← 该版本的元数据：库、natives、assetIndex、mainClass、参数
        ↓ 按 rules 过滤当前平台
client.jar + libraries/*   ← 并发下载，逐个 SHA1 校验，先写 .part 再改名
        ↓
assets/indexes/<id>.json   ← 资源索引
assets/objects/xx/xxxxxx   ← 上千个资源对象（几百 MB，最慢的一步）
        ↓
natives/<id>/              ← 解压 natives 包
        ↓
java -cp … <mainClass> …   ← 挑一个满足 javaVersion 的 JDK，拼出完整命令行
        ↓
QProcess                   ← 起进程，stdout/stderr 转发到界面
```

**只靠 Qt + zlib 是怎么做到的**：

| 需要的能力 | 做法 |
| --- | --- |
| HTTPS 下载 | `QNetworkAccessManager`（重定向、并发、`downloadProgress`） |
| 完整性校验 | `QCryptographicHash` —— Mojang 给每个文件都提供 SHA1 |
| 解压 natives | 自己写的 zip 读取（`ZipExtract`）：central directory + local header + zlib 的 raw inflate；只支持 stored/deflate，足够覆盖全部 natives 包 |
| 找 Java | 扫 `JAVA_HOME` / `PATH` / `/usr/lib/jvm` / sdkman / `~/.jdks` / macOS 的 JavaVirtualMachines；先读 `release` 文件，读不到才真跑一次 `java -version` |
| 规则求值 | `McRule` 完整实现 Mojang 的 `rules` 语义：**最后一条匹配的规则说了算**；库默认 allow、参数默认 deny |
| 离线账户 | UUID = MD5(`OfflinePlayer:<名字>`)，再按 version-3 规范改两个半字节 |

**数据目录** `~/.local/share/mcl/` 用的就是标准 `.minecraft` 布局（`versions/`、
`libraries/`、`assets/`），所以库和资源可以和官方启动器共用一份。

**版本继承（`inheritsFrom`）**：模组加载器给出的 `version.json` 都只是薄壳，
例如 Fabric 的 profile 只有 `mainClass: …KnotClient`、`inheritsFrom: 1.20.1` 和 8 个自己的库。
启动前 `parseVersionJson()` 会顺着 `inheritsFrom` 一路往上合并：
`mainClass`/`assets`/`javaVersion` 取子版本，库以父版本打底、子版本同名的覆盖，
命令行参数父在前子在后。`resumeInstall()` 会先检查整条父链的描述是否都在，
缺哪个就先去下载它。

**下载器**（`Downloader`）三条要点：

| 关注点 | 做法 |
| --- | --- |
| 不卡界面 | SHA1 校验与写盘都丢进 `QThreadPool`，主线程只做事件调度 |
| 中断安全 | 先写 `<目标>.part`，校验通过再改名；任务自带 generation 号，cancel/重来之后旧回调会被丢弃 |
| 卡死兜底 | 用**停滞检测**而不是 `setTransferTimeout`——后者是总时长超时，会把 client.jar 这种大文件在慢链路上直接砍掉。只掐"45 秒没有新数据"的请求，然后单文件重试最多 3 次 |

**下载源（`Mirror`）**：默认走 BMCLAPI 镜像。要点是 **BMCLAPI 不会重写它返回内容里的 URL** ——
清单和 version.json 里的下载地址仍然是 `piston-data.mojang.com`、`libraries.minecraft.net`
这些官方域名，所以替换必须在"准备下载项"这一步做（`LaunchPlan` 里 4 处、`VanillaKernel` 里 5 处）。
映射规则：

| 官方 | 镜像 |
| --- | --- |
| `launchermeta` / `piston-meta` / `piston-data`.mojang.com | `bmclapi2.bangbang93.com`（路径不变） |
| `libraries.minecraft.net/<path>` | `bmclapi2.bangbang93.com/maven/<path>` |
| `resources.download.minecraft.net/<ab>/<hash>` | `bmclapi2.bangbang93.com/assets/<ab>/<hash>` |
| `meta.fabricmc.net/...` | `bmclapi2.bangbang93.com/fabric-meta/...` |

六个端点都验过 HEAD 200。`MCL_MIRROR=official` 可切回官方源。

**模组加载器的三个坑**（都是实测踩出来的）：

1. **库的下载信息格式不一样**。Mojang 的库有 `downloads.artifact.{path,url,sha1,size}`，
   而 Fabric 只给 `name` + 一个仓库根地址 —— 得按 Maven 约定自己推
   `group/artifact/version/artifact-version[-classifier].jar`。
2. **natives 的写法变过**。1.18 及更早靠 `natives` 字段指定当前平台的 classifier；
   **1.19 起 natives 是独立的库条目**，名字里带 `:natives-linux` 这种分类器，平台过滤
   交给 `rules`。只认旧写法的话，这些包会被当成普通库塞进 classpath，而且一个都不解压。
3. **`javaVersion` 缺省的坑**。模组加载器的描述里没有 `javaVersion` 字段，如果解析时
   默认成 8，就会**覆盖**父版本要求的 17，结果拿 Java 8 去跑 1.20.1。现在用 0 表示
   "没写"，合并时子版本只有在写了的情况下才覆盖父版本；启动时再兜底成 8（1.16 及更早
   确实要 Java 8）。

**Forge / NeoForge**：和 Fabric 的路子完全不同 —— 它们的安装包含**给客户端打补丁、生成
patched jar、注入 processors**，自己实现不现实，所以 mcl 的做法是**跑官方 installer**：

```
确保原版就绪（installer 要拿它打补丁）
   → 从 maven-metadata 挑适配该游戏版本的最新加载器
   → 下 installer
   → 预置 launcher_profiles.json（installer 会检查它）
   → java -jar <installer> --installClient <共享数据目录>
   → 捡它生成的 versions/<id>/ 建实例
```

实测 1.20.1 + Forge 47.4.23 装完能直接在 mcl 里启动（ModLauncher / FML 正常）。
注意 **installer 内部用的是它自己的下载地址**，不走我们的镜像。

**模组 / 整合包市场**：数据来自 Modrinth，走 **MCIM 镜像**
（官方 `api.modrinth.com` 国内基本连不上，本机还被 TLS 拦）。两处映射：

| 用途 | 官方 | 镜像 |
| --- | --- | --- |
| API | `api.modrinth.com/v2/...` | `mod.mcimirror.top/modrinth/v2/...` |
| 文件 | `cdn.modrinth.com/data/...` | `mod.mcimirror.top/data/...` |

文件镜像**路径完全一致，只换域名**。`.mrpack` 和 index.json 里列的每个文件都要过这一层，
否则会在 SSL 握手上失败（证书主机名不匹配）。

**整合包安装**（`.mrpack` 就是 zip）：

```
下载 .mrpack → 解压读 modrinth.index.json
   → 从 dependencies 看出要哪个原版 + 哪个加载器
   → 按加载器走已有的建实例流程（原版 / Fabric / Forge）
   → 实例建好后，下载 index 里列的全部文件（保持 path 结构）
   → 最后把 overrides/ 与 client-overrides/ 覆盖进实例目录
```

实例建好之后要继续装内容，所以三个建实例的出口（原版 / Fabric / Forge）统一走
`afterInstanceReady()` —— 早先只在 Forge 那条路上加了钩子，导致整合包走到一半就停了。

**实例图标**：按加载器分（原版草方块 / Forge 铁砧 / NeoForge 狐狸 / Fabric 布料），
整合包用它在 Modrinth 上的 `icon_url`。图标串存在 `instances.json` 的 `icon` 字段里，
空则按加载器取默认 —— 所以旧实例不需要迁移。

**依赖自动下载**：Modrinth 的版本带 `dependencies[]`，只处理 `dependency_type == "required"`
的（`optional` 是可选功能、`incompatible` 是要避开的）。依赖也有自己的依赖，所以用
`QSharedPointer<int>` 做计数器递归，全部收集齐了再一次性下载 —— 不能边收集边下，
否则会把启动流程的 `Downloader` 打断（它会 `cancel()` 掉上一批）。

**更新检查**：按文件 SHA1 调 Modrinth 的 `/version_files/update`（要 POST 一个哈希数组），
把 `mods/` `resourcepacks/` `shaderpacks/` 都算一遍。返回的 `文件名 == 新文件名` 视为没更新。

**Java 的选择权在用户手上**：mcl **不做** Java 自动下载 —— 自己挑厂商、自己管版本是玩 MC 的
基本功。`JavaLocator` 扫出本机所有 Java（读 `release` 文件拿版本与厂商，读不到才跑一次
`java -version`），实例可以用 `javaPath` 钉死用哪个；没钉就按 `javaVersion.majorVersion`
自动挑，且版本偏低时只警告不阻止（有些整合包就是要跑旧 Java）。

**账户**：离线 + 微软，入口都在苹果标记的下拉菜单里（和 macOS 一样）。

微软走的是**设备码流程**（device code flow）：启动器拿到一个 8 位码和一个网址交给用户，
用户在**自己的浏览器**里输码，启动器轮询换 token。
好处是跨平台一致、不用起本地 HTTP 服务、也不用处理回调端口被占。

完整链路（每步的错误都翻了人话）：

```
oauth20_connect.srf 申请设备码 → 轮询 oauth20_token.srf → Xbox Live 认证 → XSTS 授权
  → Minecraft 登录（拿 MC access token）→ /minecraft/profile（拿 uuid 和玩家名）
```

**这条路上有两个坑，都会表现成"申请设备码失败"：**

**一、端点用错。** `00000000402b5328` 是 **MSA（`login.live.com`）** 的客户端，
不是 Azure AD 的。拿它去请求 `login.microsoftonline.com/consumers/oauth2/v2.0/devicecode`
会被直接拒掉：

```
AADSTS700016: Application with identifier '00000000402b5328'
               was not found in the directory '9188040d-...'
```

正确端点是 **`https://login.live.com/oauth20_connect.srf`**，而且必须带上
`response_type=device_code`（少了这个参数，`login.live.com` 会按别的流程解释请求）。
参考实现是 prismarine-auth 的 `LiveTokenManager`。

**二、不能指望本地回调。** `http://localhost:PORT` 没有注册 —— 传过去只会拿到一个
标题为 "Microsoft account" 的错误页（注册了的话标题会是 "Sign in to Minecraft"）。
这个 client_id 只注册了 `https://login.live.com/oauth20_desktop.srf`，
而那个回调会停在微软自己的页面上，得让用户手动复制地址栏。
设备码流程压根没有 `redirect_uri`，正好绕开这个限制 —— 这也是 HMCL、Modrinth
这些启动器选它的原因。

> 想用授权码 + `http://127.0.0.1:PORT` 回环回调的话，前提是**自己注册一个 Azure AD 应用**，
> PrismLauncher 就是这么做的（它有自己的 client id 和 `QOAuthHttpServerReplyHandler`）。

XSTS 的那几个 `XErr` 值得单独翻译，不然用户只会看到一句 HTTP 400：
`2148916233` = 没开 Xbox 档案、`2148916235` = 地区不支持、`2148916238` = 未成年需加家庭组。

用的是 Xbox Live 的公开客户端 id `00000000402b5328` —— OAuth 的公开客户端不靠 secret 保密，
社区里的第三方启动器普遍在用这一个。

轮询的两条规则：`authorization_pending` **不算错误**（用户还没点完，下一拍接着问），
`slow_down` 则要**把间隔调长**（服务端嫌问得太勤）。`expires_in` 是 900 秒，
超时就把状态收干净、提示重新来一次。

**给用户的动作**：拿到设备码后，`copyDeviceCode()` 把码写进剪贴板，同时
`QDesktopServices::openUrl()` 直接把登录页弹出来 —— 和大多数 Minecraft 启动器一样，
用户切过去按一下粘贴就行，不用在两块屏之间手抄。剪贴板写入全部判空
（离屏跑冒烟测试时压根没有剪贴板），并且**在 Linux 上顺手写主选区**，中键一贴就有。
浏览器没弹出来（无桌面环境、或被关了）时状态栏会改成"请手动打开"，账户窗口里也留了
「重新打开浏览器」和「复制设备码」两个按钮兜底。

**令牌续期**：`refresh_token` 会连同 access token 一起存下来
（`m_expiresAtMs` 记到期时刻）。续期复用 `oauth20_token.srf`，换
`grant_type=refresh_token`，拿到新的 MSA 令牌后直接接回同一条认证链。

**日志出口（`src/Log.{h,cpp}`）**：分平台是有理由的，不是为了优雅。

Linux 下程序从终端启动，`stderr` 抬眼就看见，所以 `install()` 是**空操作**、
`line()` 照旧 `fprintf` —— **行为跟以前一模一样**。

Windows 下可执行文件是 `WIN32_EXECUTABLE`，**压根没有控制台**，`stderr` 写进去没人看。
那边改成落文件（`%LOCALAPPDATA%/mcl/mcl.log`），并且用 `qInstallMessageHandler`
把 Qt / QML 自己的警告也接过来 —— QML 报的绑定错误、加载失败在 Windows 上同样看不见，
不接管就真的两眼一抹黑。文件带毫秒时间戳、超过 2MB 转成 `.1`（只留一代）。

**一个性能坑**：`availableVersions()`（九百多条版本）**必须缓存**。
它最初每次调用都重建一遍 `QVariantList`，单次约 9ms 看着无所谓，
但 QML 里 `var` 属性的绑定会被**反复求值** —— 实测打开「新建实例」窗口
触发了上千次，累计吃掉 15 秒 CPU（22s → 5.6s）。现在只在清单变化时重建。
同理，QML 侧的筛选/搜索放在绑定的 `readonly property` 里算一次就好，
别在 delegate 或 `onXChanged` 里重复遍历。

**CI（`.github/workflows/build.yml`）**：两个 job。
`windows`（MSVC + Qt 6.8.2，runner 自带 MSVC，省掉"CMake 挑错编译器去链 mingw 版 Qt"的麻烦）
负责**把 Windows 真的跑一遍** —— 编译、离屏跑一帧、把 `mcl.log` 打出来、出 NSIS 包。
`linux` 是回归，`cpack -G DEB` 仍然出 deb。之所以要做 windows job，是因为
mcl 从头到尾在 Linux 上开发，Windows 那边没编译过也没运行过，靠读代码猜"应该没问题"不够。

Windows 的 zlib 走 `FetchContent` 拉源码（那边没有系统 zlib，Qt 也不对外导出自带的那个）；
`.rc` 资源把图标和版本信息编进 `mcl.exe`；`qt_generate_deploy_app_script` 在 `install`
阶段跑 `windeployqt`，把 Qt 的 dll 和 QML 插件收进安装目录 —— 不跑这个，用户双击就是
"缺少 Qt6Core.dll"。

**Windows 支持现状**：平台相关的地方基本都写到了（classpath 分隔符、`java.exe`、
注册表读壁纸、`WIN32_EXECUTABLE`、`rename` 前先 `remove`、rules 里的 Windows natives、
install/CPack 规则限在 `if(UNIX AND NOT APPLE)`），全树没有 POSIX-only 调用、
没有硬编码家目录。**但从未在本机验证过 Windows 运行** —— 打包链路（NSIS + `windeployqt`）已经补上，
交给 CI 的 windows job 去编译和冒烟。仍需实机确认的几处：260 字符路径上限、
杀毒软件造成的文件占用、无边框窗口在 Windows 上的行为差异、高 DPI 下的自绘坐标。

**尚未实现**：Quilt。当前是**离线/微软账户 +
原版 / Fabric / Forge + 模组/整合包/资源包/光影四个市场**。

> 备查：PrismLauncher 11.1.0 的源码复用调研结论 —— 它的启动核心本来就是静态库
> `Launcher_logic`、对 KF6/KF5 零依赖，真正的耦合点只有全局单例 `APPLICATION`、
> `LaunchController` 的对话框、`BaseInstance` 的 `QMenu` 纯虚接口。若将来要改走那条路，
> 这份结论仍然成立。
### 3.4 内核接口

```cpp
class McKernel : public QObject {
    Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)

    virtual QString name() const = 0;
    Q_INVOKABLE virtual void refresh() = 0;
    Q_INVOKABLE virtual void launch(const QString &instanceId) = 0;
    Q_INVOKABLE virtual void launchOffline(const QString &instanceId, const QString &playerName) = 0;

signals:
    void instancesChanged();
    void logLine(const QString &line);
    void launchStarted(const QString &instanceId);
    void launchFailed(const QString &instanceId, const QString &error);
    void launchFinished(const QString &instanceId, int exitCode);
};
```

实例用 `QVariantMap` 传递（`id/name/version/loader/dir/running`），避免在接口层引入 metatype，
也避免界面层看见内核的数据结构。`StubKernel` 既是第一期占位，也可长期作为回归测试替身。

---

## 4. 跨平台策略

| 关注点 | 做法 |
| --- | --- |
| 窗口 | 单窗口 + `Qt.FramelessWindowHint`；`--windowed` 退回系统边框便于调试 |
| 毛玻璃 | C++ 预渲染位图，**零 shader、零合成器接口** |
| 系统壁纸 | `Wallpaper::detect()`：Linux 走 Plasma 配置 → `/proc` 扫描 → `gsettings` → `/usr/share/wallpapers`；Windows 走注册表；macOS 走 `osascript`。取不到就用内置 macOS 12 风格壁纸 |
| 全局快捷键 | **第一期不做**。（launchpad 用的是 KF6 GlobalAccel，Plasma 专用，不能照搬；跨平台需另找方案：X11 用 XGrabKey、Wayland 走桌面环境接口、Windows 用 RegisterHotKey） |
| 打包 | Linux → CPack DEB；Windows/macOS 走 CMake 的 `WIN32_EXECUTABLE` / `MACOSX_BUNDLE` |

---

## 5. 本期踩过的坑（后续务必注意）

1. **QML 里不要用 8 位十六进制颜色字符串**（`"#ffffffcc"`）。
   本机实测它渲染出的颜色完全不是预期值（菜单栏整条变成纯蓝），
   换成 `Qt.rgba(r, g, b, a)` 立刻正常。全项目已统一为 `Qt.rgba`。
2. **不要用 `MenuBar` 当组件名** —— 和 `QtQuick.Controls.MenuBar` 重名，
   会静默解析到控件的类型上（子项被塞进 `contentItem`，`anchors.fill: parent` 失效）。
   现已改名 `ShellMenuBar`。
3. **`Item::z` 是 FINAL**，不能作为 `required property` 从 model 覆盖；
   model role 已改名 `stackZ`。
4. **`anchors.fill: parent` 不能和显式 `width`/`height` 同时用**，并且要防循环依赖。
   Dock 里 `dock.width: background.width` + `background.anchors.fill: parent` 就是死循环，
   已改为 `anchors.centerIn: parent` + 显式尺寸。
5. **`Rectangle.radius` 不会裁剪子项**（`clip` 只裁矩形）。给容器加圆角后，
   里面的 `Image`/`Rectangle` 会把四个角重新填成直角；而且子项自己也要加同样的 `radius`。
   Dock 的圆角最终是在 C++ 侧裁的（`glassrect`，见 3.1）。
5. **`easing:` 要用 `easing.type:`**；`NumberAnimation on x` 里的 `easing` 直接赋 `Easing.OutCubic` 会报
   `Unable to assign int to QEasingCurve`。
6. **`Row`/`Column` 里不要放引用父容器宽度的 `MouseArea`** ——
   它参与布局、宽度又依赖布局结果，会触发 `polish()` 死循环。
7. `QQuickImageProvider` **不是 `QObject`**，加不了 `Q_PROPERTY`；参数靠注入（`ShellSettings*`）。
8. `Q_PROPERTY` 里用到的指针类型必须在头文件里**完整定义**（moc 需要），
   只前置声明会报 `Meta Types must be fully defined`。
9. 离屏渲染请用 `QT_QPA_PLATFORM=offscreen` 且**输出到可执行的分区**
   （`/tmp` 在本机是 `noexec`，会直接 `ENOENT`）。
10. **命令行参数的规则默认值是"不适用"**。`arguments.game` / `arguments.jvm` 里
    **没有 `rules` 的元素是无条件适用的**，如果一律写成
    `McRule::evaluate(rules, features, false)`，那些元素会被整批丢掉
    （症状：游戏报 `Missing required option(s) [accessToken, version]`，
    而日志里的命令行"一个游戏参数都没有"）。只有 `rules` 非空时才需要求值。
11. **`Downloader` 的完成判定不能只写在网络回调里**。全部文件都"已存在、校验通过 →
    跳过"这条路径不经过回调，会导致第二次启动永久卡在等 `finished`。
12. **`-cp` 与 `-Djava.library.path` 只在旧版本补**。1.13+ 的 `jvm` 数组里已经带了
    这两项，自己再加一次会重复一个 `-cp`。

## 6. 内核踩过的坑（补充）

13. 用 `QNetworkAccessManager` 做后台预取时**不要占用启动流程的 `Downloader`** ——
    否则用户一点启动就撞上"下载器正忙"直接失败。两者走各自通道，启动请求在
    下载器忙时排队，等当前批次结束再继续。

---

## 6. 分期

| 期 | 内容 | 状态 |
| --- | --- | --- |
| 一 | 架构 + 骨架：桌面外壳、窗口系统、内核接口占位、deb 管线 | **已完成** |
| 二 | 自研启动内核：清单 → 下载 → natives → 拼命令行 → 起进程（离线账户 + 原版） | **已完成** |
| 三 | 账户（微软 OAuth）、模组加载器、Java 自动下载 | 待做 |
| 四 | 桌面还原度：Launchpad、Spotlight、Mission Control、通知中心、多桌面 Spaces、窗口几何持久化 | 待做 |

## 7. 已知差距

- 桌面图标不可拖动、不能新建文件夹 / 重命名（选中态与右键菜单已有）
- 控制中心点开没有面板；Spotlight 面板、通知中心、Mission Control、多桌面 Spaces 未实现
- 窗口没有"最小化到 Dock 的 genie 动效"；阴影是多层描边环近似，不是高斯模糊
- Dock 图标放大只缩放自身，没有把邻居推开的完整 magnify
- 废纸篓是占位，点它没有动作
- 没有全局快捷键呼出

## 8. 交互清单（已实现）

| 区域 | 左键 | 右键 |
| --- | --- | --- |
| 菜单栏 | 菜单项下拉（真可用的：关于本机 / 系统设置 / 深色模式 / 关闭窗口）；空白处拖动移动整个窗口 | — |
| 启动台 | 铺满所有实例；单击图标启动该实例；点空白关闭 | 图标上右键：启动 / 重命名 / 实例设置 / 打开实例文件夹 / 钉到 Dock / 删除 |
| 启动台（拖拽） | 按住图标拖动 → 松手按落点重排；拖到底部 Dock 区域松手 → 钉到 Dock | — |
| Dock（实例） | 启动该实例 | 启动 / 从 Dock 移除 |
| Dock（启动台图标） | 展开或收起启动台 | — |
| Dock 图标 | 启动或切到该应用；未运行时会新建窗口 | 显示所有窗口 / 新建窗口 / 退出 |
| 桌面图标 | 单击选中，双击打开 | 打开 / 显示简介 / 移到废纸篓（置灰） |
| 桌面空白 | 取消选中 | 切壁纸（内置 / 跟随系统）、切深色模式、显示视图选项 |
| 窗口 | 标题栏拖动、双击缩放、点任意处置顶；交通灯 = 关闭 / 最小化 / 最大化 | — |
