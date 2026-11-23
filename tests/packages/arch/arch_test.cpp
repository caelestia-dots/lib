#include <qdatetime.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qprocess.h>
#include <qsignalspy.h>
#include <qstandardpaths.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include <sys/stat.h>

#include <functional>

#include "archbackend.hpp"
#include "common.hpp"

using namespace caelestia::packages;
using Qt::StringLiterals::operator""_s;

namespace {

constexpr auto k_fakeParu = R"(#!/bin/sh
if [ "$1" = "-Qua" ]; then
    echo ":: Checking AUR"
    echo "baz 1.0-1 -> 1.1-1"
else
    echo "$*"
fi
echo "$1" >> "$FAKE_LOG"
)";

QByteArray desc(const QString& name, const QString& version) {
    return u"%NAME%\n%1\n\n%VERSION%\n%2\n\n%ARCH%\nx86_64\n\n"_s.arg(name, version).toUtf8();
}

} // namespace

class TestArchBackend : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

    [[nodiscard]] QString confPath() const { return m_root.filePath(u"pacman.conf"_s); }

    [[nodiscard]] QString systemDb() const { return m_root.filePath(u"var/lib/pacman"_s); }

    void writeLocal(const QString& name, const QString& version) const {
        writeFile(systemDb() + u"/local/%1-%2/desc"_s.arg(name, version), desc(name, version));
    }

    void writeSyncDb(const QString& dbFile, const QList<std::pair<QString, QString>>& packages) const {
        const auto stage = m_root.filePath(u"stage"_s);
        QVERIFY(QDir(stage).removeRecursively());
        QStringList entries;
        for (const auto& [name, version] : packages) {
            entries << u"%1-%2"_s.arg(name, version);
            writeFile(stage + u"/%1/desc"_s.arg(entries.last()), desc(name, version));
        }

        QVERIFY(QDir().mkpath(QFileInfo(dbFile).absolutePath()));
        QProcess tar;
        tar.start(u"tar"_s, QStringList{ u"-cf"_s, dbFile, u"-C"_s, stage } + entries);
        QVERIFY(tar.waitForFinished());
        QCOMPARE(tar.exitCode(), 0);
    }

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        if (QStandardPaths::findExecutable(u"pacman-conf"_s).isEmpty() ||
            QStandardPaths::findExecutable(u"tar"_s).isEmpty())
            QSKIP("pacman-conf or tar not available");

        qputenv("XDG_CACHE_HOME", m_root.filePath(u"cache"_s).toUtf8());
        qputenv("FAKE_LOG", m_root.filePath(u"paru.log"_s).toUtf8());

        const auto bin = m_root.filePath(u"bin"_s);
        writeFile(bin + u"/paru"_s, k_fakeParu);
        QVERIFY(::chmod((bin + u"/paru"_s).toUtf8().constData(), 0755) == 0);
        qputenv("PATH", (bin + u':' + qEnvironmentVariable("PATH")).toUtf8());

        writeFile(systemDb() + u"/local/ALPM_DB_VERSION"_s, "9\n");
        writeLocal(u"foo"_s, u"1.0-1"_s);
        writeLocal(u"bar"_s, u"1.0-1"_s);
        writeLocal(u"baz"_s, u"1.0-1"_s);
        writeSyncDb(m_root.filePath(u"repo/test.db"_s), { { u"foo"_s, u"2.0-1"_s }, { u"bar"_s, u"2.0-1"_s } });
        writeSyncDb(systemDb() + u"/sync/test.db"_s, { { u"foo"_s, u"1.5-1"_s } });
        QFile systemSync(systemDb() + u"/sync/test.db"_s);
        QVERIFY(systemSync.open(QIODevice::ReadWrite));
        QVERIFY(systemSync.setFileTime(QDateTime::currentDateTime().addSecs(-3600), QFileDevice::FileModificationTime));

        writeFile(confPath(), QStringLiteral(R"([options]
RootDir = %1
DBPath = %1/var/lib/pacman/
SigLevel = Never
Architecture = x86_64
IgnorePkg = bar

[test]
Server = file://%1/repo
)")
                                  .arg(m_root.path())
                                  .toUtf8());
    }

    void createRequiresConfig() {
        QVERIFY(!ArchBackend::create(this, m_root.filePath(u"missing.conf"_s)));
        QVERIFY(ArchBackend::create(this, confPath()));
    }

    void capabilitiesReflectHelper() {
        const auto* const backend = ArchBackend::create(this, confPath());
        QVERIFY(backend);
        QCOMPARE(backend->name(), u"Arch"_s);
        const auto caps = backend->capabilities();
        QVERIFY(caps.testFlag(Capability::ListInstalled));
        QVERIFY(caps.testFlag(Capability::ListUpdates));
        QVERIFY(caps.testFlag(Capability::Install));
        QVERIFY(caps.testFlag(Capability::Remove));
        QVERIFY(caps.testFlag(Capability::Upgrade));
        QVERIFY(caps.testFlag(Capability::ManageForeign));
    }

    void coldCacheSeedsFromSystemSyncDb() {
        const auto* const backend = ArchBackend::create(this, confPath());
        QVERIFY(backend);
        auto future = backend->updates(false);
        future.waitForFinished();

        const auto updates = future.result();
        QCOMPARE(updates.size(), 1);
        QCOMPARE(updates[0].package.name, u"foo"_s);
        QCOMPARE(updates[0].newVersion, u"1.5-1"_s);

        const QFileInfo seeded(m_root.filePath(u"cache/caelestia/packages/alpm/sync/test.db"_s));
        QVERIFY(seeded.exists());
        QCOMPARE(seeded.lastModified(), QFileInfo(systemDb() + u"/sync/test.db"_s).lastModified());
    }

    void refreshDownloadsSyncDb() {
        const auto* const backend = ArchBackend::create(this, confPath());
        QVERIFY(backend);
        auto future = backend->updates(true);
        future.waitForFinished();

        const auto dbPath = m_root.filePath(u"cache/caelestia/packages/alpm"_s);
        QVERIFY(QFileInfo::exists(dbPath + u"/sync/test.db"_s));
        QVERIFY(QFileInfo(dbPath + u"/local"_s).isSymLink());
    }

    void updatesMergeRepoAndAur() {
        const auto* const backend = ArchBackend::create(this, confPath());
        QVERIFY(backend);
        QCOMPARE(readFile(m_root.filePath(u"paru.log"_s)).count(u"-Qua"_s), 1);
        auto future = backend->updates(false);
        future.waitForFinished();
        QCOMPARE(readFile(m_root.filePath(u"paru.log"_s)).count(u"-Qua"_s), 1);

        const auto updates = future.result();
        QCOMPARE(updates.size(), 2);
        QCOMPARE(updates[0], (Update{ .package = { .name = u"foo"_s, .version = u"1.0-1"_s, .source = u"test"_s },
                                 .newVersion = u"2.0-1"_s }));
        QCOMPARE(updates[1], (Update{ .package = { .name = u"baz"_s, .version = u"1.0-1"_s, .source = u"aur"_s },
                                 .newVersion = u"1.1-1"_s }));
    }

    void installedListsSources() {
        const auto* const backend = ArchBackend::create(this, confPath());
        QVERIFY(backend);
        auto future = backend->installed();
        future.waitForFinished();

        const auto packages = future.result();
        QCOMPARE(packages.size(), 3);
        QCOMPARE(packages[0], (Package{ .name = u"bar"_s, .version = u"1.0-1"_s, .source = u"test"_s }));
        QCOMPARE(packages[1], (Package{ .name = u"baz"_s, .version = u"1.0-1"_s, .source = QString() }));
        QCOMPARE(packages[2], (Package{ .name = u"foo"_s, .version = u"1.0-1"_s, .source = u"test"_s }));
    }

    void rejectsInvalidNames() {
        const auto* const backend = ArchBackend::create(this, confPath());
        QVERIFY(backend);
        const std::function<Transaction*()> starters[] = {
            [&] {
                return backend->install({}, this);
            },
            [&] {
                return backend->install({ u"--config=/tmp/x"_s }, this);
            },
            [&] {
                return backend->remove({ u"foo"_s, u"-u"_s }, this);
            },
        };
        for (const auto& start : starters) {
            const auto* const transaction = start();
            QSignalSpy finished(transaction, &Transaction::finished);
            QVERIFY(finished.wait());
            QVERIFY(!transaction->succeeded());
        }
    }

    void installRunsHelper() {
        const auto* const backend = ArchBackend::create(this, confPath());
        QVERIFY(backend);
        const auto* const transaction = backend->install({ u"foo"_s }, this);
        QSignalSpy finished(transaction, &Transaction::finished);
        QVERIFY(finished.wait());
        QVERIFY(transaction->succeeded());
        QVERIFY(transaction->output().join(u' ').endsWith(u"--skipreview -S -- foo"_s));
    }
};

QTEST_GUILESS_MAIN(TestArchBackend)
#include "arch_test.moc"
