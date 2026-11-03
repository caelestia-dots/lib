#pragma once

#include <qfuture.h>
#include <qimage.h>
#include <qrect.h>

class QScreen;

namespace caelestia::screenshot {

[[nodiscard]] bool isSupported();

[[nodiscard]] QFuture<QImage> capture(const QScreen* screen);
[[nodiscard]] QFuture<QImage> capture(const QRect& region);

} // namespace caelestia::screenshot
