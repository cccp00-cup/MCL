#pragma once

#include <QString>

namespace Wallpaper
{
// 跨平台检测当前桌面壁纸的本地文件路径；找不到返回空串。
QString detect();
}
