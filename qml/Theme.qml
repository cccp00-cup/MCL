pragma Singleton

import QtQuick

// mcl 的设计令牌：尺寸与颜色集中在这里，组件只引用语义名。
// 深色模式只有一个开关（shellSettings.darkMode），在这一点切换，组件不需要知道
// 自己处于哪种模式。
QtObject {
    readonly property bool dark: shellSettings.darkMode

    // ——— 尺寸 ———
    readonly property int menuBarHeight: 26
    readonly property int titleBarHeight: 28
    readonly property int windowRadius: 10
    readonly property int dockIconSize: 52
    readonly property int dockIconRadius: 12
    readonly property int dockPadding: 7
    readonly property int dockBottomMargin: 6
    // Dock 可见高度：图标 + 上下留白 + 运行指示点的位置
    readonly property int dockHeight: dockIconSize + dockPadding * 2 + 5

    // ——— 颜色 ———
    readonly property color windowBody: dark ? "#1c1c1e" : "#ffffff"
    readonly property color windowTitleBar: dark ? "#2b2b2e" : "#ececee"
    readonly property color windowBorder: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.15)
    readonly property color windowShadow: dark ? Qt.rgba(0, 0, 0, 0.63) : Qt.rgba(0, 0, 0, 0.33)

    readonly property color textPrimary: dark ? "#f5f5f7" : "#1d1d1f"
    readonly property color textSecondary: dark ? "#98989d" : "#6e6e73"
    readonly property color textTertiary: dark ? "#6d6d72" : "#9a9aa0"
    readonly property color menuBarText: dark ? "#f5f5f7" : "#1d1d1f"

    readonly property color separator: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.12)
    readonly property color hoverFill: dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.07)
    readonly property color panelFill: dark ? Qt.rgba(0.17, 0.17, 0.18, 0.65) : Qt.rgba(0.96, 0.96, 0.97, 0.80)

    // ——— 菜单栏与 Dock 的「外壳」材质 ———
    // 固定深色，**不跟随窗口的深浅模式**。macOS 允许「菜单栏和 Dock 使用深色」而
    // 与窗口外观相互独立，这里就采用这种设定：两个部件在任何壁纸、任何窗口配色下
    // 观感都稳定，文字永远是浅色。
    readonly property color chromeTint: Qt.rgba(0.04, 0.04, 0.06, 0.34)
    readonly property color chromeText: "#f7f7f9"
    readonly property color chromeTextDim: Qt.rgba(1, 1, 1, 0.68)
    readonly property color chromeSeparator: Qt.rgba(1, 1, 1, 0.18)
    readonly property color chromeBorder: Qt.rgba(1, 1, 1, 0.22)
    readonly property color chromeHover: Qt.rgba(1, 1, 1, 0.18)
    readonly property color chromeHoverStrong: Qt.rgba(1, 1, 1, 0.28)
    // 从菜单栏 / Dock 弹出的菜单也用深色
    readonly property color chromeMenuFill: Qt.rgba(0.13, 0.13, 0.15, 0.88)
    readonly property color chromeMenuBorder: Qt.rgba(1, 1, 1, 0.16)
    readonly property color chromeMenuSeparator: Qt.rgba(1, 1, 1, 0.15)

    readonly property color accent: "#0a84ff"
    readonly property color trafficClose: "#ff5f57"
    readonly property color trafficMinimize: "#febc2e"
    readonly property color trafficZoom: "#28c840"

    // ——— 动画时长（macOS 观感偏快）———
    readonly property int durationFast: 110
    readonly property int durationNormal: 200
}
