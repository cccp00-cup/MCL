import QtQuick

// 「模组市场」：从 Modrinth 找模组，装到指定实例的 mods/ 里。
Item {
    id: root

    // 窗口关掉就解绑 —— 免得以后从 Dock 直接进来时还装着上次那个实例
    Component.onDestruction: modMarket.targetInstance = ""

    MarketView {
        anchors.fill: parent
        client: modMarket
        kind: "mod"
        onInstallRequested: function (project) {
            // 从「实例设置」进来的话，市场已经被绑到那个实例上了，直接装
            if (modMarket.targetInstance !== "") {
                kernel.installMod(modMarket.targetInstance, project.id, project.title)
                return
            }
            // 否则先问装到哪个实例
            targetMenu.project = project
            targetMenu.open()
        }
    }

    // 装到哪个实例？用菜单列出来让用户挑
    McMenu {
        id: targetMenu
        property var project: null
        entries: {
            const out = []
            const list = kernel.instances
            for (let i = 0; i < list.length; ++i)
                out.push({ text: list[i].name, action: list[i].id })
            if (out.length === 0)
                out.push({ text: qsTr("还没有实例"), action: "", enabled: false })
            return out
        }
        // 菜单比较长，用启动台那套深色材质
        dark: true
        onTriggered: function (instanceId) {
            if (instanceId === "" || !targetMenu.project)
                return
            kernel.installMod(instanceId, targetMenu.project.id, targetMenu.project.title)
        }
    }
}
