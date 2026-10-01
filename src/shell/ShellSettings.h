#pragma once

#include <QObject>
#include <QString>

// 桌面外观设置：壁纸与毛玻璃参数。
// 持久化到 ~/.config/mcl/settings.conf，改一个值就立刻存一次。
//
// wallpaperPath 为空表示使用内置的 macOS 12 Monterey 风格壁纸 —— 这也是默认值，
// 因为 mcl 的目标就是"复刻 macOS 12 桌面"。想要跟随真实桌面壁纸时，
// 调 detectSystemWallpaper() 拿到路径塞进来即可。
class ShellSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString wallpaperPath READ wallpaperPath WRITE setWallpaperPath NOTIFY appearanceChanged)
    Q_PROPERTY(int blurRadius READ blurRadius WRITE setBlurRadius NOTIFY appearanceChanged)
    Q_PROPERTY(qreal dim READ dim WRITE setDim NOTIFY appearanceChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY appearanceChanged)
    Q_PROPERTY(qreal grain READ grain WRITE setGrain NOTIFY appearanceChanged)
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    // 外观指纹：QML 把它拼进 image:// URL，属性一变 URL 就变，从而让 Qt 的图片缓存失效
    Q_PROPERTY(QString appearanceKey READ appearanceKey NOTIFY appearanceChanged)

public:
    explicit ShellSettings(QObject *parent = nullptr);

    static QString configFilePath();

    QString wallpaperPath() const { return m_wallpaperPath; }
    int blurRadius() const { return m_blurRadius; }
    qreal dim() const { return m_dim; }
    qreal saturation() const { return m_saturation; }
    qreal grain() const { return m_grain; }
    bool darkMode() const { return m_darkMode; }
    QString appearanceKey() const;

    void setWallpaperPath(const QString &path);
    void setBlurRadius(int radius);
    void setDim(qreal dim);
    void setSaturation(qreal saturation);
    void setGrain(qreal grain);
    void setDarkMode(bool dark);
    // 命令行临时覆盖：只影响本次运行，不写进配置文件
    void overrideDarkMode(bool dark);

    // 把外观恢复成"刚装好的样子"
    Q_INVOKABLE void resetAppearance();

    // 探测系统壁纸（Linux 走 Plasma 配置 / /proc / gsettings，Windows 走注册表，
    // macOS 走 osascript）；找不到返回空串。
    Q_INVOKABLE QString detectSystemWallpaper() const;

    // 把当前桌面壁纸切到系统壁纸 / 切回内置 macOS 12 风格壁纸
    Q_INVOKABLE void useSystemWallpaper();
    Q_INVOKABLE void useBuiltinWallpaper();

Q_SIGNALS:
    void appearanceChanged();
    void darkModeChanged();

private:
    void load();
    void save() const;

    QString m_wallpaperPath;
    int m_blurRadius = 42;
    qreal m_dim = 0.10;
    qreal m_saturation = 1.18;
    qreal m_grain = 0.16;
    bool m_darkMode = false;
    bool m_loading = false;
    bool m_suppressSave = false;
};
