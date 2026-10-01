import QtQuick

// 「资源包市场」：从 Modrinth 找资源包，装到实例的 resourcepacks/。
Item {
    id: root

    Component.onDestruction: resourceMarket.targetInstance = ""

    // 从「实例设置」进来时，默认按那个实例的 MC 版本筛 ——
    // 资源包和光影挑错版本一样不生效。
    Component.onCompleted: marketView.versionFilter = root.instanceVersion()

    function instanceVersion() {
        if (resourceMarket.targetInstance === "")
            return ""
        const list = kernel.instances
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === resourceMarket.targetInstance)
                return list[i].versionId
        }
        return ""
    }

    MarketView {
        id: marketView
        anchors.fill: parent
        client: resourceMarket
        kind: "resourcepack"
        onInstallRequested: function (project) {
            if (resourceMarket.targetInstance !== "") {
                kernel.installContent(resourceMarket.targetInstance, project.id, "resourcepack",
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
            kernel.installContent(instanceId, targetMenu.project.id, "resourcepack",
                                  targetMenu.project.title)
        }
    }
}
