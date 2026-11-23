#include <qdir.h>
#include <qfile.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include <sys/stat.h>

#include "aurhelper.hpp"
#include "common.hpp"

using namespace caelestia::packages;
using Qt::StringLiterals::operator""_s;

class TestAurHelper : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

    [[nodiscard]] QString binDir() const { return m_root.filePath(u"bin"_s); }

    void installFake(const QString& name) const {
        const auto path = QDir(binDir()).filePath(name);
        writeFile(path, "#!/bin/sh\nexit 0\n");
        QVERIFY(::chmod(path.toUtf8().constData(), 0755) == 0);
    }

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        qputenv("PATH", binDir().toUtf8());
        qputenv("XDG_CACHE_HOME", m_root.filePath(u"cache"_s).toUtf8());
        QVERIFY(QDir().mkpath(binDir()));
    }

    void init() {
        const auto names = QDir(binDir()).entryList(QDir::Files);
        for (const auto& name : names)
            QFile::remove(QDir(binDir()).filePath(name));
    }

    void noHelperWithoutExecutables() {
        installFake(u"pacman"_s);
        QVERIFY(!findAurHelper());
    }

    void prefersParuOverYay() {
        installFake(u"yay"_s);
        QCOMPARE(findAurHelper(), AurHelper::Yay);
        installFake(u"paru"_s);
        QCOMPARE(findAurHelper(), AurHelper::Paru);
    }

    void paruCommandsAreHeadless() {
        const QStringList flags{
            u"--sudo"_s,
            u"/usr/bin/pkexec"_s,
            u"--noconfirm"_s,
            u"--color"_s,
            u"never"_s,
            u"--skipreview"_s,
        };
        QCOMPARE(command(AurHelper::Paru, { u"-S"_s, u"foo"_s }),
            (Command{ .program = u"paru"_s, .arguments = flags + QStringList{ u"-S"_s, u"foo"_s } }));
    }

    void yayCommandsAnswerPrompts() {
        const auto cmd = command(AurHelper::Yay, { u"-Syu"_s });
        QCOMPARE(cmd.program, u"yay"_s);
        QVERIFY(cmd.arguments.contains(u"--sudo"_s));
        QVERIFY(cmd.arguments.contains(u"--answerdiff"_s));
        QVERIFY(cmd.arguments.contains(u"--answerupgrade"_s));
        QVERIFY(!cmd.arguments.contains(u"--skipreview"_s));
        QCOMPARE(cmd.arguments.last(), u"-Syu"_s);
    }

    void pacmanFallbackUsesPkexec() {
        QCOMPARE(
            command(std::nullopt, { u"-R"_s, u"foo"_s }), (Command{ .program = u"/usr/bin/pkexec"_s,
                                                              .arguments = { u"/usr/bin/pacman"_s, u"--noconfirm"_s,
                                                                  u"--color"_s, u"never"_s, u"-R"_s, u"foo"_s } }));
    }

    void validatesPackageNames() {
        const QStringList valid{
            u"firefox"_s,
            u"lib32-glibc"_s,
            u"gtk+3"_s,
            u"python-pyqt6"_s,
            u"extra/foo"_s,
            u"_underscore"_s,
            u"a.b@c"_s,
        };
        for (const auto& name : valid)
            QVERIFY2(isPackageName(name), qPrintable(name));

        const QStringList invalid{
            u""_s,
            u"-u"_s,
            u"--config=/tmp/x"_s,
            u"a b"_s,
            u".hidden"_s,
            u"a/b/c"_s,
            u"foo;rm"_s,
            u"-"_s,
            u"/foo"_s,
        };
        for (const auto& name : invalid)
            QVERIFY2(!isPackageName(name), qPrintable(name));
    }

    void environmentForcesPkexecAndLocale() {
        const auto env = transactionEnvironment();
        QCOMPARE(env.value(u"PACMAN_AUTH"_s), u"/usr/bin/pkexec"_s);
        QCOMPARE(env.value(u"LC_ALL"_s), u"C"_s);
    }

    void parsesAurOutput() {
        const auto updates = parseAurUpdates(u":: Checking\nfoo-git 1.0-1 -> 1.1-1\n\nnoise\nbar 2 -> 3\n"_s);
        QCOMPARE(updates.size(), 2);
        QCOMPARE(updates[0].package.name, u"foo-git"_s);
        QCOMPARE(updates[0].package.source, u"aur"_s);
        QCOMPARE(updates[0].newVersion, u"1.1-1"_s);
        QCOMPARE(updates[1].package.name, u"bar"_s);
    }

    void queriesHelperForUpdates() {
        writeFile(
            QDir(binDir()).filePath(u"paru"_s), "#!/bin/sh\n[ \"$1\" = -Qua ] || exit 1\necho 'baz 1.0-1 -> 1.1-1'\n");
        QVERIFY(::chmod(QDir(binDir()).filePath(u"paru"_s).toUtf8().constData(), 0755) == 0);
        AurUpdateQuery query(AurHelper::Paru);
        const auto updates = query.finish().value_or(QList<Update>());
        QCOMPARE(updates.size(), 1);
        QCOMPARE(updates.first().package.name, u"baz"_s);
    }

    void failedQueryHasNoResult() {
        AurUpdateQuery query(AurHelper::Yay);
        QVERIFY(!query.finish());
    }

    void cacheRoundTrips() {
        QVERIFY(cachedAurUpdates().isEmpty());
        const QList<Update> updates{
            { .package = { .name = u"foo"_s, .version = u"1-1"_s, .source = u"aur"_s }, .newVersion = u"2-1"_s },
            { .package = { .name = u"bar"_s, .version = u"3"_s, .source = u"aur"_s }, .newVersion = u"4"_s },
        };
        cacheAurUpdates(updates);
        QCOMPARE(cachedAurUpdates(), updates);
        cacheAurUpdates({});
        QVERIFY(cachedAurUpdates().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAurHelper)
#include "aurhelper_test.moc"
