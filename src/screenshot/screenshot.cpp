#include "caelestia/screenshot/screenshot.hpp"

#include <qguiapplication.h>
#include <qscreen.h>
#include <qscreen_platform.h>

#include <memory>
#include <utility>

#include "capture.hpp"
#include "image.hpp"
#include "workspace.hpp"

namespace caelestia::screenshot {

namespace {

QFuture<QImage> failed() {
    return QtFuture::makeReadyValueFuture(QImage());
}

QFuture<QImage> captureOutput(Globals& globals, const QScreen* screen, wl_output* output) {
    return (new Capture(globals, globals.sourceManager().create_source(output), screen->size()))->future();
}

QFuture<QImage> captureWorkspace(Globals& globals, const QScreen* screen, wl_output* output) {
    auto workspaces = globals.snapshotWorkspaces();
    auto* const workspace = workspaces->activeWorkspace(output);
    if (workspace == nullptr) {
        qCWarning(lcScreenshot) << "No visible workspace for" << screen->name() << "- capturing output instead";
        return captureOutput(globals, screen, output);
    }

    auto* const source = globals.workspaceSourceManager().create_source(
        workspace, QtWayland::hyprland_workspace_image_capture_source_manager_v1::capture_mode_everything);
    return (new Capture(globals, source, screen->size(), std::move(workspaces)))
        ->future()
        .then([&globals, screen, output](const QImage& image) {
            if (!image.isNull())
                return QtFuture::makeReadyValueFuture(image);
            qCWarning(lcScreenshot) << "Workspace capture failed for" << screen->name() << "- capturing output instead";
            return captureOutput(globals, screen, output);
        })
        .unwrap();
}

} // namespace

bool isSupported() {
    const auto* globals = Globals::instance();
    return globals != nullptr && globals->supported();
}

QFuture<QImage> capture(const QScreen* screen) {
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

    auto* const output = native->output();
    if (globals->workspaceCaptureSupported())
        return captureWorkspace(*globals, screen, output);
    return captureOutput(*globals, screen, output);
}

QFuture<QImage> capture(const QRect& region) {
    QList<QRect> geometries;
    QList<QFuture<QImage>> futures;
    const auto screens = QGuiApplication::screens();
    for (const auto* screen : screens) {
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
