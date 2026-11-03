#pragma once

#include <qrect.h>
#include <qstring.h>

#include <variant>

#include "caelestia/ipc/common.hpp"

namespace caelestia::ipc {

template <typename T> using Result = std::variant<T, Error>;

[[nodiscard]] Result<QRect> pickRegion();
[[nodiscard]] Result<QString> screenshot(bool region, bool annotate);
[[nodiscard]] Result<QString> annotate(const QString& file);

} // namespace caelestia::ipc
