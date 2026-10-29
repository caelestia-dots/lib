#pragma once

#include <qrect.h>
#include <qtypes.h>

#include <variant>

namespace caelestia::ipc {

enum class Error : quint8 {
    NotRunning,
    Busy,
    Cancelled,
    Failed,
};

using Response = std::variant<Error, QRect>;

} // namespace caelestia::ipc
