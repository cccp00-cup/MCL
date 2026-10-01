import QtQuick

// 启动台：全屏铺开所有**实例**。
//
// 和 macOS 的 Launchpad 一致的地方：背景是模糊的桌面壁纸、Dock 依然可见
// （所以它的 z 排在菜单栏和 Dock 之下）。不一样的是这里显示的只有实例。
//
// 交互：
//   · 左键单击 → 启动这个实例
//   · 右键 → 重命名 / 钉到 Dock / 从 Dock 移除 / 删除
//   · 按住拖动 → 排序；拖到底部 Dock 区域松手 → 钉到 Dock
//   · 点空白处 → 关闭
Item {
    id: pad

    property bool opened: false
    property real dockZoneHeight: 100 // 底部这块算"Dock 区域"
    // 由外部注入：Dock 的横向范围，用来判断是否拖到了 Dock 上
    property real dockLeft: 0
    property real dockRight: 0
    signal requestClose()

    visible: opacity > 0.01
    opacity: opened ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
    }
    // 比菜单栏和 Dock（都是 1000）低，所以它们盖在启动台上面 —— 这正是要的效果
    z: 500

    // ——————————————————— 背景：模糊壁纸 + 压暗
    Item {
        anchors.fill: parent
        clip: true

        Image {
            // opened 做闸：没打开时不请求这张大图。
            // 用整屏端点而不是 glassrect —— 启动台就是全屏，不需要尺寸参数，
            // 也就不会踩到"QML 求值时 pad.width 还是 0"那个坑。
            source: pad.opened ? "image://mcl/padbg/" + shellSettings.appearanceKey : ""
            width: pad.width
            height: pad.height
            x: 0
            y: 0
        }
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.30)
        }
    }

    // ——————————————————— 拖拽状态（幽灵图标）
    Item {
        id: dragLayer
        anchors.fill: parent
        visible: false
        z: 10

        property string label: ""
        property real ghostX: 0
        property real ghostY: 0

        Rectangle {
            x: dragLayer.ghostX - width / 2
            y: dragLayer.ghostY - height / 2
            width: 96
            height: 96
            radius: 20
            color: Qt.rgba(1, 1, 1, 0.20)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.35)

            Image {
                anchors.centerIn: parent
                width: 48
                height: 48
                source: "qrc:/mcl/icons/cube.svg"
                sourceSize: Qt.size(48, 48)
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.bottom
                anchors.topMargin: 6
                text: dragLayer.label
                color: "white"
                font.pixelSize: 12
            }
        }
    }

    // ——————————————————— 实例网格
    GridView {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 70
        anchors.rightMargin: 70
        anchors.topMargin: 70
        anchors.bottomMargin: 130
        clip: true
        cellWidth: 152
        cellHeight: 164
        model: kernel.instances

        delegate: Item {
            id: cell
            required property var modelData
            required property int index

            width: grid.cellWidth
            height: grid.cellHeight
            opacity: dragArea.dragging ? 0.2 : 1
            Behavior on opacity {
                NumberAnimation { duration: 120 }
            }

            Item {
                id: iconBox
                anchors.horizontalCenter: parent.horizontalCenter
                y: 12
                width: 92
                height: 92

                Rectangle {
                    anchors.fill: parent
                    radius: 20
                    border.width: 1
                    border.color: Qt.rgba(0, 0, 0, 0.20)
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.lighter("#3f8f5f", 1.25) }
                        GradientStop { position: 1.0; color: Qt.darker("#3f8f5f", 1.15) }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 20
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.30) }
                        GradientStop { position: 0.45; color: Qt.rgba(1, 1, 1, 0.04) }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.12) }
                    }
                }
                Image {
                    anchors.centerIn: parent
                    width: 46
                    height: 46
                    // 原版草方块 / Forge 铁砧 / NeoForge 狐狸 / Fabric 布料；
                    // 整合包用它自己在 Modrinth 上设的图标
                    source: modelData.icon !== undefined && modelData.icon !== ""
                            ? modelData.icon : "qrc:/mcl/icons/grass-block.svg"
                    sourceSize: Qt.size(46, 46)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                // 运行中的实例给个绿点
                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 6
                    width: 12
                    height: 12
                    radius: 6
                    color: "#2fa36b"
                    border.width: 1.5
                    border.color: "white"
                    visible: modelData.running === true
                }
            }

            Text {
                id: nameText
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: iconBox.bottom
                anchors.topMargin: 8
                width: parent.width - 12
                text: modelData.name
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                color: "white"
                font.pixelSize: 13
                style: Text.Outline
                styleColor: Qt.rgba(0, 0, 0, 0.55)
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: nameText.bottom
                anchors.topMargin: 2
                text: modelData.versionId + " · " + modelData.loader
                color: Qt.rgba(1, 1, 1, 0.72)
                font.pixelSize: 11
                style: Text.Outline
                styleColor: Qt.rgba(0, 0, 0, 0.5)
            }

            // —— 右键菜单
            McMenu {
                id: cellMenu
                dark: true
                entries: [
                    { text: qsTr("启动"), action: "launch" },
                    { text: "", action: "" },
                    { text: qsTr("重命名…"), action: "rename" },
                    { text: qsTr("实例设置…"), action: "settings" },
                    { text: qsTr("打开实例文件夹"), action: "folder" },
                    { text: "", action: "" },
                    { text: modelData.pinned ? qsTr("从 Dock 移除") : qsTr("钉到 Dock"), action: "pin" },
                    { text: "", action: "" },
                    { text: qsTr("删除实例"), action: "remove" }
                ]
                onTriggered: function (action) {
                    if (action === "launch") {
                        kernel.launchOffline(cell.modelData.id, "Player")
                    } else if (action === "pin") {
                        kernel.pinInstance(cell.modelData.id, !cell.modelData.pinned)
                    } else if (action === "remove") {
                        kernel.removeInstance(cell.modelData.id)
                    } else if (action === "rename") {
                        pad.openRename(cell.modelData.id, cell.modelData.name)
                    } else if (action === "settings") {
                        // 告诉设置窗口要编辑谁，再把它打开
                        desktop.editingInstance = cell.modelData.id
                        desktop.overlay = ""     // 先收起启动台，别盖住窗口
                        desktop.openApp("instanceSettings")
                    } else if (action === "folder") {
                        kernel.openInstanceFolder(cell.modelData.id)
                    }
                }
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                // 关键：GridView 是可滑动的，不挡住的话它会把手势抢去当滚动，
                // 图标根本拖不动（表现就是"拖了没反应 / 整页乱滑"）。
                preventStealing: true

                property bool dragging: false
                property real pressX: 0
                property real pressY: 0
                // 按下点相对图标中心的偏移：幽灵跟着鼠标走时要减掉它，否则图标会"跳"到指针下
                property real grabX: 0
                property real grabY: 0

                onPressed: function (mouse) {
                    pressX = mouse.x
                    pressY = mouse.y
                    grabX = mouse.x - cell.width / 2
                    grabY = mouse.y - cell.height / 2
                }
                onPositionChanged: function (mouse) {
                    if (mouse.buttons === 0)
                        return
                    const moved = Math.abs(mouse.x - pressX) + Math.abs(mouse.y - pressY)
                    const here = mapToItem(pad, mouse.x, mouse.y)
                    if (!dragging && moved > 10) {
                        dragging = true
                        grid.interactive = false // 拖动期间禁掉网格自身的滑动
                        dragLayer.visible = true
                        dragLayer.label = cell.modelData.name
                        // 立刻定到当前位置，别让幽灵先闪在 (0,0)
                        dragLayer.ghostX = here.x - grabX
                        dragLayer.ghostY = here.y - grabY
                    }
                    if (dragging) {
                        dragLayer.ghostX = here.x - grabX
                        dragLayer.ghostY = here.y - grabY
                    }
                }

                function endDrag(committed) {
                    dragging = false
                    dragLayer.visible = false
                    grid.interactive = true
                }

                onReleased: function (mouse) {
                    if (!dragging)
                        return

                    const here = mapToItem(pad, mouse.x, mouse.y)
                    endDrag(true)

                    // 落在底部 Dock 区域 → 钉上去
                    if (here.y > pad.height - pad.dockZoneHeight
                        && here.x >= pad.dockLeft - 20 && here.x <= pad.dockRight + 20) {
                        kernel.pinInstance(cell.modelData.id, true)
                        return
                    }

                    // 否则按落点重排
                    const cols = Math.max(1, Math.floor(grid.width / grid.cellWidth))
                    const col = Math.floor((here.x - grid.x) / grid.cellWidth)
                    const row = Math.floor((here.y - grid.y) / grid.cellHeight)
                    const target = row * cols + Math.max(0, Math.min(col, cols - 1))
                    kernel.moveInstance(cell.modelData.id,
                                        Math.max(0, Math.min(target, grid.count - 1)))
                }

                // 拖到窗口外松手时 Qt 发的是 canceled 而不是 released，
                // 不处理的话幽灵图标会一直挂在屏幕上、网格也一直不可滑动
                onCanceled: endDrag(false)
                onClicked: function (mouse) {
                    if (mouse.button === Qt.RightButton) {
                        cellMenu.popupAt(mouse.x, mouse.y)
                        return
                    }
                    kernel.launchOffline(cell.modelData.id, "Player")
                }
            }
        }
    }

    // 空状态
    Column {
        anchors.centerIn: parent
        spacing: 10
        visible: grid.count === 0

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("还没有实例")
            color: "white"
            font.pixelSize: 20
            style: Text.Outline
            styleColor: Qt.rgba(0, 0, 0, 0.5)
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("在桌面右键 →「新建实例」，或点 Dock 里的启动器")
            color: Qt.rgba(1, 1, 1, 0.7)
            font.pixelSize: 13
            style: Text.Outline
            styleColor: Qt.rgba(0, 0, 0, 0.5)
        }
    }

    // ——————————————————— 重命名浮层
    Rectangle {
        id: renameLayer
        anchors.centerIn: parent
        width: 340
        height: 132
        radius: 12
        visible: false
        color: Qt.rgba(0.16, 0.16, 0.18, 0.96)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.18)
        z: 20

        property string targetId: ""
        signal confirmed(string id, string name)

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 18
            text: qsTr("重命名实例")
            color: "#f5f5f7"
            font.pixelSize: 13
        }

        Rectangle {
            id: inputBox
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            anchors.topMargin: 46
            height: 32
            radius: 7
            color: Qt.rgba(1, 1, 1, 0.10)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.25)

            TextInput {
                id: nameInput
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: TextInput.AlignVCenter
                color: "white"
                font.pixelSize: 13
                selectByMouse: true
                clip: true
                Keys.onReturnPressed: renameLayer.confirm()
                Keys.onEscapePressed: pad.closeRename()
            }
        }

        Row {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 14
            spacing: 8

            Rectangle {
                width: 64
                height: 26
                radius: 6
                color: cancelMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.08)
                Text {
                    anchors.centerIn: parent
                    text: qsTr("取消")
                    color: "#f5f5f7"
                    font.pixelSize: 12
                }
                MouseArea {
                    id: cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: pad.closeRename()
                }
            }

            Rectangle {
                width: 64
                height: 26
                radius: 6
                color: okMouse.containsMouse ? Qt.darker(Theme.accent, 1.1) : Theme.accent
                Text {
                    anchors.centerIn: parent
                    text: qsTr("确定")
                    color: "white"
                    font.pixelSize: 12
                }
                MouseArea {
                    id: okMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: renameLayer.confirm()
                }
            }
        }

        function confirm() {
            kernel.renameInstance(renameLayer.targetId, nameInput.text)
            pad.closeRename()
        }
    }

    Connections {
        target: renameLayer
        function onConfirmed(id, name) {
            kernel.renameInstance(id, name)
            pad.closeRename()
        }
    }

    function openRename(id, currentName) {
        renameLayer.targetId = id
        nameInput.text = currentName
        renameLayer.visible = true
        nameInput.forceActiveFocus()
        nameInput.selectAll()
    }

    function closeRename() {
        renameLayer.visible = false
    }

    // 点空白处关闭启动台（图标和浮层会先吃掉点击）
    MouseArea {
        anchors.fill: parent
        z: -1
        onClicked: {
            if (renameLayer.visible)
                pad.closeRename()
            else
                pad.requestClose()
        }
    }
}
