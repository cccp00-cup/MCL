#pragma once

#include <QHash>
#include <QImage>
#include <QQuickImageProvider>
#include <QString>

class ShellSettings;

// 桌面底材图片提供者。为 QML 提供四类图片：
//
//   image://mcl/wallpaper[/key]  —— 清晰壁纸（桌面背景用，不模糊）
//   image://mcl/glass[/key]      —— 壁纸经模糊 + 调色 + 暗角 + 颗粒后的整块底材；
//                                   菜单栏 / Dock / 窗口标题栏把它按窗口坐标裁切，
//                                   就得到"背后内容的毛玻璃"，不需要 ShaderEffect
//   image://mcl/glassrect/<x>/<y>/<w>/<h>/<radius>/<dw>/<dh>/<key>
//                                —— 从上面那张底材里取出桌面坐标 (x,y,w,h) 的一块，
//                                   并按 radius 裁成圆角。**圆角必须在这里做**：
//                                   QML 的 Rectangle.radius 不影响子项裁剪（clip 只裁
//                                   矩形），在 QML 里套圆角会把四个角重新填成直角。
//                                   同时也避开了整图缩放导致的模糊。
//   image://mcl/noise/<size>     —— 可平铺的磨砂颗粒纹理
//   image://mcl/glow/<size>      —— 径向柔光，供 QML 做鼠标跟随高光
//
// 全部在 C++ 侧用 QPainter 一次性像素处理并缓存，**不依赖合成器模糊，也不依赖
// ShaderEffect**，因此 Linux / Windows / macOS 上表现一致 —— 这是 mcl 跨平台的前提。
//
// URL 里的数字只是"外观指纹"，不参与计算；属性一变 URL 就变，从而让 Qt 的 Image
// 缓存失效。真正的参数从 ShellSettings 读取。
class GlassProvider : public QQuickImageProvider
{
public:
    explicit GlassProvider(ShellSettings *settings);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    static constexpr int cacheWidth() { return kCacheW; }
    static constexpr int cacheHeight() { return kCacheH; }

private:
    // 壁纸缓存基准尺寸：桌面窗口一般小于它，放大时平滑缩放即可
    static constexpr int kCacheW = 1600;
    static constexpr int kCacheH = 1000;

    QImage wallpaper();   // 清晰壁纸（含内置 macOS 12 风格兜底）
    void ensureGlass();   // 模糊底材
    QImage noiseTile(int size) const;
    QImage radialGlow(int size) const;

    // 内置的 macOS 12 Monterey 风格壁纸：拿不到系统壁纸时的默认桌面
    static QImage builtinWallpaper();
    static QImage coverScaled(const QImage &src, int w, int h);
    static void adjustTone(QImage &img, double saturation, double contrast);
    static void applyVignette(QImage &img, double strength);
    static void applyGrain(QImage &img, double amount);

    ShellSettings *m_settings;

    QImage m_wallpaperCache;
    QString m_wallpaperKey;

    QImage m_glassCache;
    QString m_glassKey;
    mutable QHash<int, QImage> m_glowCache;
};
