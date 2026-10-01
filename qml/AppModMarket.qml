import QtQuick

// 「模组市场」：从 Modrinth 找模组，装到指定实例的 mods/ 里。
Item {
    id: root

    // 窗口关掉就解绑 —— 免得以后从 Dock 直接进来时还装着上次那个实例
    Component.onDestruction: modMarket.targetInstance = ""

    // 从「实例设置」进来时，默认就按那个实例的 MC 版本筛。
    // 不这么做的话列表里会混着一堆别的版本的模组 —— 看着能装，装完起不来。
    // （筛选本身是 Modrinth 那边做的，见 ModrinthClient 的 game_versions facet。）
    Component.onCompleted: marketView.versionFilter = root.instanceVersion()

    // 没绑实例（从 Dock 直接进来的）就返回空 = 不限
    function instanceVersion() {
        if (modMarket.targetInstance === "")
            return ""
        const list = kernel.instances
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === modMarket.targetInstance)
                return list[i].versionId
        }
        return ""
    }


    // 详情页打开中（null = 显示列表）
    property var detailProject: null

    readonly property var targetInstanceInfo: {
        if (modMarket.targetInstance === "")
            return null
        const list = kernel.instances
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === modMarket.targetInstance)
                return list[i]
        }
        return null
    }

    MarketView {
        visible: root.detailProject === null
        id: marketView
        anchors.fill: parent
        client: modMarket
        kind: "mod"
        onDetailRequested: function (project) { root.detailProject = project }
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
        property string versionId: ""
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
            kernel.installMod(instanceId, targetMenu.project.id, targetMenu.project.title,
                               targetMenu.versionId)
        }
    }

    // 详情页：摊开所有版本让用户自己挑，并把匹配实例的标出来
    MarketDetail {
        anchors.fill: parent
        visible: root.detailProject !== null
        client: modMarket
        project: root.detailProject
        kind: "mod"
        instanceGameVersion: root.targetInstanceInfo
                             ? String(root.targetInstanceInfo.versionId) : ""
        instanceLoader: root.targetInstanceInfo
                        ? String(root.targetInstanceInfo.loader).toLowerCase() : ""
        onBackRequested: root.detailProject = null
        onInstallRequested: function (version) {
            // 用户点名要的版本，一路带到底 —— 内核不会再自己挑
            if (modMarket.targetInstance !== "") {
                kernel.installMod(modMarket.targetInstance, root.detailProject.id,
                                   root.detailProject.title, version.id)
                return
            }
            // 没绑实例就先问装到哪，把版本一起带过去
            targetMenu.project = root.detailProject
            targetMenu.versionId = version.id
            targetMenu.open()
        }
    }

}
