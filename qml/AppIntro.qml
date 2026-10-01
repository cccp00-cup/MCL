import QtQuick

// 「mcl 项目说明」。
//
// 和「关于本机」分开：那个是 macOS 那种一屏系统信息，这个是项目的来龙去脉
// 和已知限制。帮助菜单里点进来，内容可以滚。
Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        anchors.margins: 1
        contentWidth: width
        contentHeight: content.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            x: 26
            y: 22
            width: scroller.width - 52
            spacing: 6

            // ——————————————— 标题
            Text {
                text: "mcl"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Text {
                width: parent.width
                text: qsTr("一个标准尺寸窗口里的 macOS 12 桌面，外加一个完全自研的 Minecraft 启动内核。")
                font.pixelSize: 12
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            Text {
                text: qsTr("版本 %1").arg(Qt.application.version)
                font.pixelSize: 11
                color: Theme.textTertiary
            }

            Item { width: 1; height: 10 }

            // ——————————————— 它是怎么做的
            Text {
                text: qsTr("它是怎么做的")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Repeater {
                model: [
                    { k: qsTr("桌面外壳"),
                      v: qsTr("菜单栏、Dock、窗口、启动台全部用 Qt 6 + QML 自绘，不依赖 KDE 框架。毛玻璃也不是 shader —— 把壁纸模糊一次，各部件按屏幕坐标取自己那块切片。") },
                    { k: qsTr("启动内核"),
                      v: qsTr("完全自研：读官方版本清单 → 并发下载并逐个校验 SHA1 → 解压 natives → 拼 java 命令行 → 拉起游戏。只用 Qt 和 zlib，不依赖也不调用 PrismLauncher。") },
                    { k: qsTr("内容来源"),
                      v: qsTr("模组 / 整合包 / 资源包 / 光影都来自 Modrinth。国内直连不通，走 MCIM 镜像。游戏本体走 BMCLAPI。") },
                    { k: qsTr("账户"),
                      v: qsTr("微软账户走设备码流程 —— 码自动进剪贴板、浏览器自动弹出，切过去粘贴即可。也可以直接用离线账户。") }
                ]

                delegate: Row {
                    required property var modelData
                    width: content.width
                    spacing: 10

                    Text {
                        width: 74
                        text: modelData.k
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 11
                        color: Theme.textTertiary
                    }
                    Text {
                        width: parent.width - 84
                        text: modelData.v
                        font.pixelSize: 11
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item { width: 1; height: 10 }

            // ——————————————— 已知限制
            Text {
                text: qsTr("已知限制")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Repeater {
                model: [
                    qsTr("微软账户的令牌过期后要重新走一次设备码（续期还没做）"),
                    qsTr("Modrinth 上老版本内容偏少 —— 1.12.2 的模组只有 1.20.1 的十二分之一左右，很多老模组只发布在 CurseForge"),
                    qsTr("不支持 Quilt"),
                    qsTr("Java 不做自动下载 —— 自己挑厂商、自己管版本，实例设置里指定")
                ]
                delegate: Text {
                    required property string modelData
                    width: content.width - 10
                    x: 10
                    text: "· " + modelData
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }

            Item { width: 1; height: 10 }

            // ——————————————— 链接
            Text {
                text: qsTr("链接")
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Repeater {
                model: [
                    { k: qsTr("项目主页"), v: "github.com/cccp00-cup/mcl" },
                    { k: qsTr("架构说明"), v: "docs/ARCHITECTURE.md" }
                ]
                delegate: Row {
                    required property var modelData
                    width: content.width
                    spacing: 10

                    Text {
                        width: 74
                        text: modelData.k
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 11
                        color: Theme.textTertiary
                    }
                    Text {
                        width: parent.width - 84
                        text: modelData.v
                        font.pixelSize: 11
                        color: Theme.accent
                    }
                }
            }
        }
    }
}
