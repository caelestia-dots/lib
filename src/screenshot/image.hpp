#pragma once

#include <qimage.h>
#include <qlist.h>
#include <qrect.h>

#include <cstdint>
#include <optional>

namespace caelestia::screenshot {

[[nodiscard]] std::optional<uint32_t> preferredShmFormat(const QList<uint32_t>& offered);
[[nodiscard]] QImage::Format imageFormat(uint32_t shmFormat);
[[nodiscard]] QImage applyTransform(const QImage& image, uint32_t transform);
[[nodiscard]] QImage composite(const QRect& region, const QList<QRect>& geometries, const QList<QImage>& parts);

} // namespace caelestia::screenshot
