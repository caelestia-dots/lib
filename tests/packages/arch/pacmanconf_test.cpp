#include <qdir.h>
#include <qstandardpaths.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include <alpm.h>

#include <stdexcept>

#include "common.hpp"
#include "pacmanconf.hpp"

using namespace caelestia::packages;
using Qt::StringLiterals::operator""_s;

class TestPacmanConf : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

    [[nodiscard]] QString confPath() const { return m_root.filePath(u"pacman.conf"_s); }

    [[nodiscard]] PacmanConf load() const {
        const auto conf = readPacmanConf(confPath());
        if (!conf)
            throw std::runtime_error("unable to read pacman.conf");
        return *conf;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        if (QStandardPaths::findExecutable(u"pacman-conf"_s).isEmpty())
            QSKIP("pacman-conf not available");

        QVERIFY(QDir().mkpath(m_root.filePath(u"db"_s)));
        writeFile(m_root.filePath(u"mirrorlist"_s), "Server = https://example.org/$repo/os/$arch\n");
        writeFile(confPath(), QStringLiteral(R"([options]
RootDir = %1
DBPath = %1/db/
Architecture = x86_64 x86_64_v3
SigLevel = Required DatabaseOptional
IgnorePkg = linux
IgnorePkg = firefox
ParallelDownloads = 3

[core]
Include = %1/mirrorlist

[custom]
SigLevel = Optional TrustAll
Usage = Sync Search
Server = file://%1/custom
)")
                                  .arg(m_root.path())
                                  .toUtf8());
    }

    // NOLINTBEGIN(bugprone-signed-bitwise)
    void sigLevelTokens() {
        QCOMPARE(parseSigLevel({}, 42), 42);
        QCOMPARE(
            parseSigLevel(
                { u"PackageRequired"_s, u"PackageTrustedOnly"_s, u"DatabaseOptional"_s, u"DatabaseTrustedOnly"_s }, 0),
            ALPM_SIG_PACKAGE | ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL);
        QCOMPARE(parseSigLevel({ u"PackageNever"_s, u"DatabaseNever"_s }, 0), 0);
        QCOMPARE(parseSigLevel({ u"PackageOptional"_s, u"PackageTrustAll"_s }, 0),
            ALPM_SIG_PACKAGE | ALPM_SIG_PACKAGE_OPTIONAL | ALPM_SIG_PACKAGE_MARGINAL_OK | ALPM_SIG_PACKAGE_UNKNOWN_OK);
    }

    void usageTokens() {
        QCOMPARE(parseUsage({}), ALPM_DB_USAGE_ALL);
        QCOMPARE(parseUsage({ u"All"_s }), ALPM_DB_USAGE_ALL);
        QCOMPARE(parseUsage({ u"Sync"_s, u"Search"_s }), ALPM_DB_USAGE_SYNC | ALPM_DB_USAGE_SEARCH);
    }

    void readsOptions() {
        const auto conf = load();
        QCOMPARE(conf.rootDir, m_root.path());
        QCOMPARE(conf.dbPath, m_root.path() + u"/db"_s);
        QCOMPARE(conf.architectures, (QStringList{ u"x86_64"_s, u"x86_64_v3"_s }));
        QCOMPARE(conf.ignorePkgs, (QStringList{ u"linux"_s, u"firefox"_s }));
        QCOMPARE(conf.parallelDownloads, 3);
        QCOMPARE(conf.sigLevel, ALPM_SIG_PACKAGE | ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL);
    }

    void readsRepos() {
        const auto conf = load();
        QCOMPARE(conf.repos.size(), 2);

        const auto& core = conf.repos[0];
        QCOMPARE(core.name, u"core"_s);
        QCOMPARE(core.servers, (QStringList{ u"https://example.org/core/os/x86_64"_s }));
        QCOMPARE(core.sigLevel, conf.sigLevel);
        QCOMPARE(core.usage, ALPM_DB_USAGE_ALL);

        const auto& custom = conf.repos[1];
        QCOMPARE(custom.name, u"custom"_s);
        QCOMPARE(custom.servers, (QStringList{ u"file://"_s + m_root.path() + u"/custom"_s }));
        QCOMPARE(custom.sigLevel, ALPM_SIG_PACKAGE | ALPM_SIG_PACKAGE_OPTIONAL | ALPM_SIG_PACKAGE_MARGINAL_OK |
                                      ALPM_SIG_PACKAGE_UNKNOWN_OK | ALPM_SIG_DATABASE | ALPM_SIG_DATABASE_OPTIONAL |
                                      ALPM_SIG_DATABASE_MARGINAL_OK | ALPM_SIG_DATABASE_UNKNOWN_OK);
        QCOMPARE(custom.usage, ALPM_DB_USAGE_SYNC | ALPM_DB_USAGE_SEARCH);
    }

    // NOLINTEND(bugprone-signed-bitwise)

    void missingConfigFails() { QVERIFY(!readPacmanConf(m_root.filePath(u"nope.conf"_s))); }
};

QTEST_GUILESS_MAIN(TestPacmanConf)
#include "pacmanconf_test.moc"
