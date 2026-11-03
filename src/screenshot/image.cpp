#include "image.hpp"

#include <qpainter.h>
#include <qtransform.h>

#include <wayland-client-protocol.h>

#include <algorithm>

namespace caelestia::screenshot {

namespace {

struct FormatMapping {
    uint32_t shm;
    QImage::Format image;
};

constexpr FormatMapping k_formats[] = {
    { .shm = WL_SHM_FORMAT_XRGB8888, .image = QImage::Format_RGB32 },
    { .shm = WL_SHM_FORMAT_ARGB8888, .image = QImage::Format_ARGB32_Premultiplied },
    { .shm = WL_SHM_FORMAT_XBGR8888, .image = QImage::Format_RGBX8888 },
    { .shm = WL_SHM_FORMAT_ABGR8888, .image = QImage::Format_RGBA8888_Premultiplied },
};

} // namespace

std::optional<uint32_t> preferredShmFormat(const QList<uint32_t>& offered) {
    const auto* chosen = std::ranges::find_if(k_formats, [&offered](const FormatMapping& mapping) {
        return offered.contains(mapping.shm);
    });
    if (chosen == std::end(k_formats))
        return std::nullopt;
    return chosen->shm;
}

QImage::Format imageFormat(uint32_t shmFormat) {
    for (const auto& [shm, image] : k_formats) {
        if (shm == shmFormat)
            return image;
    }
    return QImage::Format_Invalid;
}

QImage applyTransform(const QImage& image, uint32_t transform) {
    const auto flipped = transform >= WL_OUTPUT_TRANSFORM_FLIPPED;
    const auto rotation = static_cast<int>(transform & 3) * 90;

    auto result = flipped ? image.flipped(Qt::Horizontal) : image;
    if (rotation != 0)
        result = result.transformed(QTransform().rotate(rotation));
    return result;
}

QImage composite(const QRect& region, const QList<QRect>& geometries, const QList<QImage>& parts) {
    if (std::ranges::any_of(parts, &QImage::isNull))
        return {};

    const auto scale = std::ranges::max(parts, {}, &QImage::devicePixelRatio).devicePixelRatio();

    QImage image((QSizeF(region.size()) * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const auto& geometry = geometries[i];
        const QRectF target(QPointF(geometry.topLeft() - region.topLeft()) * scale, QSizeF(geometry.size()) * scale);
        painter.drawImage(target, parts[i], parts[i].rect());
    }
    painter.end();

    image.setDevicePixelRatio(scale);
    return image;
}

} // namespace caelestia::screenshot
