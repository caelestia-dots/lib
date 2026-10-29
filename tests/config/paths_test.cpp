#include <qdir.h>
#include <qjsonobject.h>
#include <qstandardpaths.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include "caelestia/config/config.hpp"
#include "common.hpp"

using namespace caelestia::config;
using Qt::StringLiterals::operator""_s;

class TestConfigPaths : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QString m_home;

    static QJsonObject writtenPaths() { return readJson(Config::configPath()).value(u"paths"_s).toObject(); }

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        m_home = m_root.path() + u"/home"_s;
        QVERIFY(QDir().mkpath(m_home));
        qputenv("HOME", m_home.toUtf8());
        qputenv("XDG_CONFIG_HOME", (m_root.path() + u"/config"_s).toUtf8());
        qputenv("XDG_STATE_HOME", (m_root.path() + u"/state"_s).toUtf8());
        qputenv("XDG_RUNTIME_DIR", m_root.path().toUtf8());
        QCOMPARE(QDir::homePath(), m_home);
    }

    void staticDirs() {
        QCOMPARE(ConfigPaths::stateDir(), m_root.path() + u"/state/caelestia"_s);
        QCOMPARE(ConfigPaths::runtimeDir(), m_root.path() + u"/caelestia"_s);
        QCOMPARE(ConfigPaths::customSchemesDir(), Config::configDir() + u"/schemes"_s);
        QCOMPARE(ConfigPaths::customThemesDir(), Config::configDir() + u"/themes"_s);
    }

    void defaultDirsFollowStandardLocations() {
        const auto pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        const auto movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        QCOMPARE(ConfigPaths::defaultWallpapersDir(), pictures + u"/Wallpapers"_s);
        QCOMPARE(ConfigPaths::defaultScreenshotsDir(), pictures + u"/Screenshots"_s);
        QCOMPARE(ConfigPaths::defaultRecordingsDir(), movies + u"/Recordings"_s);
        QVERIFY(ConfigPaths::defaultWallpapersDir().startsWith(m_home));
        QVERIFY(ConfigPaths::defaultRecordingsDir().startsWith(m_home));
    }

    void resolvePath_data() {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("expected");

        const auto def = u"/base/default"_s;
        QTest::newRow("empty") << u""_s << def;
        QTest::newRow("absolute") << u"/abs/dir"_s << u"/abs/dir"_s;
        QTest::newRow("absolute-trailing-slash") << u"/abs/dir/"_s << u"/abs/dir"_s;
        QTest::newRow("absolute-uncleaned") << u"/a/./b/../c"_s << u"/a/c"_s;
        QTest::newRow("relative") << u"sub"_s << def + u"/sub"_s;
        QTest::newRow("relative-nested") << u"sub/deep"_s << def + u"/sub/deep"_s;
        QTest::newRow("relative-dot") << u"./sub"_s << def + u"/sub"_s;
        QTest::newRow("relative-parent") << u"../other"_s << u"/base/other"_s;
        QTest::newRow("tilde") << u"~"_s << m_home;
        QTest::newRow("tilde-slash") << u"~/"_s << m_home;
        QTest::newRow("tilde-sub") << u"~/pics"_s << m_home + u"/pics"_s;
        QTest::newRow("tilde-user-not-expanded") << u"~user/pics"_s << def + u"/~user/pics"_s;
        QTest::newRow("tilde-in-middle-not-expanded") << u"a/~/b"_s << def + u"/a/~/b"_s;
    }

    void resolvePath() {
        QFETCH(const QString, path);
        QFETCH(const QString, expected);
        QCOMPARE(ConfigPaths::resolvePath(path, u"/base/default"_s), expected);
    }

    void defaultsWhenUnset() {
        const auto& paths = Config::instance().paths();
        QVERIFY(Config::instance().root().isEmpty());
        QCOMPARE(paths.wallpapersDir(), ConfigPaths::defaultWallpapersDir());
        QCOMPARE(paths.screenshotsDir(), ConfigPaths::defaultScreenshotsDir());
        QCOMPARE(paths.recordingsDir(), ConfigPaths::defaultRecordingsDir());
    }

    void readsConfiguredValues() {
        auto& config = Config::instance();
        writeFile(Config::configPath(), R"({"paths":{
            "wallpapersDir":"/walls","screenshotsDir":"~/shots","recordingsDir":"clips"}})");
        config.reload();

        const auto& paths = config.paths();
        QCOMPARE(paths.wallpapersDir(), u"/walls"_s);
        QCOMPARE(paths.screenshotsDir(), m_home + u"/shots"_s);
        QCOMPARE(paths.recordingsDir(), ConfigPaths::defaultRecordingsDir() + u"/clips"_s);
    }

    void emptyOrNonStringFallsBackToDefault() {
        auto& config = Config::instance();
        writeFile(Config::configPath(), R"({"paths":{"wallpapersDir":"","screenshotsDir":5,"recordingsDir":null}})");
        config.reload();

        const auto& paths = config.paths();
        QCOMPARE(paths.wallpapersDir(), ConfigPaths::defaultWallpapersDir());
        QCOMPARE(paths.screenshotsDir(), ConfigPaths::defaultScreenshotsDir());
        QCOMPARE(paths.recordingsDir(), ConfigPaths::defaultRecordingsDir());
    }

    void settersUpdateAndPersist() {
        auto& config = Config::instance();
        config.reload();
        const auto& paths = config.paths();

        paths.setWallpapersDir(u"/w"_s);
        paths.setScreenshotsDir(u"~/s"_s);
        paths.setRecordingsDir(u"r"_s);

        QCOMPARE(paths.wallpapersDir(), u"/w"_s);
        QCOMPARE(paths.screenshotsDir(), m_home + u"/s"_s);
        QCOMPARE(paths.recordingsDir(), ConfigPaths::defaultRecordingsDir() + u"/r"_s);

        const auto written = writtenPaths();
        QCOMPARE(written[u"wallpapersDir"_s].toString(), u"/w"_s);
        QCOMPARE(written[u"screenshotsDir"_s].toString(), u"~/s"_s);
        QCOMPARE(written[u"recordingsDir"_s].toString(), u"r"_s);
    }

    void settersStoreRawNotResolved() {
        const auto& paths = Config::instance().paths();
        paths.setWallpapersDir(u"~/a/../b"_s);
        QCOMPARE(writtenPaths().value(u"wallpapersDir"_s).toString(), u"~/a/../b"_s);
        QCOMPARE(paths.wallpapersDir(), m_home + u"/b"_s);
    }
};

QTEST_GUILESS_MAIN(TestConfigPaths)

#include "paths_test.moc"
