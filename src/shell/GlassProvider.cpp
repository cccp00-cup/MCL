#include "GlassProvider.h"
#include "ShellSettings.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

namespace
{
inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline int clamp255(int v)
{
    return clampi(v, 0, 255);
}

// 一维横向盒式模糊，滑动窗口求和 O(w*h)。
void boxBlurHorizontal(QImage &img, int radius)
{
    const int w = img.width();
    const int h = img.height();
    const int window = radius * 2 + 1;
    QImage out(img.size(), img.format());

    for (int y = 0; y < h; ++y) {
        const auto *src = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        auto *dst = reinterpret_cast<QRgb *>(out.scanLine(y));

        int sr = 0, sg = 0, sb = 0, sa = 0;
        for (int x = -radius; x <= radius; ++x) {
            const QRgb c = src[clampi(x, 0, w - 1)];
            sr += qRed(c);
            sg += qGreen(c);
            sb += qBlue(c);
            sa += qAlpha(c);
        }
        for (int x = 0; x < w; ++x) {
            dst[x] = qRgba(sr / window, sg / window, sb / window, sa / window);
            const QRgb remove = src[clampi(x - radius, 0, w - 1)];
            const QRgb add = src[clampi(x + radius + 1, 0, w - 1)];
            sr += qRed(add) - qRed(remove);
            sg += qGreen(add) - qGreen(remove);
            sb += qBlue(add) - qBlue(remove);
            sa += qAlpha(add) - qAlpha(remove);
        }
    }
    img = std::move(out);
}

void boxBlurVertical(QImage &img, int radius)
{
    const int w = img.width();
    const int h = img.height();
    const int window = radius * 2 + 1;
    QImage out(img.size(), img.format());

    for (int x = 0; x < w; ++x) {
        int sr = 0, sg = 0, sb = 0, sa = 0;
        for (int y = -radius; y <= radius; ++y) {
            const QRgb c =
                reinterpret_cast<const QRgb *>(img.constScanLine(clampi(y, 0, h - 1)))[x];
            sr += qRed(c);
            sg += qGreen(c);
            sb += qBlue(c);
            sa += qAlpha(c);
        }
        for (int y = 0; y < h; ++y) {
            reinterpret_cast<QRgb *>(out.scanLine(y))[x] =
                qRgba(sr / window, sg / window, sb / window, sa / window);
            const QRgb remove =
                reinterpret_cast<const QRgb *>(img.constScanLine(clampi(y - radius, 0, h - 1)))[x];
            const QRgb add = reinterpret_cast<const QRgb *>(
                img.constScanLine(clampi(y + radius + 1, 0, h - 1)))[x];
            sr += qRed(add) - qRed(remove);
            sg += qGreen(add) - qGreen(remove);
            sb += qBlue(add) - qBlue(remove);
            sa += qAlpha(add) - qAlpha(remove);
        }
    }
    img = std::move(out);
}

// 三次横纵盒式模糊 ≈ 高斯模糊。
void softBlur(QImage &img, int radius)
{
    radius = qBound(2, radius, 64);
    for (int pass = 0; pass < 3; ++pass) {
        boxBlurHorizontal(img, radius);
        boxBlurVertical(img, radius);
    }
}
} // namespace

GlassProvider::GlassProvider(ShellSettings *settings)
    : QQuickImageProvider(QQmlImageProviderBase::Image)
    , m_settings(settings)
{
}

// 等比放大到覆盖整块底图后居中裁剪，避免拉伸导致色彩分布失真。
QImage GlassProvider::coverScaled(const QImage &src, int w, int h)
{
    if (src.isNull() || w <= 0 || h <= 0)
        return {};
    const double fx = double(w) / src.width();
    const double fy = double(h) / src.height();
    const double f = qMax(fx, fy);
    const QImage scaled = src.scaled(int(qCeil(src.width() * f)), int(qCeil(src.height() * f)),
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int x = qMax(0, (scaled.width() - w) / 2);
    const int y = qMax(0, (scaled.height() - h) / 2);
    return scaled.copy(x, y, qMin(w, scaled.width()), qMin(h, scaled.height()))
        .scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

// 饱和度 + 轻微对比度：让底材更通透、有层次。
void GlassProvider::adjustTone(QImage &img, double saturation, double contrast)
{
    const int h = img.height();
    const int w = img.width();
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb c = line[x];
            const int r = qRed(c);
            const int g = qGreen(c);
            const int b = qBlue(c);
            const int luma = (r * 77 + g * 150 + b * 29) >> 8;
            int nr = luma + int((r - luma) * saturation);
            int ng = luma + int((g - luma) * saturation);
            int nb = luma + int((b - luma) * saturation);
            nr = int((nr - 128) * contrast) + 128;
            ng = int((ng - 128) * contrast) + 128;
            nb = int((nb - 128) * contrast) + 128;
            line[x] = qRgba(clamp255(nr), clamp255(ng), clamp255(nb), qAlpha(c));
        }
    }
}

