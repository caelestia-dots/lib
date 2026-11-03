#include "caelestia/screenshot/screenshot.hpp"

#include <qbuffer.h>
#include <qdatetime.h>
#include <qguiapplication.h>
#include <qscreen.h>
#include <qscreen_platform.h>

#include "caelestia/utils/cache.hpp"
#include "capture.hpp"
#include "image.hpp"

namespace caelestia::screenshot {

using Qt::StringLiterals::operator""_s;

namespace {

const QStringView k_cacheExt = u".png";
const int k_cacheVersion = 1;

QFuture<QImage> failed() {
    return QtFuture::makeReadyValueFuture(QImage());
}

QFuture<QImage> captureScreen(Globals& globals, const QScreen* screen) {
    const auto* native = screen ? screen->nativeInterface<QNativeInterface::QWaylandScreen>() : nullptr;
    if (native == nullptr) {
        qCWarning(lcScreenshot) << "Screen is not a Wayland output";
        return failed();
    }

    return (new Capture(globals, native->output(), screen->size()))->future();
}

Globals* globalsIfSupported() {
    auto* const globals = Globals::instance();
    return globals != nullptr && globals->supported() ? globals : nullptr;
}

utils::FileCache& imageCache() {
    static utils::FileCache s_cache(u"screenshots"_s, 512ll * 1024 * 1024, k_cacheExt.toString(), k_cacheVersion);
    return s_cache;
}

} // namespace

bool isSupported() {
    return globalsIfSupported() != nullptr;
}

QFuture<QImage> capture() {
    QRect region;
    const auto screens = QGuiApplication::screens();
    for (const auto* screen : screens)
        region |= screen->geometry();
    return capture(region);
}

QFuture<QImage> capture(const QScreen* screen) {
    auto* const globals = globalsIfSupported();
    if (globals == nullptr) {
        qCWarning(lcScreenshot) << "Screen capture is not supported by the compositor";
        return failed();
    }

    return captureScreen(*globals, screen);
}

QFuture<QImage> capture(const QRect& region) {
    auto* const globals = globalsIfSupported();
    if (globals == nullptr) {
        qCWarning(lcScreenshot) << "Screen capture is not supported by the compositor";
        return failed();
    }

    QList<const QScreen*> intersecting;
    const auto screens = QGuiApplication::screens();
    for (const auto* screen : screens) {
        if (screen->geometry().intersects(region))
            intersecting << screen;
    }

    if (intersecting.isEmpty()) {
        qCWarning(lcScreenshot) << "Region" << region << "does not intersect any screen";
        return failed();
    }

    QList<QRect> geometries;
    QList<QFuture<QImage>> futures;
    for (const auto* screen : std::as_const(intersecting)) {
        geometries << screen->geometry();
        futures << captureScreen(*globals, screen);
    }

    return QtFuture::whenAll(futures.begin(), futures.end())
        .then([region, geometries](const QList<QFuture<QImage>>& results) {
            QList<QImage> parts;
            for (const auto& result : results)
                parts << result.result();
            return composite(region, geometries, parts);
        });
}

QFuture<QString> cache(const QImage& image) {
    const auto key = QString::number(QDateTime::currentMSecsSinceEpoch());
    return imageCache()
        .store(key,
            [image] {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                image.save(&buffer, "PNG");
                return bytes;
            })
        .then([key](bool) {
            return imageCache().dir() + u'/' + key + k_cacheExt;
        });
}

} // namespace caelestia::screenshot
