#pragma once

#include <QString>

// 下载源。
//
// 国内直连 Mojang 的 CDN 很慢，所以默认走镜像。注意两家镜像的**映射方式完全不同**：
//
//   · BMCLAPI —— 收敛到单一域名 + 路径前缀（`bmclapi2.bangbang93.com/maven/...`）
//   · FastMCMirror —— 每个服务一个专用子域（`libraries.fastmcmirror.org/...`），
//     而且它**没有镜像 piston-meta / piston-data**，这两个只能落回官方。
//
// 不管哪家，都**不会重写返回内容里的 URL** —— 清单和 version.json 里的下载地址
// 仍然是官方域名，替换必须在"准备下载项"这一步做。
namespace Mirror
{
enum class Source {
    Official,     // 直连 Mojang / Fabric 官方
    Bmclapi,      // bmclapi2.bangbang93.com
    FastMCMirror, // *.fastmcmirror.org
};

void setSource(Source source);
Source source();

// 按 id 取源（"official" / "bmclapi" / "fastmcmirror"），认不出返回默认
Source fromId(const QString &id);
QString sourceId(Source source);

// 当前源的显示名（界面上用）
QString sourceName();

// 把官方地址换成镜像地址；该镜像不覆盖的域名原样返回（调用方一般还带着
// fallbackUrl，失败时会自动换回官方）。
QString rewrite(const QString &url);
} // namespace Mirror
