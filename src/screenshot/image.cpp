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

std::optional<uint32_t> preferredShmFormat(const QList<uint32_t>& offered, bool preferAlpha) {
    const auto usable = [&offered](const FormatMapping& mapping) {
        return offered.contains(mapping.shm);
    };
    const auto usableWithAlpha = [&usable](const FormatMapping& mapping) {
        return usable(mapping) && QImage::toPixelFormat(mapping.image).alphaUsage() == QPixelFormat::UsesAlpha;
    };

    const auto* chosen = preferAlpha ? std::ranges::find_if(k_formats, usableWithAlpha) : std::end(k_formats);
    if (chosen == std::end(k_formats))
        chosen = std::ranges::find_if(k_formats, usable);
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

bool isBlank(const QImage& image) {
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const quint32*>(image.constScanLine(y));
        if (!std::all_of(line, line + image.width(), [](quint32 pixel) {
                return pixel == 0;
            }))
            return false;
    }
    return true;
}

QImage applyTransform(const QImage& image, uint32_t transform) {
    const auto flipped = transform >= WL_OUTPUT_TRANSFORM_FLIPPED;
    const auto rotation = static_cast<int>(transform & 3u) * 90;

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
