cmake_minimum_required(VERSION 3.21)

project(mcl
    VERSION 0.3.0
    DESCRIPTION "macOS 12 style desktop shell with a Minecraft launcher kernel"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 Network
)

# zlib 只有 ZipExtract 用（解压 natives 和 installer）。
# Windows 上没有系统 zlib，Qt 也不对外导出它自带的那个，所以那边拉一份源码来编；
# Linux/macOS 继续用系统库，不改变原来的构建方式。
if(WIN32)
    include(FetchContent)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    # 关键：不要 zlib 的 install 规则。
    # CPack 会把所有 install() 都收进包里，包括 FetchContent 拉来的子项目 ——
    # 于是打 NSIS 时去跑 zlib-build/cmake_install.cmake，撞上它内部的组件检查直接报错。
    # 开关名是 zlib 沿用 CMake 约定的 SKIP_INSTALL_ALL（不是 ZLIB_INSTALL，
    # 那是我一开始猜错的，改了没用）。
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(zlib)
    set(MCL_ZLIB_TARGET zlibstatic)
else()
    find_package(ZLIB REQUIRED)
    set(MCL_ZLIB_TARGET ZLIB::ZLIB)
endif()

qt_standard_project_setup(REQUIRES 6.5)

# ---------------------------------------------------------------- 可执行文件
qt_add_executable(mcl
    src/main.cpp

    src/Log.cpp
    src/Log.h

    src/shell/Wallpaper.cpp
    src/shell/Wallpaper.h
    src/shell/GlassProvider.cpp
    src/shell/GlassProvider.h
    src/shell/ShellSettings.cpp
    src/shell/ShellSettings.h
    src/shell/WindowModel.cpp
    src/shell/WindowModel.h
    src/shell/DesktopController.cpp
    src/shell/DesktopController.h

    src/kernel/McKernel.h
    src/kernel/AccountManager.cpp
    src/kernel/AccountManager.h
    src/kernel/InstanceStore.cpp
    src/kernel/InstanceStore.h
    src/kernel/Mirror.cpp
    src/kernel/Mirror.h
    src/kernel/ModLoader.cpp
    src/kernel/ModLoader.h
    src/kernel/ModrinthClient.cpp
    src/kernel/ModrinthClient.h
    src/kernel/McVersion.cpp
    src/kernel/McVersion.h
    src/kernel/ZipExtract.cpp
    src/kernel/ZipExtract.h
    src/kernel/Downloader.cpp
    src/kernel/Downloader.h
    src/kernel/JavaLocator.cpp
    src/kernel/JavaLocator.h
    src/kernel/LaunchPlan.cpp
    src/kernel/LaunchPlan.h
    src/kernel/VanillaKernel.cpp
    src/kernel/VanillaKernel.h
    src/kernel/StubKernel.cpp
    src/kernel/StubKernel.h
)

target_include_directories(mcl PRIVATE src)

target_link_libraries(mcl PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    ${MCL_ZLIB_TARGET}
)

# ---------------------------------------------------------------- QML 模块
# 源文件放在 qml/ 下保持目录整洁，但用 QT_RESOURCE_ALIAS 摊平到模块根，
# 这样 main.cpp 的 loadFromModule("Mcl", "Desktop") 才能找到 Desktop.qml。
set(MCL_QML_FILES
    Theme
    Desktop
    ShellMenuBar
    StatusIcons
    Launchpad
    MarketView
    McMenu
    Dock
    DockIcon
    McWindow
    AppLauncher
    AppModMarket
    AppPackMarket
    AppInstanceSettings
    AppResourceMarket
    AppShaderMarket
    AppAccount
    AppConsole
    AppSettings
    AppAbout
    AppNewInstance
)

foreach(_qml IN LISTS MCL_QML_FILES)
    set_source_files_properties(qml/${_qml}.qml PROPERTIES QT_RESOURCE_ALIAS ${_qml}.qml)
endforeach()

# 设计令牌是单例：全项目只有一个 Theme 实例
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

list(TRANSFORM MCL_QML_FILES PREPEND "qml/")
list(TRANSFORM MCL_QML_FILES APPEND ".qml")

qt_add_qml_module(mcl
    URI Mcl
    VERSION 1.0
    QML_FILES ${MCL_QML_FILES}
)

# 界面使用的图标资源，挂在 /mcl 前缀下（QML 里写 qrc:/mcl/icons/xxx.svg）
qt_add_resources(mcl "mcl_icons"
    PREFIX "/mcl"
    FILES
        icons/app.svg
        icons/cube.svg
        icons/terminal.svg
        icons/gear.svg
        icons/info.svg
        icons/trash.svg
        icons/launchpad.svg
        icons/package.svg
        icons/grass-block.svg
        icons/anvil.svg
        icons/fox.svg
        icons/fabric.svg
        icons/palette.svg
        icons/sparkle.svg
        assets/background.jpg
        assets/apple-logo.png
        icons/png/mcl-256.png
)

