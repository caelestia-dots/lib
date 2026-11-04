#pragma once

#include <qimage.h>
#include <qnamespace.h>
#include <qsize.h>
#include <qstring.h>

namespace caelestia::media {

// First frame of the video at path, scaled to size by mode without upscaling; native when size is empty
[[nodiscard]] QImage videoFrame(
    const QString& path, const QSize& size = {}, Qt::AspectRatioMode mode = Qt::KeepAspectRatio);

} // namespace caelestia::media
