#include "ShellSettings.h"
#include "Wallpaper.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

ShellSettings::ShellSettings(QObject *parent)
    : QObject(parent)
{
    load();
}

QString ShellSettings::configFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + QStringLiteral("/settings.conf");
}

QString ShellSettings::appearanceKey() const
{
    return QStringLiteral("%1/%2/%3/%4/%5")
        .arg(m_wallpaperPath.isEmpty() ? QStringLiteral("builtin") : m_wallpaperPath)
        .arg(m_blurRadius)
        .arg(m_dim, 0, 'f', 3)
        .arg(m_saturation, 0, 'f', 3)
        .arg(m_grain, 0, 'f', 3);
}

void ShellSettings::setWallpaperPath(const QString &path)
{
    if (m_wallpaperPath == path)
        return;
    m_wallpaperPath = path;
    save();
    Q_EMIT appearanceChanged();
}

void ShellSettings::setBlurRadius(int radius)
{
    radius = qBound(2, radius, 64);
    if (m_blurRadius == radius)
        return;
    m_blurRadius = radius;
    save();
    Q_EMIT appearanceChanged();
}

void ShellSettings::setDim(qreal dim)
{
    dim = qBound(0.0, dim, 0.9);
    if (qFuzzyCompare(m_dim + 1.0, dim + 1.0))
        return;
    m_dim = dim;
    save();
    Q_EMIT appearanceChanged();
}

void ShellSettings::setSaturation(qreal saturation)
{
    saturation = qBound(0.0, saturation, 3.0);
    if (qFuzzyCompare(m_saturation + 1.0, saturation + 1.0))
        return;
    m_saturation = saturation;
    save();
    Q_EMIT appearanceChanged();
}

void ShellSettings::setGrain(qreal grain)
{
    grain = qBound(0.0, grain, 1.0);
    if (qFuzzyCompare(m_grain + 1.0, grain + 1.0))
        return;
    m_grain = grain;
    save();
    Q_EMIT appearanceChanged();
}

void ShellSettings::setDarkMode(bool dark)
{
    if (m_darkMode == dark)
        return;
    m_darkMode = dark;
    save();
    Q_EMIT darkModeChanged();
}

void ShellSettings::overrideDarkMode(bool dark)
{
    if (m_darkMode == dark)
        return;
    // 命令行参数不是用户的持久选择，改值但不落盘
    m_suppressSave = true;
    m_darkMode = dark;
    m_suppressSave = false;
    Q_EMIT darkModeChanged();
}

void ShellSettings::resetAppearance()
{
    m_wallpaperPath.clear();
    m_blurRadius = 42;
    m_dim = 0.10;
    m_saturation = 1.18;
    m_grain = 0.16;
    save();
    Q_EMIT appearanceChanged();
}

QString ShellSettings::detectSystemWallpaper() const
{
    return Wallpaper::detect();
}

void ShellSettings::useSystemWallpaper()
{
    const QString path = Wallpaper::detect();
    if (path.isEmpty())
        return;
    setWallpaperPath(path);
}

void ShellSettings::useBuiltinWallpaper()
{
    setWallpaperPath(QString());
}

void ShellSettings::load()
{
    m_loading = true;
    QSettings s(configFilePath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("appearance"));
    m_wallpaperPath = s.value(QStringLiteral("wallpaperPath")).toString();
    m_blurRadius = s.value(QStringLiteral("blurRadius"), 42).toInt();
    m_dim = s.value(QStringLiteral("dim"), 0.10).toDouble();
    m_saturation = s.value(QStringLiteral("saturation"), 1.18).toDouble();
    m_grain = s.value(QStringLiteral("grain"), 0.16).toDouble();
    s.endGroup();
    m_darkMode = s.value(QStringLiteral("shell/darkMode"), false).toBool();
    m_loading = false;
}

void ShellSettings::save() const
{
    if (m_loading || m_suppressSave)
        return;
    QSettings s(configFilePath(), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("appearance"));
    s.setValue(QStringLiteral("wallpaperPath"), m_wallpaperPath);
    s.setValue(QStringLiteral("blurRadius"), m_blurRadius);
    s.setValue(QStringLiteral("dim"), m_dim);
    s.setValue(QStringLiteral("saturation"), m_saturation);
    s.setValue(QStringLiteral("grain"), m_grain);
    s.endGroup();
    s.setValue(QStringLiteral("shell/darkMode"), m_darkMode);
    s.sync();
}
