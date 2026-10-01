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


    // 详情页打开中（null = 显示列表）
    property var detailProject: null

    readonly property var targetInstanceInfo: {
        if (shaderMarket.targetInstance === "")
            return null
        const list = kernel.instances
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === shaderMarket.targetInstance)
                return list[i]
        }
        return null
    }

    MarketView {
        visible: root.detailProject === null
        id: marketView
        anchors.fill: parent
        client: shaderMarket
        kind: "shader"
        onDetailRequested: function (project) { root.detailProject = project }
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
        property string versionId: ""
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

    // 详情页：摊开所有版本让用户自己挑，并把匹配实例的标出来
    MarketDetail {
        anchors.fill: parent
        visible: root.detailProject !== null
        client: shaderMarket
        project: root.detailProject
        kind: "shader"
        instanceGameVersion: root.targetInstanceInfo
                             ? String(root.targetInstanceInfo.versionId) : ""
        instanceLoader: root.targetInstanceInfo
                        ? String(root.targetInstanceInfo.loader).toLowerCase() : ""
        onBackRequested: root.detailProject = null
        onInstallRequested: function (version) {
            // 用户点名要的版本，一路带到底 —— 内核不会再自己挑
            if (shaderMarket.targetInstance !== "") {
                kernel.installContent(shaderMarket.targetInstance, root.detailProject.id,
                                   "shader", root.detailProject.title, version.id)
                return
            }
            // 没绑实例就先问装到哪，把版本一起带过去
            targetMenu.project = root.detailProject
            targetMenu.versionId = version.id
            targetMenu.open()
        }
    }

}
