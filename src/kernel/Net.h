#pragma once

#include <QNetworkRequest>

// 网络请求的统一设置。所有出网的请求都该过一下这里。
namespace Net {

// 给请求套上 mcl 的标准设置。
//
// **关键：关掉 HTTP/2。**
//
// Qt 默认会尝试协商 HTTP/2，但 Mojang 的 CDN、BMCLAPI 这类源（以及中间的网络
// 设备）并不都认它。协商失败时报的是：
//
//     qt.network.http2: stream N finished with error: "HTTP/2 protocol error"
//
// 更要命的是连接废掉之后，请求常常**悬在那里不返回** —— 既不成功也不失败，
// 界面上就是卡在某个阶段一动不动（比如"查询 Forge 的版本"）。因为不是
// 明确的连接错误，重试逻辑也够不着它。
//
// 强制走 HTTP/1.1 之后这些源都稳定了。
void configure(QNetworkRequest &request);

} // namespace Net
