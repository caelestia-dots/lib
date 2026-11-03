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

} // namespace

bool isSupported() {
    const auto* globals = Globals::instance();
    return globals != nullptr && globals->supported();
}

QFuture<QImage> capture(QScreen* screen) {
    auto* globals = Globals::instance();
    if (globals == nullptr || !globals->supported()) {
        qCWarning(lcScreenshot) << "Screen capture is not supported by the compositor";
        return failed();
    }

    const auto* native = screen ? screen->nativeInterface<QNativeInterface::QWaylandScreen>() : nullptr;
    if (native == nullptr) {
        qCWarning(lcScreenshot) << "Screen is not a Wayland output";
        return failed();
    }

    return (new Capture(*globals, native->output(), screen->size()))->future();
}

QFuture<QImage> capture(const QRect& region) {
    QList<QRect> geometries;
    QList<QFuture<QImage>> futures;
    const auto screens = QGuiApplication::screens();
    for (auto* screen : screens) {
        if (!screen->geometry().intersects(region))
            continue;
        geometries << screen->geometry();
        futures << capture(screen);
    }

    if (geometries.isEmpty()) {
        qCWarning(lcScreenshot) << "Region" << region << "does not intersect any screen";
        return failed();
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
