#include "capture.hpp"

#include <qguiapplication.h>
#include <qguiapplication_platform.h>

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

#include "image.hpp"

namespace caelestia::screenshot {

Q_LOGGING_CATEGORY(lcScreenshot, "caelestia.screenshot", QtInfoMsg)

namespace {

constexpr int k_bytesPerPixel = 4;

} // namespace

Globals* Globals::instance() {
    static Globals* const k_instance = [] {
        const auto* app = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
        return app ? new Globals(app->display()) : nullptr;
    }();
    return k_instance;
}

Globals::Globals(wl_display* display)
    : m_registry(wl_display_get_registry(display)) {
    static const wl_registry_listener k_listener{
        .global = handleGlobal,
        .global_remove = handleGlobalRemove,
    };
    wl_registry_add_listener(m_registry, &k_listener, this);
    wl_display_roundtrip(display);
}

bool Globals::supported() const {
    return m_shm != nullptr && m_sourceManager.isInitialized() && m_captureManager.isInitialized();
}

wl_shm* Globals::shm() const {
    return m_shm;
}

QtWayland::ext_output_image_capture_source_manager_v1& Globals::sourceManager() {
    return m_sourceManager;
}

QtWayland::ext_image_copy_capture_manager_v1& Globals::captureManager() {
    return m_captureManager;
}

void Globals::handleGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    auto* const self = static_cast<Globals*>(data);
    const auto bindVersion = static_cast<int>(std::min(version, 1u));

    if (std::strcmp(interface, wl_shm_interface.name) == 0)
        self->m_shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    else if (std::strcmp(interface, QtWayland::ext_output_image_capture_source_manager_v1::interface()->name) == 0)
        self->m_sourceManager.init(registry, name, bindVersion);
    else if (std::strcmp(interface, QtWayland::ext_image_copy_capture_manager_v1::interface()->name) == 0)
        self->m_captureManager.init(registry, name, bindVersion);
}

void Globals::handleGlobalRemove(void* data, wl_registry* registry, uint32_t name) {
    Q_UNUSED(data);
    Q_UNUSED(registry);
    Q_UNUSED(name);
}

ShmBuffer::ShmBuffer(wl_shm* shm, int width, int height, uint32_t format)
    : m_width(width)
    , m_height(height)
    , m_stride(width * k_bytesPerPixel)
    , m_size(static_cast<size_t>(m_stride) * static_cast<size_t>(height)) {
    const int fd = memfd_create("caelestia-screenshot", MFD_CLOEXEC);
    if (fd < 0) {
        qCWarning(lcScreenshot) << "Unable to create shm file:" << std::strerror(errno);
        return;
    }

    if (ftruncate(fd, static_cast<off_t>(m_size)) < 0) {
        qCWarning(lcScreenshot) << "Unable to size shm file:" << std::strerror(errno);
        close(fd);
        return;
    }

    m_data = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m_data == MAP_FAILED) {
        qCWarning(lcScreenshot) << "Unable to map shm file:" << std::strerror(errno);
        m_data = nullptr;
        close(fd);
        return;
    }

    m_pool = wl_shm_create_pool(shm, fd, static_cast<int32_t>(m_size));
    m_buffer = wl_shm_pool_create_buffer(m_pool, 0, width, height, m_stride, format);
    close(fd);
}

ShmBuffer::~ShmBuffer() {
    if (m_buffer)
        wl_buffer_destroy(m_buffer);
    if (m_pool)
        wl_shm_pool_destroy(m_pool);
    if (m_data)
        munmap(m_data, m_size);
}

bool ShmBuffer::valid() const {
    return m_buffer != nullptr;
}

wl_buffer* ShmBuffer::buffer() const {
    return m_buffer;
}

const uchar* ShmBuffer::data() const {
    return static_cast<const uchar*>(m_data);
}

int ShmBuffer::width() const {
    return m_width;
}

int ShmBuffer::height() const {
    return m_height;
}

int ShmBuffer::stride() const {
    return m_stride;
}

Capture::Capture(Globals& globals, wl_output* output, const QSize& logicalSize)
    : m_shm(globals.shm())
    , m_logicalSize(logicalSize)
    , m_source(globals.sourceManager().create_source(output)) {
    m_promise.start();
    ext_image_copy_capture_session_v1::init(globals.captureManager().create_session(m_source, 0));
}

Capture::~Capture() {
    if (ext_image_copy_capture_frame_v1::isInitialized())
        ext_image_copy_capture_frame_v1::destroy();
    m_buffer.reset();
    if (ext_image_copy_capture_session_v1::isInitialized())
        ext_image_copy_capture_session_v1::destroy();
    ext_image_capture_source_v1_destroy(m_source);
}

QFuture<QImage> Capture::future() {
    return m_promise.future();
}

void Capture::ext_image_copy_capture_session_v1_buffer_size(uint32_t width, uint32_t height) {
    if (ext_image_copy_capture_frame_v1::isInitialized())
        return;

    m_width = static_cast<int>(width);
    m_height = static_cast<int>(height);
}

void Capture::ext_image_copy_capture_session_v1_shm_format(uint32_t format) {
    if (ext_image_copy_capture_frame_v1::isInitialized())
        return;

    m_formats << format;
}

void Capture::ext_image_copy_capture_session_v1_done() {
    if (m_done || ext_image_copy_capture_frame_v1::isInitialized())
        return;

    const auto chosen = preferredShmFormat(m_formats);
    if (!chosen || m_width <= 0 || m_height <= 0) {
        fail("no usable buffer format");
        return;
    }
    m_format = *chosen;

    m_buffer = std::make_unique<ShmBuffer>(m_shm, m_width, m_height, m_format);
    if (!m_buffer->valid()) {
        fail("unable to allocate buffer");
        return;
    }

    ext_image_copy_capture_frame_v1::init(create_frame());
    attach_buffer(m_buffer->buffer());
    damage_buffer(0, 0, m_width, m_height);
    capture();
}

void Capture::ext_image_copy_capture_session_v1_stopped() {
    fail("session stopped");
}

void Capture::ext_image_copy_capture_frame_v1_transform(uint32_t transform) {
    m_transform = transform;
}

void Capture::ext_image_copy_capture_frame_v1_ready() {
    const QImage raw(
        m_buffer->data(), m_buffer->width(), m_buffer->height(), m_buffer->stride(), imageFormat(m_format));
    auto image = applyTransform(raw.copy(), m_transform);
    image.setDevicePixelRatio(static_cast<qreal>(image.width()) / m_logicalSize.width());
    finish(image);
}

void Capture::ext_image_copy_capture_frame_v1_failed(uint32_t reason) {
    switch (reason) {
    case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS:
        fail("buffer constraints mismatch");
        break;
    case EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED:
        fail("session stopped");
        break;
    default:
        fail("unknown error");
        break;
    }
}

void Capture::fail(const char* reason) {
    if (m_done)
        return;

    qCWarning(lcScreenshot) << "Capture failed:" << reason;
    finish({});
}

void Capture::finish(const QImage& image) {
    if (m_done)
        return;

    m_done = true;
    m_promise.addResult(image);
    m_promise.finish();
    deleteLater();
}

} // namespace caelestia::screenshot
