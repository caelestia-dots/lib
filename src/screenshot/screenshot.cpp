#include "caelestia/screenshot/screenshot.hpp"

#include <qguiapplication.h>
#include <qpointer.h>
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

wl_output* outputOf(const QScreen* screen) {
    const auto* native = screen ? screen->nativeInterface<QNativeInterface::QWaylandScreen>() : nullptr;
    return native ? native->output() : nullptr;
}

QFuture<QImage> captureOutput(Globals& globals, const QSize& size, wl_output* output) {
    return (new Capture(globals, globals.sourceManager().create_source(output), size))->future();
}

QFuture<QImage> captureWorkspace(
    Globals& globals, const QScreen* screen, wl_output* output, std::shared_ptr<WorkspaceSnapshot> workspaces) {
    const auto name = screen->name();
    const auto size = screen->size();
    auto* const workspace = workspaces->activeWorkspace(output);
    if (workspace == nullptr) {
        qCWarning(lcScreenshot) << "No visible workspace for" << name << "- capturing output instead";
        return captureOutput(globals, size, output);
    }

    auto* const source = globals.workspaceSourceManager().create_source(
        workspace, QtWayland::hyprland_workspace_image_capture_source_manager_v1::capture_mode_everything);
    return (new Capture(globals, source, size, std::move(workspaces)))
        ->future()
        .then([&globals, guard = QPointer<const QScreen>(screen), name, size](const QImage& image) {
            if (!image.isNull())
                return QtFuture::makeReadyValueFuture(image);
            qCWarning(lcScreenshot) << "Workspace capture failed for" << name << "- capturing output instead";
            auto* const output = outputOf(guard);
            return output ? captureOutput(globals, size, output) : failed();
        })
        .unwrap();
}

QFuture<QImage> captureScreen(Globals& globals, const QScreen* screen, std::shared_ptr<WorkspaceSnapshot> workspaces) {
    auto* const output = outputOf(screen);
    if (output == nullptr) {
        qCWarning(lcScreenshot) << "Screen is not a Wayland output";
        return failed();
    }

    if (workspaces)
        return captureWorkspace(globals, screen, output, std::move(workspaces));
    return captureOutput(globals, screen->size(), output);
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

    return captureScreen(
        *globals, screen, globals->workspaceCaptureSupported() ? globals->snapshotWorkspaces() : nullptr);
}

QFuture<QImage> capture(const QRect& region) {
    auto* globals = Globals::instance();
    if (globals == nullptr || !globals->supported()) {
        qCWarning(lcScreenshot) << "Screen capture is not supported by the compositor";
        return failed();
    }

    QList<QRect> geometries;
    QList<QFuture<QImage>> futures;
    QList<const QScreen*> intersecting;
    std::shared_ptr<WorkspaceSnapshot> workspaces;
    const auto screens = QGuiApplication::screens();
    for (const auto* screen : screens) {
        if (!screen->geometry().intersects(region))
            continue;
        geometries << screen->geometry();
        intersecting << screen;
    }

    if (geometries.isEmpty()) {
        qCWarning(lcScreenshot) << "Region" << region << "does not intersect any screen";
        return failed();
    }

    if (globals->workspaceCaptureSupported())
        workspaces = globals->snapshotWorkspaces();
    for (const auto* screen : std::as_const(intersecting))
        futures << captureScreen(*globals, screen, workspaces);

    return QtFuture::whenAll(futures.begin(), futures.end())
        .then([region, geometries](const QList<QFuture<QImage>>& results) {
            QList<QImage> parts;
            for (const auto& result : results)
                parts << result.result();
            return composite(region, geometries, parts);
        });
}

} // namespace caelestia::screenshot
