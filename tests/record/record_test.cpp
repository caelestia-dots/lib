#include <qdir.h>
#include <qfile.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qprocess.h>
#include <qtemporarydir.h>
#include <qtest.h>
#include <qthread.h>

#include <sys/stat.h>

#include <csignal>

#include "caelestia/record/record.hpp"
#include "common.hpp"

using namespace caelestia;
using Qt::StringLiterals::operator""_s;

namespace {

constexpr auto k_fakeRecorder = R"(#!/bin/sh
PATH=/usr/bin:/bin
echo "$*" >> "$FAKE_LOG"
[ -n "$FAKE_EXIT" ] && exit "$FAKE_EXIT"
trap 'exit 0' INT TERM
trap 'echo pause >> "$FAKE_LOG"' USR2
while :; do sleep 0.05; done
)";

class TestRecord : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QString m_output;

    [[nodiscard]] QString logPath() const { return m_root.filePath(u"log"_s); }

    [[nodiscard]] QString log() const { return readFile(logPath()); }

    [[nodiscard]] record::Options bare() const {
        return { .output = m_output, .monitor = {}, .region = {}, .fps = 0, .audio = false, .extraArgs = {} };
    }

    [[nodiscard]] record::Options options() const {
        auto opts = bare();
        opts.monitor = u"DP-1"_s;
        opts.fps = 60;
        return opts;
    }

    [[nodiscard]] static bool processExists(qint64 pid) { return ::kill(static_cast<pid_t>(pid), 0) == 0; }

    [[nodiscard]] bool waitForLog(const QString& text) const {
        for (int i = 0; i < 100; ++i) {
            if (log().contains(text))
                return true;
            QThread::msleep(20);
        }
        return false;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        qputenv("XDG_RUNTIME_DIR", m_root.path().toUtf8());
        qputenv("PATH", m_root.filePath(u"bin"_s).toUtf8());
        qputenv("FAKE_LOG", logPath().toUtf8());
        m_output = m_root.filePath(u"out/recording.mp4"_s);

        const auto recorder = m_root.filePath(u"bin/gpu-screen-recorder"_s);
        writeFile(recorder, k_fakeRecorder);
        QCOMPARE(chmod(recorder.toUtf8().constData(), 0755), 0);
    }

    void init() {
        QFile::remove(logPath());
        qunsetenv("FAKE_EXIT");
    }

    void cleanup() {
        static_cast<void>(record::stop());
        QFile::remove(record::statePath());
    }

    void idleWithoutState() { QCOMPARE(record::state(), record::State{}); }

    void startSpawnsRecorder() {
        auto opts = options();
        opts.audio = true;
        opts.extraArgs = { u"-q"_s, u"high"_s };
        QVERIFY(record::start(opts));
        QVERIFY(waitForLog(u"-o"_s));
        QCOMPARE(log(), u"-w DP-1 -f 60 -a default_output -q high -o %1\n"_s.arg(m_output));

        const auto state = record::state();
        QVERIFY(state.running);
        QVERIFY(!state.paused);
        QVERIFY(state.pid > 0);
        QVERIFY(processExists(state.pid));
        QCOMPARE(state.output, m_output);
        QVERIFY(state.elapsedMs >= 0);
        QVERIFY(QFile::exists(record::statePath()));
        QVERIFY(QDir(m_root.filePath(u"out"_s)).exists());
    }

    void startUsesRegion() {
        auto opts = bare();
        opts.region = u"300x200+10+20"_s;
        QVERIFY(record::start(opts));
        QVERIFY(waitForLog(u"-o"_s));
        QCOMPARE(log(), u"-w region -region 300x200+10+20 -o %1\n"_s.arg(m_output));
    }

    void startDefaultsToScreen() {
        QVERIFY(record::start(bare()));
        QVERIFY(waitForLog(u"-o"_s));
        QCOMPARE(log(), u"-w screen -o %1\n"_s.arg(m_output));
    }

    void startRefusesWhileRunning() {
        QVERIFY(record::start(options()));
        QVERIFY(!record::start(options()));
    }

    void startFailsWithoutRecorder() {
        qputenv("PATH", m_root.filePath(u"missing"_s).toUtf8());
        QVERIFY(!record::start(options()));
        QVERIFY(!QFile::exists(record::statePath()));
        qputenv("PATH", m_root.filePath(u"bin"_s).toUtf8());
    }

    void exitedRecorderIsNotRunning() {
        qputenv("FAKE_EXIT", "1");
        QVERIFY(record::start(options()));
        QVERIFY(waitForLog(u"-o"_s));
        QTRY_VERIFY(!record::state().running);
        QVERIFY(!record::stop());
    }

    void staleStateIsReplaced() {
        QProcess proc;
        proc.start(u"/bin/true"_s, {});
        QVERIFY(proc.waitForFinished());
        writeFile(record::statePath(), QJsonDocument(QJsonObject{
                                                         { u"pid"_s, proc.processId() },
                                                         { u"startTicks"_s, 1 },
                                                         { u"output"_s, u"/stale.mp4"_s },
                                                         { u"startedAt"_s, 1 },
                                                     })
                                           .toJson());

        QVERIFY(!record::state().running);
        QVERIFY(record::start(options()));
        QCOMPARE(record::state().output, m_output);
    }

    void pauseAndResumeSignalAndTrackElapsed() {
        QVERIFY(record::start(options()));
        QVERIFY(waitForLog(u"-o"_s));

        QVERIFY(record::pause());
        QVERIFY(waitForLog(u"pause"_s));
        QVERIFY(record::state().paused);
        QVERIFY(!record::pause());

        const auto elapsed = record::state().elapsedMs;
        QThread::msleep(100);
        QCOMPARE(record::state().elapsedMs, elapsed);

        QVERIFY(record::resume());
        QVERIFY(!record::state().paused);
        QVERIFY(!record::resume());
        QThread::msleep(100);
        QVERIFY(record::state().elapsedMs >= elapsed + 100);
    }

    void pauseFailsWhenIdle() { QVERIFY(!record::pause()); }

    void resumeFailsWhenIdle() { QVERIFY(!record::resume()); }

    void stopWaitsForExitAndClears() {
        QVERIFY(record::start(options()));
        QVERIFY(waitForLog(u"-o"_s));
        const auto pid = record::state().pid;

        QCOMPARE(record::stop(), m_output);
        QVERIFY(!QFile::exists(record::statePath()));
        QCOMPARE(record::state(), record::State{});
        QTRY_VERIFY(!processExists(pid));
    }

    void stopFailsWhenIdle() { QVERIFY(!record::stop()); }
};

} // namespace

QTEST_GUILESS_MAIN(TestRecord)

#include "record_test.moc"
