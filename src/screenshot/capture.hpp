#pragma once

#include <qfuture.h>
#include <qimage.h>
#include <qlist.h>
#include <qloggingcategory.h>
#include <qobject.h>
#include <qpromise.h>
#include <qsize.h>

#include <cstdint>
#include <memory>

#include "qwayland-ext-image-capture-source-v1.h"
#include "qwayland-ext-image-copy-capture-v1.h"
#include "qwayland-hyprland-workspace-image-capture-source-v1.h"

struct wl_buffer;
struct wl_display;
struct wl_output;
struct wl_registry;
struct wl_shm;
struct wl_shm_pool;

namespace caelestia::screenshot {

Q_DECLARE_LOGGING_CATEGORY(lcScreenshot)

class WorkspaceSnapshot;

class Globals {
public:
    Q_DISABLE_COPY_MOVE(Globals)

    [[nodiscard]] static Globals* instance();

    [[nodiscard]] bool supported() const;
    [[nodiscard]] bool workspaceCaptureSupported() const;
    [[nodiscard]] wl_shm* shm() const;
    [[nodiscard]] QtWayland::ext_output_image_capture_source_manager_v1& sourceManager();
    [[nodiscard]] QtWayland::hyprland_workspace_image_capture_source_manager_v1& workspaceSourceManager();
    [[nodiscard]] QtWayland::ext_image_copy_capture_manager_v1& captureManager();
    [[nodiscard]] std::unique_ptr<WorkspaceSnapshot> snapshotWorkspaces() const;

private:
    explicit Globals(wl_display* display);
    ~Globals() = default;

    static void handleGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
    static void handleGlobalRemove(void* data, wl_registry* registry, uint32_t name);

    wl_display* m_display;
    wl_registry* m_registry = nullptr;
    wl_shm* m_shm = nullptr;
    uint32_t m_workspaceManagerName = 0;
    QtWayland::ext_output_image_capture_source_manager_v1 m_sourceManager;
    QtWayland::hyprland_workspace_image_capture_source_manager_v1 m_workspaceSourceManager;
    QtWayland::ext_image_copy_capture_manager_v1 m_captureManager;
};

class ShmBuffer {
public:
    Q_DISABLE_COPY_MOVE(ShmBuffer)

    ShmBuffer(wl_shm* shm, int width, int height, uint32_t format);
    ~ShmBuffer();

    [[nodiscard]] bool valid() const;
    [[nodiscard]] wl_buffer* buffer() const;
    [[nodiscard]] const uchar* data() const;
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] int stride() const;

private:
    int m_width;
    int m_height;
    int m_stride;
    size_t m_size;
    void* m_data = nullptr;
    wl_shm_pool* m_pool = nullptr;
    wl_buffer* m_buffer = nullptr;
};

class Capture final : public QObject,
                      private QtWayland::ext_image_copy_capture_session_v1,
                      private QtWayland::ext_image_copy_capture_frame_v1 {
public:
    Capture(Globals& globals, ext_image_capture_source_v1* source, const QSize& logicalSize,
        std::shared_ptr<WorkspaceSnapshot> workspaces = nullptr);
    ~Capture() override;

    [[nodiscard]] QFuture<QImage> future();

private:
    void ext_image_copy_capture_session_v1_buffer_size(uint32_t width, uint32_t height) override;
    void ext_image_copy_capture_session_v1_shm_format(uint32_t format) override;
    void ext_image_copy_capture_session_v1_done() override;
    void ext_image_copy_capture_session_v1_stopped() override;

    void ext_image_copy_capture_frame_v1_transform(uint32_t transform) override;
    void ext_image_copy_capture_frame_v1_ready() override;
    void ext_image_copy_capture_frame_v1_failed(uint32_t reason) override;

    void fail(const char* reason);
    void finish(const QImage& image);

    wl_shm* m_shm;
    QSize m_logicalSize;
    std::shared_ptr<WorkspaceSnapshot> m_workspaces;
    QPromise<QImage> m_promise;
    ext_image_capture_source_v1* m_source;
    std::unique_ptr<ShmBuffer> m_buffer;
    int m_width = 0;
    int m_height = 0;
    QList<uint32_t> m_formats;
    uint32_t m_format = 0;
    uint32_t m_transform = 0;
    bool m_done = false;
};

} // namespace caelestia::screenshot