if(WIN32)
    # 把图标和版本信息编进 mcl.exe，否则资源管理器里是个白板
    set(MCL_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/mcl.rc")
    string(REPLACE "." "," MCL_VERSION_COMMA "${PROJECT_VERSION}")
    set(MCL_ICO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/mcl.ico")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/mcl.rc.in"
        "${MCL_RC_FILE}"
        @ONLY
    )
    target_sources(mcl PRIVATE "${MCL_RC_FILE}")

    # 装 windeployqt：把 Qt 的 dll 和 QML 插件收进安装目录。
    # 不跑这个，用户双击就是"缺少 Qt6Core.dll"。
    qt_generate_deploy_app_script(
        TARGET mcl
        OUTPUT_SCRIPT MCL_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${MCL_DEPLOY_SCRIPT})
endif()

set_target_properties(mcl PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "local.mcl"
    MACOSX_BUNDLE_BUNDLE_NAME "mcl"
)

# ---------------------------------------------------------------- 安装
include(GNUInstallDirs)

install(TARGETS mcl
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES icons/app.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME mcl.svg
    )
    # 预渲染的多尺寸位图：部分 Dock / 任务栏 / 托盘只认 PNG
    foreach(_size 32 48 64 128 256 512)
        install(FILES "icons/png/mcl-${_size}.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps"
            RENAME mcl.png
        )
    endforeach()
    install(FILES mcl.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
endif()

# ---------------------------------------------------------------- 打包
# Debian/Ubuntu 安装包：
#     ./make-deb.sh
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "mcl")
    set(CPACK_PACKAGE_VENDOR "mcl")
    set(CPACK_PACKAGE_CONTACT "cccp00-cup <2995535609@qq.com>")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "macOS 12 style desktop shell with a Minecraft launcher kernel")
    set(CPACK_PACKAGE_DESCRIPTION
        "mcl 在一个标准窗口里自绘一整套 macOS 12 桌面（菜单栏、Dock、自绘窗口、启动台）。内置完全自研的 Minecraft 启动内核：读官方版本清单、并发下载并校验、解压 natives、拼装 java 命令行并拉起游戏。只依赖 Qt 与 zlib，不依赖 PrismLauncher，也不调用它的二进制。")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/cccp00-cup/mcl")

    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

    set(CPACK_DEBIAN_PACKAGE_NAME "mcl")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "cccp00-cup <2995535609@qq.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")

    # Qt 的 QML 模块是运行时才 dlopen 的，dpkg-shlibdeps 探测不到，必须显式声明。
    # 其余 ELF 依赖（libqt6core6t64、libqt6quick6 等）由 SHLIBDEPS 自动推导后合并。
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtqml-workerscript, libqt6svg6")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postinst")
        set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
            "${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postinst")
    endif()

    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
    set(CPACK_DEBIAN_DEBUGINFO_PACKAGE OFF)

    include(CPack)
endif()

# Windows 安装包（NSIS）：
#     cmake -B build -DCMAKE_BUILD_TYPE=Release
#     cmake --build build --config Release
#     cpack --config build/CPackConfig.cmake
if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME "mcl")
    set(CPACK_PACKAGE_VENDOR "mcl")
    set(CPACK_PACKAGE_CONTACT "cccp00-cup <2995535609@qq.com>")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "macOS 12 style desktop shell with a Minecraft launcher kernel")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/cccp00-cup/mcl")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "mcl")
    set(CPACK_PACKAGE_FILE_NAME "mcl-${PROJECT_VERSION}-win64")
    set(CPACK_PACKAGE_EXECUTABLES "mcl" "mcl")

    set(CPACK_NSIS_DISPLAY_NAME "mcl")
    set(CPACK_NSIS_PACKAGE_NAME "mcl")
    # 注意别让 NSIS 把 exe 找成 bin/mcl.exe —— 我们装到根目录
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "mcl.exe")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/mcl.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/mcl.ico")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    # 开始菜单快捷方式。
    # 反斜杠要写 4 个：CPackConfig.cmake 自己也是 CMake 脚本，会被再解析一次，
    # 所以值里必须留下成对的反斜杠，否则第二次解析会把 \m 当成非法转义。
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\mcl.lnk' '$INSTDIR\\\\mcl.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\mcl.lnk'")

    include(CPack)
endif()
