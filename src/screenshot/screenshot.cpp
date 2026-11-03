#include "caelestia/screenshot/screenshot.hpp"

#include <qguiapplication.h>
#include <qscreen.h>
#include <qscreen_platform.h>

#include "capture.hpp"
#include "image.hpp"

namespace caelestia::screenshot {

namespace {

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

} // namespace caelestia::screenshot
