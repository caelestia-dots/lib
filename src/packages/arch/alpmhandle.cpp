#include "alpmhandle.hpp"

#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qloggingcategory.h>

#include <algorithm>

#include "caelestia/utils/cache.hpp"

namespace caelestia::packages {

using Qt::StringLiterals::operator""_s;

namespace {

Q_LOGGING_CATEGORY(lcAlpm, "caelestia.packages.alpm", QtInfoMsg)

QString dbPath() {
    return utils::Cache::cacheDir() + u"/packages/alpm"_s;
}

QString str(const char* value) {
    return QString::fromUtf8(value);
}

bool linkLocalDb(const QString& systemDbPath, const QString& dbPath) {
    const auto target = QDir(systemDbPath).filePath(u"local"_s);
    const auto link = QDir(dbPath).filePath(u"local"_s);
    if (!QFileInfo::exists(target)) {
        qCWarning(lcAlpm) << "Local database" << target << "does not exist";
        return false;
    }

    const QFileInfo info(link);
    if (info.isSymLink() && info.symLinkTarget() == target)
        return true;

    QFile::remove(link);
    return QFile::link(target, link);
}

bool registerRepo(alpm_handle_t* handle, const Repo& repo) {
    auto* db = alpm_register_syncdb(handle, repo.name.toUtf8().constData(), repo.sigLevel);
    if (!db)
        return false;

    alpm_db_set_usage(db, repo.usage);
    return std::ranges::all_of(repo.servers, [db](const QString& server) {
        return alpm_db_add_server(db, server.toUtf8().constData()) == 0;
    });
}

} // namespace

AlpmHandle::AlpmHandle(alpm_handle_t* handle)
    : m_handle(handle) {}

AlpmHandle::~AlpmHandle() {
    alpm_release(m_handle);
}

std::unique_ptr<AlpmHandle> AlpmHandle::open(const PacmanConf& conf) {
    const auto path = dbPath();
    if (!QDir().mkpath(path) || !linkLocalDb(conf.dbPath, path)) {
        qCWarning(lcAlpm) << "Unable to prepare database directory" << path;
        return nullptr;
    }

    alpm_errno_t err{};
    auto* raw = alpm_initialize(conf.rootDir.toUtf8().constData(), path.toUtf8().constData(), &err);
    if (!raw) {
        qCWarning(lcAlpm) << "Unable to initialise libalpm:" << alpm_strerror(err);
        return nullptr;
    }

    std::unique_ptr<AlpmHandle> handle(new AlpmHandle(raw));
    alpm_option_set_logfile(raw, QDir(path).filePath(u"alpm.log"_s).toUtf8().constData());
    for (const auto& arch : conf.architectures)
        alpm_option_add_architecture(raw, arch.toUtf8().constData());
    for (const auto& pkg : conf.ignorePkgs)
        alpm_option_add_ignorepkg(raw, pkg.toUtf8().constData());
    for (const auto& group : conf.ignoreGroups)
        alpm_option_add_ignoregroup(raw, group.toUtf8().constData());
    if (conf.parallelDownloads > 0)
        alpm_option_set_parallel_downloads(raw, static_cast<unsigned int>(conf.parallelDownloads));

    for (const auto& repo : conf.repos) {
        if (!registerRepo(raw, repo)) {
            qCWarning(lcAlpm) << "Unable to register repository" << repo.name << handle->error();
            return nullptr;
        }
    }

    return handle;
}

QString AlpmHandle::error() const {
    return QString::fromUtf8(alpm_strerror(alpm_errno(m_handle)));
}

alpm_db_t* AlpmHandle::syncDbFor(alpm_pkg_t* local) const {
    for (const auto* i = alpm_get_syncdbs(m_handle); i; i = alpm_list_next(i)) {
        auto* const db = static_cast<alpm_db_t*>(i->data);
        if (alpm_db_get_pkg(db, alpm_pkg_get_name(local)))
            return db;
    }
    return nullptr;
}

bool AlpmHandle::refresh() {
    if (alpm_db_update(m_handle, alpm_get_syncdbs(m_handle), 0) < 0) {
        qCWarning(lcAlpm) << "Unable to refresh databases:" << error();
        return false;
    }
    return true;
}

QList<Package> AlpmHandle::installed() const {
    QList<Package> packages;
    for (const auto* i = alpm_db_get_pkgcache(alpm_get_localdb(m_handle)); i; i = alpm_list_next(i)) {
        auto* const local = static_cast<alpm_pkg_t*>(i->data);
        const auto* db = syncDbFor(local);
        packages << Package{
            .name = str(alpm_pkg_get_name(local)),
            .version = str(alpm_pkg_get_version(local)),
            .source = db ? str(alpm_db_get_name(db)) : QString(),
        };
    }
    return packages;
}

QList<Update> AlpmHandle::updates() const {
    QList<Update> updates;
    auto* const syncDbs = alpm_get_syncdbs(m_handle);
    for (const auto* i = alpm_db_get_pkgcache(alpm_get_localdb(m_handle)); i; i = alpm_list_next(i)) {
        auto* const local = static_cast<alpm_pkg_t*>(i->data);
        if (alpm_pkg_should_ignore(m_handle, local))
            continue;

        auto* const candidate = alpm_sync_get_new_version(local, syncDbs);
        if (!candidate)
            continue;

        updates << Update{
            .package = {
                .name = str(alpm_pkg_get_name(local)),
                .version = str(alpm_pkg_get_version(local)),
                .source = str(alpm_db_get_name(alpm_pkg_get_db(candidate))),
            },
            .newVersion = str(alpm_pkg_get_version(candidate)),
        };
    }
    return updates;
}

} // namespace caelestia::packages
