#pragma once

#include <qstring.h>
#include <qstringlist.h>
#include <qtypes.h>

#include <optional>

namespace caelestia::record {

struct Options {
    QString output;
    QString monitor;
    QString region;
    int fps = 0;
    bool audio = false;
    QStringList extraArgs;
};

struct State {
    bool running = false;
    bool paused = false;
    qint64 pid = 0;
    QString output;
    qint64 elapsedMs = 0;

    bool operator==(const State&) const = default;
};

[[nodiscard]] QString statePath();
[[nodiscard]] State state();

[[nodiscard]] bool start(const Options& options);
[[nodiscard]] std::optional<QString> stop();

[[nodiscard]] bool pause();
[[nodiscard]] bool resume();

} // namespace caelestia::record
