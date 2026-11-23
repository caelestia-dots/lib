#include "archbackend.hpp"

#include <qfileinfo.h>
#include <qfuture.h>
#include <qstandardpaths.h>

#include "../processtransaction.hpp"
#include "alpmhandle.hpp"
#include "pacmanconf.hpp"

namespace caelestia::packages {

using Qt::StringLiterals::operator""_s;

namespace {

constexpr QStringView k_defaultConfig = u"/etc/pacman.conf";

std::unique_ptr<AlpmHandle> openHandle(const QString& configPath) {
    const auto conf = readPacmanConf(configPath);
    return conf ? AlpmHandle::open(*conf) : nullptr;
}

Transaction* run(const Command& command, QObject* parent) {
    return new ProcessTransaction(command.program, command.arguments, transactionEnvironment(), parent);
}

Transaction* runOnPackages(
    std::optional<AurHelper> helper, const QString& operation, const QStringList& names, QObject* parent) {
    if (names.isEmpty())
        return Transaction::failed(u"No packages given"_s, parent);
    for (const auto& name : names) {
        if (!isPackageName(name))
            return Transaction::failed(u"Invalid package name: "_s + name, parent);
    }
    return run(command(helper, QStringList{ operation, u"--"_s } + names), parent);
}

} // namespace

ArchBackend::ArchBackend(QString configPath, std::optional<AurHelper> helper, QObject* parent)
    : Backend(parent)
    , m_configPath(std::move(configPath))
    , m_helper(helper) {}

ArchBackend* ArchBackend::create(QObject* parent, const QString& configPath) {
    const auto config = configPath.isEmpty() ? k_defaultConfig.toString() : configPath;
    if (!QFileInfo::exists(config) || QStandardPaths::findExecutable(u"pacman-conf"_s).isEmpty())
        return nullptr;
    return new ArchBackend(config, findAurHelper(), parent);
}

QString ArchBackend::name() const {
    return u"Arch"_s;
}

Capabilities ArchBackend::capabilities() const {
    Capabilities flags = Capability::ListInstalled | Capability::ListUpdates | Capability::Upgrade |
                         Capability::Install | Capability::Remove;
    if (m_helper)
        flags |= Capability::ManageForeign;
    return flags;
}

QFuture<QList<Package>> ArchBackend::installed() const {
    return QtFuture::makeReadyVoidFuture().then(QtFuture::Launch::Async, [config = m_configPath] {
        const auto handle = openHandle(config);
        return handle ? handle->installed() : QList<Package>();
    });
}

QFuture<QList<Update>> ArchBackend::updates(bool refresh) const {
    return QtFuture::makeReadyVoidFuture().then(
        QtFuture::Launch::Async, [config = m_configPath, helper = m_helper, refresh] {
            std::optional<AurUpdateQuery> aur;
            if (helper)
                aur.emplace(*helper);

            QList<Update> updates;
            if (const auto handle = openHandle(config)) {
                if (refresh)
                    handle->refresh();
                updates = handle->updates();
            }

            if (aur)
                updates += aur->finish();
            return updates;
        });
}

Transaction* ArchBackend::upgrade(QObject* parent) const {
    return run(command(m_helper, { u"-Syu"_s }), parent);
}

Transaction* ArchBackend::install(const QStringList& names, QObject* parent) const {
    return runOnPackages(m_helper, u"-S"_s, names, parent);
}

Transaction* ArchBackend::remove(const QStringList& names, QObject* parent) const {
    return runOnPackages(m_helper, u"-Rns"_s, names, parent);
}

} // namespace caelestia::packages
