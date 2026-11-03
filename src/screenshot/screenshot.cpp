#include "caelestia/screenshot/screenshot.hpp"

#include <qguiapplication.h>
#include <qpainter.h>
#include <qscreen.h>
#include <qscreen_platform.h>

#include <algorithm>

#include "capture.hpp"

namespace caelestia::screenshot {

namespace {

QFuture<QImage> failed() {
    return QtFuture::makeReadyValueFuture(QImage());
}

QImage composite(const QRect& region, const QList<QScreen*>& screens, const QList<QFuture<QImage>>& results) {
    const auto scale = std::ranges::max(screens, {}, &QScreen::devicePixelRatio)->devicePixelRatio();

    QImage image((QSizeF(region.size()) * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    for (qsizetype i = 0; i < screens.size(); ++i) {
        const auto part = results[i].result();
        if (part.isNull())
            return {};

        const auto geometry = screens[i]->geometry();
        const QRectF target(QPointF(geometry.topLeft() - region.topLeft()) * scale, QSizeF(geometry.size()) * scale);
        painter.drawImage(target, part, part.rect());
    }
    painter.end();

    image.setDevicePixelRatio(scale);
    return image;
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

    return (new Capture(*globals, native->output(), screen->devicePixelRatio()))->future();
}

QFuture<QImage> capture(const QRect& region) {
    QList<QScreen*> screens;
    QList<QFuture<QImage>> futures;
    const auto all = QGuiApplication::screens();
    for (auto* screen : all) {
        if (!screen->geometry().intersects(region))
            continue;
        screens << screen;
        futures << capture(screen);
    }

    if (screens.isEmpty()) {
        qCWarning(lcScreenshot) << "Region" << region << "does not intersect any screen";
        return failed();
    }

    return QtFuture::whenAll(futures.begin(), futures.end())
        .then([region, screens](const QList<QFuture<QImage>>& results) {
            return composite(region, screens, results);
        });
}

} // namespace caelestia::screenshot
