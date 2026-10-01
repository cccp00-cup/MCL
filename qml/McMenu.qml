import QtQuick
import QtQuick.Controls

// macOS 12 风格的弹出菜单。
// **菜单栏的下拉菜单和桌面的右键菜单共用这一个组件** —— 它们本来就是同一种东西，
// 只是定位方式不同：下拉用固定坐标，右键用 popupAt() 落在鼠标位置。
//
// entries 的元素：
//   { text, action, shortcut, enabled }
//   text 为空串表示一条分隔线
Popup {
    id: menu

    property real menuX: 0
    property real menuY: 0
    property var entries: []
    // true = 深色菜单。菜单栏与 Dock 弹出的菜单固定深色（与它们的材质一致）；
    // 默认跟随窗口外观（桌面右键菜单）。
    property bool dark: shellSettings.darkMode

    signal triggered(string action)

    x: menuX
    y: menuY
    padding: 5
    width: 226
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // 在相对 parent 的 (px, py) 处弹出。
    // above=true 表示菜单底边贴在 py 上 —— Dock 在屏幕底部，菜单要往上弹。
    // 这里不做边界裁剪：Popup 本来就不受 parent 裁剪，而且需要能落到父项之外。
    function popupAt(px, py, above) {
        menuX = px
        menuY = above ? py - implicitHeight : py
        open()
    }

    background: Rectangle {
        radius: 7
        color: menu.dark ? Theme.chromeMenuFill : Qt.rgba(0.97, 0.97, 0.98, 0.95)
        border.width: 1
        border.color: menu.dark ? Theme.chromeMenuBorder : Qt.rgba(0, 0, 0, 0.125)
    }

    contentItem: Column {
        spacing: 1

        Repeater {
            model: menu.entries

            delegate: Item {
                id: row
                required property var modelData

                readonly property bool isSeparator: !modelData.text
                readonly property bool itemEnabled: modelData.enabled === undefined
                                                    ? true : modelData.enabled
                readonly property string shortcut: modelData.shortcut ? modelData.shortcut : ""

                width: menu.width - menu.padding * 2
                height: isSeparator ? 9 : 25

                Rectangle {
                    visible: row.isSeparator
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: menu.dark ? Theme.chromeMenuSeparator : Theme.separator
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 5
                    visible: !row.isSeparator
                    color: hover.containsMouse && row.itemEnabled ? Theme.accent : "transparent"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    text: row.isSeparator ? "" : row.modelData.text
                    font.pixelSize: 13
                    color: {
                        if (!row.itemEnabled)
                            return menu.dark ? Theme.chromeTextDim : Theme.textTertiary
                        if (hover.containsMouse)
                            return "white"
                        return menu.dark ? Theme.chromeText : Theme.textPrimary
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    text: row.shortcut
                    font.pixelSize: 12
                    color: hover.containsMouse && row.itemEnabled
                           ? Qt.rgba(1, 1, 1, 0.75)
                           : (menu.dark ? Theme.chromeTextDim : Theme.textTertiary)
                }

                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !row.isSeparator && row.itemEnabled
                    onClicked: {
                        menu.close()
                        menu.triggered(row.modelData.action)
                    }
                }
            }
        }
    }
}
