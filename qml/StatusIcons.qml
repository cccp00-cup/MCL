import QtQuick

// 菜单栏右侧的状态区图标：Spotlight / 控制中心 / Wi-Fi / 电池。
// 全部自绘 —— 不引入图标字体，也不依赖任何平台资源；颜色跟随深浅模式。
Row {
    id: root

    spacing: 14
    readonly property color iconColor: Theme.chromeText

    // —— Spotlight（放大镜）
    Canvas {
        width: 17
        height: 17
        anchors.verticalCenter: parent.verticalCenter

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.iconColor
            ctx.lineWidth = 1.45
            ctx.lineCap = "round"
            ctx.beginPath()
            ctx.arc(7.4, 7.4, 5.3, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(11.5, 11.5)
            ctx.lineTo(15.2, 15.2)
            ctx.stroke()
        }
    }

    // —— 控制中心（macOS 12 的两个滑块）
    Item {
        width: 17
        height: 17
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            x: 0
            y: 2.4
            width: 17
            height: 4
            radius: 2
            color: root.iconColor
            opacity: 0.35
        }
        Rectangle {
            x: 1
            y: 1.9
            width: 5
            height: 5
            radius: 2.5
            color: root.iconColor
        }
        Rectangle {
            x: 0
            y: 10.6
            width: 17
            height: 4
            radius: 2
            color: root.iconColor
            opacity: 0.35
        }
        Rectangle {
            x: 11
            y: 10.1
            width: 5
            height: 5
            radius: 2.5
            color: root.iconColor
        }
    }

    // —— Wi-Fi
    Canvas {
        width: 19
        height: 17
        anchors.verticalCenter: parent.verticalCenter

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = root.iconColor
            ctx.fillStyle = root.iconColor
            ctx.lineWidth = 1.5
            ctx.lineCap = "round"
            const cx = 9.5
            const cy = 14.2
            for (let i = 0; i < 3; ++i) {
                const r = 3.0 + i * 3.1
                ctx.beginPath()
                ctx.arc(cx, cy, r, Math.PI * 1.22, Math.PI * 1.78)
                ctx.stroke()
            }
            ctx.beginPath()
            ctx.arc(cx, cy, 1.05, 0, Math.PI * 2)
            ctx.fill()
        }
    }

    // —— 电池
    Item {
        width: 27
        height: 17
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            x: 0
            y: 2.5
            width: 23
            height: 12
            radius: 3.5
            color: "transparent"
            border.width: 1.2
            border.color: root.iconColor
            opacity: 0.55

            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 1.6
                anchors.verticalCenter: parent.verticalCenter
                width: (parent.width - 3.2) * 0.78
                height: parent.height - 3.2
                radius: 2
                color: root.iconColor
            }
        }
        Rectangle {
            x: 24
            y: 6.4
            width: 2
            height: 4.2
            radius: 1
            color: root.iconColor
            opacity: 0.55
        }
    }
}
