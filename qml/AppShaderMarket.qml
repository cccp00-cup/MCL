import QtQuick

// 「光影市场」：从 Modrinth 找光影包，装到实例的 shaderpacks/。
Item {
    id: root

    Component.onDestruction: shaderMarket.targetInstance = ""

    // 从「实例设置」进来时，默认按那个实例的 MC 版本筛 ——
    // 资源包和光影挑错版本一样不生效。
    Component.onCompleted: marketView.versionFilter = root.instanceVersion()

    function instanceVersion() {
        if (shaderMarket.targetInstance === "")
            return ""
        const list = kernel.instances
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === shaderMarket.targetInstance)
                return list[i].versionId
        }
        return ""
    }

    MarketView {
        id: marketView
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
