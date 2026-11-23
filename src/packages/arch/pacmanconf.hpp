#pragma once

#include <qlist.h>
#include <qstring.h>
#include <qstringlist.h>

#include <optional>

namespace caelestia::packages {

struct Repo {
    QString name;
    QStringList servers;
    int sigLevel = 0;
    int usage = 0;

    bool operator==(const Repo&) const = default;
};

struct PacmanConf {
    QString rootDir;
    QString dbPath;
    QString gpgDir;
    QStringList architectures;
    QStringList ignorePkgs;
    QStringList ignoreGroups;
    int sigLevel = 0;
    int parallelDownloads = 0;
    QList<Repo> repos;
};

[[nodiscard]] int parseSigLevel(const QStringList& tokens, int fallback);
[[nodiscard]] int parseUsage(const QStringList& tokens);

[[nodiscard]] std::optional<PacmanConf> readPacmanConf(const QString& configPath = {});

} // namespace caelestia::packages
