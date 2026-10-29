#include "caelestia/config/paths.hpp"

#include <qdir.h>
#include <qstandardpaths.h>

#include "caelestia/config/config.hpp"

namespace caelestia::config {

using Qt::StringLiterals::operator""_s;

ConfigPaths::ConfigPaths(Config* config)
    : m_config(config) {}

QString ConfigPaths::defaultWallpapersDir() {
    return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + u"/Wallpapers"_s;
}

QString ConfigPaths::wallpapersDir() const {
    return resolvePath(m_config->value(u"paths.wallpapersDir").toString(), defaultWallpapersDir());
}

void ConfigPaths::setWallpapersDir(const QString& dir) const {
    m_config->setValue(u"paths.wallpapersDir", dir);
}

QString ConfigPaths::defaultScreenshotsDir() {
    return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + u"/Screenshots"_s;
}

QString ConfigPaths::screenshotsDir() const {
    return resolvePath(m_config->value(u"paths.screenshotsDir").toString(), defaultScreenshotsDir());
}

void ConfigPaths::setScreenshotsDir(const QString& dir) const {
    m_config->setValue(u"paths.screenshotsDir", dir);
}

QString ConfigPaths::defaultRecordingsDir() {
    return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + u"/Recordings"_s;
}

QString ConfigPaths::recordingsDir() const {
    return resolvePath(m_config->value(u"paths.recordingsDir").toString(), defaultRecordingsDir());
}

void ConfigPaths::setRecordingsDir(const QString& dir) const {
    m_config->setValue(u"paths.recordingsDir", dir);
}

QString ConfigPaths::stateDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + u"/caelestia"_s;
}

QString ConfigPaths::runtimeDir() {
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + u"/caelestia"_s;
}

QString ConfigPaths::customSchemesDir() {
    return Config::configDir() + u"/schemes"_s;
}

QString ConfigPaths::customThemesDir() {
    return Config::configDir() + u"/themes"_s;
}

QString ConfigPaths::resolvePath(QString path, const QString& defaultPath) {
    if (path.isEmpty())
        return defaultPath;

    if (path == u'~' || path.startsWith(u"~/"))
        path = QDir::homePath() + path.mid(1);
    return QDir::cleanPath(QDir(defaultPath).absoluteFilePath(path));
}

} // namespace caelestia::config
