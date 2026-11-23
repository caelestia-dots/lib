#include "archbackend.hpp"

#include <qfileinfo.h>
#include <qfuture.h>
#include <qstandardpaths.h>

#include <stdexcept>

#include "../processtransaction.hpp"
#include "alpmhandle.hpp"
#include "pacmanconf.hpp"

namespace caelestia::packages {

using Qt::StringLiterals::operator""_s;

namespace {

constexpr QStringView k_defaultConfig = u"/etc/pacman.conf";

std::unique_ptr<AlpmHandle> openHandle(const QString& configPath) {
    const auto conf = readPacmanConf(configPath);
    if (!conf)
        throw std::runtime_error("Unable to read pacman configuration");

    auto handle = AlpmHandle::open(*conf);
    if (!handle)
        throw std::runtime_error("Unable to open package databases");
    return handle;
}

std::unique_ptr<Transaction> run(const Command& command) {
    return std::make_unique<ProcessTransaction>(command.program, command.arguments, transactionEnvironment());
}

std::unique_ptr<Transaction> runOnPackages(
    std::optional<AurHelper> helper, const QString& operation, const QStringList& names) {
    if (names.isEmpty())
        return Transaction::failed(u"No packages given"_s);
    for (const auto& name : names) {
        if (!isPackageName(name))
            return Transaction::failed(u"Invalid package name: "_s + name);
    }
    return run(command(helper, QStringList{ operation, u"--"_s } + names));
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
        return openHandle(config)->installed();
    });
}

QFuture<UpdateCheck> ArchBackend::updates(bool refresh) const {
    return QtFuture::makeReadyVoidFuture().then(
        QtFuture::Launch::Async, [config = m_configPath, helper = m_helper, refresh] {
            std::optional<AurUpdateQuery> aur;
            if (refresh && helper)
                aur.emplace(*helper);

            UpdateCheck check;
            const auto handle = openHandle(config);
            if (refresh) {
                if (const auto error = handle->refresh())
                    check.warnings.append(*error);
            }
            check.updates = handle->updates();

            if (aur) {
                const auto result = aur->finish();
                if (result.ok())
                    cacheAurUpdates(result.updates);
                else
                    check.warnings.append(result.error);
            }
            check.updates += cachedAurUpdates();
            return check;
        });
}

std::unique_ptr<Transaction> ArchBackend::upgrade() const {
    return run(command(m_helper, { u"-Syu"_s }));
}

std::unique_ptr<Transaction> ArchBackend::install(const QStringList& names) const {
    return runOnPackages(m_helper, u"-S"_s, names);
}

std::unique_ptr<Transaction> ArchBackend::remove(const QStringList& names) const {
    return runOnPackages(m_helper, u"-Rns"_s, names);
}

} // namespace caelestia::packages