// 暗角：四角轻微压暗，让中央的桌面主体更突出。
void GlassProvider::applyVignette(QImage &img, double strength)
{
    if (strength <= 0.001)
        return;
    const double cx = img.width() / 2.0;
    const double cy = img.height() / 2.0;
    const double maxD = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const double dy = y - cy;
        for (int x = 0; x < img.width(); ++x) {
            const double dx = x - cx;
            const double d = std::sqrt(dx * dx + dy * dy) / maxD;
            const double k = 1.0 - strength * d * d;
            const QRgb c = line[x];
            line[x] = qRgba(clamp255(int(qRed(c) * k)), clamp255(int(qGreen(c) * k)),
                            clamp255(int(qBlue(c) * k)), qAlpha(c));
        }
    }
}

// 颗粒：模拟磨砂玻璃的细微噪点，避免大面积纯色渐变显得“塑料”。
void GlassProvider::applyGrain(QImage &img, double amount)
{
    if (amount <= 0.001)
        return;
    const int amplitude = int(qBound(0.0, amount, 1.0) * 16.0);
    if (amplitude <= 0)
        return;
    quint32 seed = 0x9e3779b9u;
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            const int n = int((seed >> 16) & 0xff) - 128;
            const int d = n * amplitude / 128;
            const QRgb c = line[x];
            line[x] = qRgba(clamp255(qRed(c) + d), clamp255(qGreen(c) + d), clamp255(qBlue(c) + d),
                            qAlpha(c));
        }
    }
}

// 内置壁纸：优先用随包携带的那张图（assets/background.jpg）；
// 万一资源没打进二进制，再退回程序化生成的 macOS 12 风格渐变兜底。
QImage GlassProvider::builtinWallpaper()
{
    QImage bundled(QStringLiteral(":/mcl/assets/background.jpg"));
    if (!bundled.isNull())
        return bundled;

    QImage img(kCacheW, kCacheH, QImage::Format_ARGB32);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);

    QLinearGradient base(0, 0, kCacheW, kCacheH);
    base.setColorAt(0.00, QColor(0x3a, 0x1f, 0x6b));
    base.setColorAt(0.28, QColor(0x7a, 0x2d, 0x8f));
    base.setColorAt(0.52, QColor(0xc4, 0x4d, 0x6a));
    base.setColorAt(0.76, QColor(0xd4, 0x7a, 0x4a));
    base.setColorAt(1.00, QColor(0x1c, 0x45, 0x88));
    p.fillRect(img.rect(), base);

    auto blob = [&](qreal cx, qreal cy, qreal radius, QColor color) {
        QRadialGradient g(QPointF(cx * kCacheW, cy * kCacheH), radius);
        g.setColorAt(0.0, color);
        g.setColorAt(1.0, QColor(color.red(), color.green(), color.blue(), 0));
        p.fillRect(img.rect(), g);
    };
    blob(0.18, 0.22, 640, QColor(0x6a, 0x3a, 0xd0, 150)); // 左上紫
    blob(0.52, 0.12, 520, QColor(0xd8, 0x4a, 0x9a, 130)); // 上中品红
    blob(0.84, 0.30, 560, QColor(0xf0, 0x8a, 0x4a, 120)); // 右上橙
    blob(0.80, 0.88, 660, QColor(0x2a, 0x9c, 0xc8, 140)); // 右下青
    blob(0.22, 0.84, 560, QColor(0x33, 0x4f, 0xb8, 130)); // 左下蓝
    p.end();

    return img;
}

// 清晰壁纸：桌面背景用，不模糊，也不跟随毛玻璃的饱和度设置。
QImage GlassProvider::wallpaper()
{
    const QString key = m_settings->wallpaperPath();
    if (key == m_wallpaperKey && !m_wallpaperCache.isNull())
        return m_wallpaperCache;

    QImage base;
    if (!key.isEmpty())
        base.load(key);
    if (base.isNull())
        base = builtinWallpaper();

    base = coverScaled(base, kCacheW, kCacheH).convertToFormat(QImage::Format_ARGB32);
    if (base.isNull())
        base = builtinWallpaper();

    m_wallpaperCache = base;
    m_wallpaperKey = key;
    return m_wallpaperCache;
}

void GlassProvider::ensureGlass()
{
    const QString key = m_settings->appearanceKey();
    if (key == m_glassKey && !m_glassCache.isNull())
        return;

    QImage base = wallpaper().convertToFormat(QImage::Format_ARGB32);

    softBlur(base, m_settings->blurRadius());
    adjustTone(base, m_settings->saturation(), 1.06);
    applyVignette(base, 0.22);
    applyGrain(base, m_settings->grain());

    QPainter p(&base);
    // 压暗 + 一丝冷色，统一毛玻璃面板的观感。
    p.fillRect(base.rect(), QColor(0, 0, 0, int(qBound(0.0, m_settings->dim(), 0.9) * 255)));
    p.fillRect(base.rect(), QColor(28, 52, 96, 20));

    // 左上角的柔和反光，让整窗像一块被光扫过的玻璃。
    QRadialGradient sheen(QPointF(kCacheW * 0.22, -kCacheH * 0.15), kCacheH * 0.95);
    sheen.setColorAt(0.0, QColor(255, 255, 255, 24));
    sheen.setColorAt(0.6, QColor(255, 255, 255, 8));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillRect(base.rect(), sheen);
    p.end();

    m_glassCache = base;
    m_glassKey = key;
}

