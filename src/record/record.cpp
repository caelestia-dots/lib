#include "caelestia/record/record.hpp"

#include <qdatetime.h>
#include <qdeadlinetimer.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qlockfile.h>
#include <qloggingcategory.h>
#include <qprocess.h>
#include <qsavefile.h>
#include <qthread.h>

#include <csignal>

#include "caelestia/config/paths.hpp"

namespace caelestia::record {

using Qt::StringLiterals::operator""_s;

namespace {

Q_LOGGING_CATEGORY(lcRecord, "caelestia.record", QtInfoMsg)

constexpr QStringView k_recorder = u"gpu-screen-recorder";
constexpr int k_lockTimeoutMs = 5000;
constexpr int k_stopTimeoutMs = 10000;
constexpr int k_stopPollMs = 50;

struct PersistedData {
    qint64 pid = 0;
    qint64 startTicks = 0;
    QString outputPath;
    qint64 startedAt = 0;
    qint64 pausedSince = 0;
    qint64 pausedTotal = 0;
};

QString lockPath() {
    return config::ConfigPaths::runtimeDir() + u"/record.lock"_s;
}

qint64 now() {
    return QDateTime::currentMSecsSinceEpoch();
}

std::optional<qint64> startTicks(qint64 pid) {
    QFile stat(u"/proc/%1/stat"_s.arg(pid));
    if (!stat.open(QIODevice::ReadOnly))
        return std::nullopt;

    const auto content = stat.readAll();
    const auto fields = content.mid(content.lastIndexOf(')') + 2).split(' ');
    if (fields.size() < 20 || fields[0] == "Z")
        return std::nullopt;

    return fields[19].toLongLong();
}

bool alive(const PersistedData& persisted) {
    return startTicks(persisted.pid) == persisted.startTicks;
}

std::optional<PersistedData> load() {
    QFile file(statePath());
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;

    const auto json = QJsonDocument::fromJson(file.readAll()).object();
    if (json.isEmpty())
        return std::nullopt;

    return PersistedData{
        .pid = json.value(u"pid").toInteger(),
        .startTicks = json.value(u"startTicks").toInteger(),
        .outputPath = json.value(u"outputPath").toString(),
        .startedAt = json.value(u"startedAt").toInteger(),
        .pausedSince = json.value(u"pausedSince").toInteger(),
        .pausedTotal = json.value(u"pausedTotal").toInteger(),
    };
}

std::optional<PersistedData> loadLive() {
    auto persisted = load();
    if (persisted && alive(*persisted))
        return persisted;
    return std::nullopt;
}

bool save(const PersistedData& persisted) {
    QDir().mkpath(QFileInfo(statePath()).absolutePath());
    QSaveFile file(statePath());
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcRecord) << "Unable to open" << file.fileName() << file.errorString();
        return false;
    }

    const QJsonObject obj{
        { u"pid"_s, persisted.pid },
        { u"startTicks"_s, persisted.startTicks },
        { u"outputPath"_s, persisted.outputPath },
        { u"startedAt"_s, persisted.startedAt },
        { u"pausedSince"_s, persisted.pausedSince },
        { u"pausedTotal"_s, persisted.pausedTotal },
    };
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));

    if (!file.commit()) {
        qCWarning(lcRecord) << "Unable to write" << file.fileName() << file.errorString();
        return false;
    }
    return true;
}

bool lock(QLockFile& lockFile) {
    QDir().mkpath(config::ConfigPaths::runtimeDir());
    if (lockFile.tryLock(k_lockTimeoutMs))
        return true;

    qCWarning(lcRecord) << "Unable to acquire" << lockFile.fileName();
    return false;
}

QStringList arguments(const Options& options) {
    QStringList args{ u"-w"_s };
    if (options.region.isEmpty())
        args << (options.monitor.isEmpty() ? u"screen"_s : options.monitor);
    else
        args << u"region"_s << u"-region"_s << options.region;

    if (options.fps > 0)
        args << u"-f"_s << QString::number(options.fps);
    if (options.audio)
        args << u"-a"_s << u"default_output"_s;

    return args << options.extraArgs << u"-o"_s << options.output;
}

bool signal(qint64 pid, int sig) {
    if (::kill(static_cast<pid_t>(pid), sig) == 0)
        return true;

    qCWarning(lcRecord) << "Unable to signal recorder" << pid;
    return false;
}

bool setPaused(bool paused) {
    QLockFile lockFile(lockPath());
    if (!lock(lockFile))
        return false;

    auto persisted = loadLive();
    if (!persisted) {
        qCWarning(lcRecord) << "No recording is running";
        return false;
    }

    if ((persisted->pausedSince != 0) == paused) {
        qCWarning(lcRecord) << "Recording is already" << (paused ? "paused" : "running");
        return false;
    }

    if (!signal(persisted->pid, SIGUSR2))
        return false;

    const auto current = now();
    if (paused) {
        persisted->pausedSince = current;
    } else {
        persisted->pausedTotal += current - persisted->pausedSince;
        persisted->pausedSince = 0;
    }

    return save(*persisted);
}

} // namespace

QString statePath() {
    return config::ConfigPaths::runtimeDir() + u"/record.json"_s;
}

State state() {
    const auto persisted = loadLive();
    if (!persisted)
        return {};

    const auto current = now();
    const auto pausedNow = persisted->pausedSince ? current - persisted->pausedSince : 0;
    return {
        .running = true,
        .paused = persisted->pausedSince != 0,
        .pid = persisted->pid,
        .output = persisted->outputPath,
        .elapsedMs = current - persisted->startedAt - persisted->pausedTotal - pausedNow,
    };
}

bool start(const Options& options) {
    QLockFile lockFile(lockPath());
    if (!lock(lockFile))
        return false;

    if (loadLive()) {
        qCWarning(lcRecord) << "A recording is already running";
        return false;
    }

    QDir().mkpath(QFileInfo(options.output).absolutePath());

    QProcess proc;
    proc.setProgram(k_recorder.toString());
    proc.setArguments(arguments(options));
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.setStandardOutputFile(QProcess::nullDevice());
    proc.setStandardErrorFile(QProcess::nullDevice());
    proc.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);

    qint64 pid = 0;
    if (!proc.startDetached(&pid)) {
        qCWarning(lcRecord) << "Unable to start" << k_recorder << proc.errorString();
        return false;
    }

    return save({
        .pid = pid,
        .startTicks = startTicks(pid).value_or(0),
        .outputPath = options.output,
        .startedAt = now(),
    });
}

std::optional<QString> stop() {
    QLockFile lockFile(lockPath());
    if (!lock(lockFile))
        return std::nullopt;

    const auto persisted = loadLive();
    if (!persisted) {
        qCWarning(lcRecord) << "No recording is running";
        return std::nullopt;
    }

    if (!signal(persisted->pid, SIGINT))
        return std::nullopt;

    const QDeadlineTimer deadline(k_stopTimeoutMs);
    while (alive(*persisted)) {
        if (deadline.hasExpired()) {
            qCWarning(lcRecord) << "Recorder" << persisted->pid << "did not exit";
            return std::nullopt;
        }
        QThread::msleep(k_stopPollMs);
    }

    QFile::remove(statePath());
    return persisted->outputPath;
}

bool pause() {
    return setPaused(true);
}

bool resume() {
    return setPaused(false);
}

} // namespace caelestia::record
