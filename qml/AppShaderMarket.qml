import QtQuick

// 「光影市场」：从 Modrinth 找光影包，装到实例的 shaderpacks/。
Item {
    id: root

    Component.onDestruction: shaderMarket.targetInstance = ""

    MarketView {
        anchors.fill: parent
        client: shaderMarket
        kind: "shader"
        onInstallRequested: function (project) {
            if (shaderMarket.targetInstance !== "") {
                kernel.installContent(shaderMarket.targetInstance, project.id, "shader",
                                      project.title)
                return
            }
            targetMenu.project = project
            targetMenu.open()
        }
    }

    McMenu {
        id: targetMenu
        property var project: null
        dark: true
        entries: {
            const out = []
            const list = kernel.instances
            for (let i = 0; i < list.length; ++i)
                out.push({ text: list[i].name, action: list[i].id })
            if (out.length === 0)
                out.push({ text: qsTr("还没有实例"), action: "", enabled: false })
            return out
        }
        onTriggered: function (instanceId) {
            if (instanceId === "" || !targetMenu.project)
                return
            kernel.installContent(instanceId, targetMenu.project.id, "shader",
                                  targetMenu.project.title)
        }
    }
}
