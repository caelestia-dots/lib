#pragma once

#include <qrect.h>

#include <variant>

#include "caelestia/ipc/common.hpp"

namespace caelestia::ipc {

template <typename T> using Result = std::variant<T, Error>;

[[nodiscard]] Result<QRect> pickRegion(bool freeze);

} // namespace caelestia::ipc
