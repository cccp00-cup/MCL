import QtQuick

// 「整合包市场」：从 Modrinth 找整合包，装成一个新实例。
//
// 装整合包的流程都在内核里（下载 .mrpack → 解析 modrinth.index.json →
// 按它声明的加载器准备版本 → 下载全部文件 → 解压 overrides → 建实例）。
Item {
    id: root

    MarketView {
        anchors.fill: parent
        client: packMarket
        kind: "modpack"
        onInstallRequested: function (project) {
            kernel.installModpack(project.id, project.title)
        }
    }
}
