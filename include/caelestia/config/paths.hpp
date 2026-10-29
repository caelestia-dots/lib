#pragma once

#include <qstring.h>
#include <qtclasshelpermacros.h>

namespace caelestia::config {

class Config;

class ConfigPaths {
public:
    Q_DISABLE_COPY_MOVE(ConfigPaths)

    [[nodiscard]] static QString defaultWallpapersDir();
    [[nodiscard]] QString wallpapersDir() const;
    void setWallpapersDir(const QString& dir) const;

    [[nodiscard]] static QString defaultScreenshotsDir();
    [[nodiscard]] QString screenshotsDir() const;
    void setScreenshotsDir(const QString& dir) const;

    [[nodiscard]] static QString defaultRecordingsDir();
    [[nodiscard]] QString recordingsDir() const;
    void setRecordingsDir(const QString& dir) const;

    [[nodiscard]] static QString stateDir();
    [[nodiscard]] static QString runtimeDir();
    [[nodiscard]] static QString customSchemesDir();
    [[nodiscard]] static QString customThemesDir();

    [[nodiscard]] static QString resolvePath(QString path, const QString& defaultPath);

private:
    explicit ConfigPaths(Config* config);

    Config* const m_config;

    friend Config;
};

} // namespace caelestia::config
