#include "pacmanconf.hpp"

#include <qloggingcategory.h>
#include <qprocess.h>
#include <qstandardpaths.h>

#include <alpm.h>

namespace caelestia::packages {

using Qt::StringLiterals::operator""_s;

namespace {

Q_LOGGING_CATEGORY(lcPacmanConf, "caelestia.packages.pacmanconf", QtInfoMsg)

constexpr QStringView k_program = u"pacman-conf";
constexpr int k_timeoutMs = 10000;

struct Directive {
    QString key;
    QString value;
};

using Directives = QList<Directive>;

std::optional<QStringList> query(const QString& configPath, QStringList args) {
    if (!configPath.isEmpty())
        args = QStringList{ u"--config"_s, configPath } + args;

    QProcess proc;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(u"LC_ALL"_s, u"C"_s);
    proc.setProcessEnvironment(env);
    proc.start(k_program.toString(), args);
    if (!proc.waitForFinished(k_timeoutMs) || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        qCWarning(lcPacmanConf) << k_program << args << "failed:" << proc.readAllStandardError().trimmed();
        return std::nullopt;
    }

    return QString::fromUtf8(proc.readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
}

std::optional<Directives> queryDirectives(const QString& configPath, const QStringList& args) {
    const auto lines = query(configPath, args);
    if (!lines)
        return std::nullopt;

    Directives directives;
    for (const auto& line : *lines) {
        const auto eq = line.indexOf(u" = ");
        if (eq != -1)
            directives << Directive{ .key = line.first(eq).trimmed(), .value = line.sliced(eq + 3).trimmed() };
    }
    return directives;
}

QStringList values(const Directives& directives, QStringView key) {
    QStringList list;
    for (const auto& directive : directives) {
        if (directive.key == key)
            list << directive.value;
    }
    return list;
}

QString value(const Directives& directives, QStringView key) {
    return values(directives, key).value(0);
}

} // namespace

int parseSigLevel(const QStringList& tokens, int fallback) {
    if (tokens.isEmpty())
        return fallback;

    int level = 0;
    for (const auto& token : tokens) {
        // NOLINTBEGIN(bugprone-signed-bitwise)
        if (token == u"PackageNever")
            level &= ~(ALPM_SIG_PACKAGE | ALPM_SIG_PACKAGE_OPTIONAL);
        else if (token == u"PackageOptional")
            level |= ALPM_SIG_PACKAGE | ALPM_SIG_PACKAGE_OPTIONAL;
        else if (token == u"PackageRequired")
            level = (level | ALPM_SIG_PACKAGE) & ~ALPM_SIG_PACKAGE_OPTIONAL;
        else if (token == u"PackageTrustedOnly")
            level &= ~(ALPM_SIG_PACKAGE_MARGINAL_OK | ALPM_SIG_PACKAGE_UNKNOWN_OK);
        else if (token == u"PackageTrustAll")
            level |= ALPM_SIG_PACKAGE_MARGINAL_OK | ALPM_SIG_PACKAGE_UNKNOWN_OK;
        else if (token == u"DatabaseNever")
            level &= ~(ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL);
        else if (token == u"DatabaseOptional")
            level |= ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL;
        else if (token == u"DatabaseRequired")
            level = (level | ALPM_SIG_DATABASE) & ~ALPM_SIG_DATABASE_OPTIONAL;
        else if (token == u"DatabaseTrustedOnly")
            level &= ~(ALPM_SIG_DATABASE_MARGINAL_OK | ALPM_SIG_DATABASE_UNKNOWN_OK);
        else if (token == u"DatabaseTrustAll")
            level |= ALPM_SIG_DATABASE_MARGINAL_OK | ALPM_SIG_DATABASE_UNKNOWN_OK;
        // NOLINTEND(bugprone-signed-bitwise)
        else
            qCWarning(lcPacmanConf) << "Unknown SigLevel" << token;
    }
    return level;
}

int parseUsage(const QStringList& tokens) {
    int usage = 0;
    for (const auto& token : tokens) {
        // NOLINTBEGIN(bugprone-signed-bitwise)
        if (token == u"Sync")
            usage |= ALPM_DB_USAGE_SYNC;
        else if (token == u"Search")
            usage |= ALPM_DB_USAGE_SEARCH;
        else if (token == u"Install")
            usage |= ALPM_DB_USAGE_INSTALL;
        else if (token == u"Upgrade")
            usage |= ALPM_DB_USAGE_UPGRADE;
        else if (token == u"All")
            usage |= ALPM_DB_USAGE_ALL;
        // NOLINTEND(bugprone-signed-bitwise)
        else
            qCWarning(lcPacmanConf) << "Unknown Usage" << token;
    }
    return usage == 0 ? ALPM_DB_USAGE_ALL : usage;
}

std::optional<PacmanConf> readPacmanConf(const QString& configPath) {
    if (QStandardPaths::findExecutable(k_program.toString()).isEmpty()) {
        qCWarning(lcPacmanConf) << k_program << "not found";
        return std::nullopt;
    }

    const QStringList directives{
        u"-v"_s,
        u"RootDir"_s,
        u"DBPath"_s,
        u"Architecture"_s,
        u"SigLevel"_s,
        u"IgnorePkg"_s,
        u"IgnoreGroup"_s,
        u"ParallelDownloads"_s,
    };
    const auto options = queryDirectives(configPath, directives);
    if (!options)
        return std::nullopt;

    PacmanConf conf;
    conf.rootDir = value(*options, u"RootDir");
    conf.dbPath = value(*options, u"DBPath");
    conf.architectures = values(*options, u"Architecture");
    conf.ignorePkgs = values(*options, u"IgnorePkg");
    conf.ignoreGroups = values(*options, u"IgnoreGroup");
    conf.sigLevel = parseSigLevel(values(*options, u"SigLevel"), ALPM_SIG_USE_DEFAULT);
    conf.parallelDownloads = value(*options, u"ParallelDownloads").toInt();

    const auto repoList = query(configPath, { u"--repo-list"_s });
    if (!repoList)
        return std::nullopt;

    for (const auto& name : *repoList) {
        const auto repo = queryDirectives(configPath, { u"--repo"_s, name });
        if (!repo)
            return std::nullopt;

        conf.repos << Repo{
            .name = name,
            .servers = values(*repo, u"Server"),
            .sigLevel = parseSigLevel(values(*repo, u"SigLevel"), conf.sigLevel),
            .usage = parseUsage(values(*repo, u"Usage")),
        };
    }

    return conf;
}

} // namespace caelestia::packages
