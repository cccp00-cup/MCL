#pragma once

#include <QString>

// 日志出口。
//
// 分平台的原因很实际：
//   - Linux：程序是从终端跑的，stderr 一抬眼就看见了，什么都别改最省事。
//   - Windows：可执行文件是 WIN32_EXECUTABLE，压根没有控制台，stderr 写进去也没人看。
//     所以那边一律落文件，顺带把 Qt / QML 自己的警告也接过来 —— 不然出了怪问题
//     你手上会一点线索都没有。
//
// 因此 install() 在非 Windows 上是**空操作**，line() 也只是照旧往 stderr 打，
// Linux 上的行为跟以前完全一致。
namespace Log {

// 装日志出口（Windows 上开文件 + 接管 Qt 消息处理器，其它平台什么都不做）
void install();

// 打一行业务日志
void line(const QString &text);

// 日志文件落在哪（Windows 上给"打开日志"之类的入口用）
QString filePath();

} // namespace Log