// 可平铺的颗粒纹理：QML 侧以极低不透明度铺在面板上，制造磨砂质感。
QImage GlassProvider::noiseTile(int size) const
{
    size = clampi(size, 32, 512);
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    quint32 seed = 0x1234567u;
    for (int y = 0; y < size; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < size; ++x) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            const int n = int((seed >> 16) & 0xff);
            line[x] = qRgba(255, 255, 255, n * 46 / 255);
        }
    }
    return img;
}

// 径向柔光：供 QML 做「鼠标跟随镜面高光」，避免依赖 GLSL shader。
QImage GlassProvider::radialGlow(int size) const
{
    size = clampi(size, 64, 1024);
    const auto it = m_glowCache.constFind(size);
    if (it != m_glowCache.constEnd())
        return it.value();

    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QRadialGradient g(QPointF(size / 2.0, size / 2.0), size / 2.0);
    g.setColorAt(0.00, QColor(255, 255, 255, 235));
    g.setColorAt(0.35, QColor(255, 255, 255, 120));
    g.setColorAt(0.70, QColor(255, 255, 255, 34));
    g.setColorAt(1.00, QColor(255, 255, 255, 0));
    p.fillRect(img.rect(), g);
    p.end();

    m_glowCache.insert(size, img);
    return img;
}

QImage GlassProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    auto finish = [&](QImage img) {
        if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0
            && requestedSize != img.size())
            img = img.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (size)
            *size = img.size();
        return img;
    };

    if (id == QLatin1String("wallpaper") || id.startsWith(QLatin1String("wallpaper/")))
        return finish(wallpaper());

    // 整屏毛玻璃（启动台背景用）。它本来就是全屏，不需要传尺寸 ——
    // 也就绕开了 QML 属性求值顺序导致尺寸还是 0 的问题。
    if (id == QLatin1String("padbg") || id.startsWith(QLatin1String("padbg/"))) {
        ensureGlass();
        return finish(m_glassCache);
    }

    // 按桌面坐标裁一块、并裁出圆角。参数：x/y/w/h/radius/desktopW/desktopH/[外观指纹]
    if (id.startsWith(QLatin1String("glassrect/"))) {
        const QStringList parts = id.split(QLatin1Char('/'));
        if (parts.size() >= 8) {
            qreal x = parts.at(1).toDouble();
            qreal y = parts.at(2).toDouble();
            qreal w = parts.at(3).toDouble();
            qreal h = parts.at(4).toDouble();
            const qreal radius = parts.at(5).toDouble();
            const qreal desktopW = parts.at(6).toDouble();
            const qreal desktopH = parts.at(7).toDouble();

            if (desktopW < 1.0 || desktopH < 1.0)
                return {};

            // 调用方可能还没来得及拿到尺寸（QML 属性求值顺序），传了 0 就按整屏算
            if (w < 1.0 || h < 1.0) {
                x = 0;
                y = 0;
                w = desktopW;
                h = desktopH;
            }

            if (w >= 1.0 && h >= 1.0) {
                ensureGlass();
                const int outW = int(qRound(w));
                const int outH = int(qRound(h));
                const qreal sx = m_glassCache.width() / desktopW;
                const qreal sy = m_glassCache.height() / desktopH;

                QImage out(outW, outH, QImage::Format_ARGB32_Premultiplied);
                out.fill(Qt::transparent);
                QPainter painter(&out);
                painter.setRenderHint(QPainter::Antialiasing);
                QPainterPath clip;
                clip.addRoundedRect(QRectF(0, 0, outW, outH), radius, radius);
                painter.setClipPath(clip);
                painter.drawImage(QRectF(0, 0, outW, outH), m_glassCache,
                                  QRectF(x * sx, y * sy, w * sx, h * sy));
                painter.end();

                if (size)
                    *size = out.size();
                return out;
            }
        }
        return {};
    }

    if (id.isEmpty() || id == QLatin1String("glass") || id.startsWith(QLatin1String("glass/"))) {
        ensureGlass();
        return finish(m_glassCache);
    }

    if (id == QLatin1String("noise") || id.startsWith(QLatin1String("noise/"))) {
        int px = 128;
        if (id.startsWith(QLatin1String("noise/")))
            px = id.mid(6).toInt();
        const QImage img = noiseTile(px);
        if (size)
            *size = img.size();
        return img;
    }

    if (id == QLatin1String("glow") || id.startsWith(QLatin1String("glow/"))) {
        int px = 256;
        if (id.startsWith(QLatin1String("glow/")))
            px = id.mid(5).toInt();
        const QImage img = radialGlow(px);
        if (size)
            *size = img.size();
        return img;
    }

    return {};
}
