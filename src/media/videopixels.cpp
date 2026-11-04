#include "videopixels.hpp"

#include <qloggingcategory.h>
#include <qrgb.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
}

#include <cmath>
#include <memory>

#include "caelestia/media/video.hpp"

namespace caelestia::media {

namespace {

Q_LOGGING_CATEGORY(lcVideo, "caelestia.scheme.video", QtInfoMsg)

struct FormatContextDeleter {
    void operator()(AVFormatContext* ctx) const { avformat_close_input(&ctx); }
};

struct CodecContextDeleter {
    void operator()(AVCodecContext* ctx) const { avcodec_free_context(&ctx); }
};

struct PacketDeleter {
    void operator()(AVPacket* packet) const { av_packet_free(&packet); }
};

struct FrameDeleter {
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const { sws_freeContext(ctx); }
};

QString avError(int code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

class VideoDecoder {
public:
    explicit VideoDecoder(const QString& path) {
        AVFormatContext* format = nullptr;
        if (const auto err = avformat_open_input(&format, path.toLocal8Bit().constData(), nullptr, nullptr); err < 0) {
            qCWarning(lcVideo) << "Failed to open video" << path << avError(err);
            return;
        }
        m_format.reset(format);

        if (const auto err = avformat_find_stream_info(format, nullptr); err < 0) {
            qCWarning(lcVideo) << "Failed to read video stream info" << path << avError(err);
            return;
        }

        const AVCodec* codec = nullptr;
        m_streamIndex = av_find_best_stream(format, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (m_streamIndex < 0) {
            qCWarning(lcVideo) << "No video stream in" << path << avError(m_streamIndex);
            return;
        }

        m_codec.reset(avcodec_alloc_context3(codec));
        if (!m_codec || avcodec_parameters_to_context(m_codec.get(), stream()->codecpar) < 0) {
            qCWarning(lcVideo) << "Failed to configure decoder for" << path;
            return;
        }
        m_codec->thread_count = 0;
        if (const auto err = avcodec_open2(m_codec.get(), codec, nullptr); err < 0) {
            qCWarning(lcVideo) << "Failed to open decoder for" << path << avError(err);
            return;
        }

        m_packet.reset(av_packet_alloc());
        m_frame.reset(av_frame_alloc());
        m_valid = m_packet && m_frame;
    }

    [[nodiscard]] bool isValid() const { return m_valid; }

    [[nodiscard]] qint64 duration() const { return m_format->duration; }

    // Seeks to the keyframe at or before `time` (AV_TIME_BASE units) and returns the first
    // decoded frame no older than `time - tolerance`, or nullptr
    const AVFrame* frameNear(qint64 time, qint64 tolerance) {
        const auto timeBase = stream()->time_base;
        const auto target = av_rescale_q(time, AV_TIME_BASE_Q, timeBase);
        const auto earliest = av_rescale_q(time - tolerance, AV_TIME_BASE_Q, timeBase);
        if (av_seek_frame(m_format.get(), m_streamIndex, target, AVSEEK_FLAG_BACKWARD) < 0)
            return nullptr;
        avcodec_flush_buffers(m_codec.get());

        while (true) {
            const auto read = av_read_frame(m_format.get(), m_packet.get());
            if (read < 0 && read != AVERROR_EOF)
                return nullptr;

            auto* packet = read >= 0 ? m_packet.get() : nullptr;
            if (packet && packet->stream_index != m_streamIndex) {
                av_packet_unref(packet);
                continue;
            }

            const auto sent = avcodec_send_packet(m_codec.get(), packet);
            if (packet)
                av_packet_unref(packet);
            if (sent < 0 && sent != AVERROR_EOF)
                return nullptr;

            while (avcodec_receive_frame(m_codec.get(), m_frame.get()) >= 0) {
                if (m_frame->best_effort_timestamp >= earliest)
                    return m_frame.get();
                av_frame_unref(m_frame.get());
            }
            if (!packet)
                return nullptr;
        }
    }

private:
    std::unique_ptr<AVFormatContext, FormatContextDeleter> m_format;
    std::unique_ptr<AVCodecContext, CodecContextDeleter> m_codec;
    std::unique_ptr<AVPacket, PacketDeleter> m_packet;
    std::unique_ptr<AVFrame, FrameDeleter> m_frame;
    int m_streamIndex = -1;
    bool m_valid = false;

    [[nodiscard]] AVStream* stream() const { return m_format->streams[m_streamIndex]; }
};

using Scaler = std::unique_ptr<SwsContext, SwsContextDeleter>;

// Converts frame to BGRA at size into dst, reusing scaler across calls
bool scaleFrame(Scaler& scaler, const AVFrame* frame, const QSize& size, std::uint8_t* dst, int stride) {
    scaler.reset(
        sws_getCachedContext(scaler.release(), frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
            size.width(), size.height(), AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (!scaler)
        return false;

    std::uint8_t* const dsts[] = { dst };
    const int strides[] = { stride };
    sws_scale(scaler.get(), frame->data, frame->linesize, 0, frame->height, dsts, strides);
    return true;
}

} // namespace

std::vector<m3colours::Argb> videoPixels(const QString& path, int samples, std::size_t pixelsPerFrame) {
    VideoDecoder decoder(path);
    if (!decoder.isValid())
        return {};

    std::vector<m3colours::Argb> pixels;
    Scaler scaler;
    std::vector<QRgb> scaled;
    const auto interval = decoder.duration() / samples;

    for (int i = 0; i < samples; ++i) {
        const auto* frame = decoder.frameNear(interval * i, interval);
        if (!frame)
            break;

        const auto size = sampleSize(frame->width, frame->height, pixelsPerFrame);
        scaled.resize(static_cast<std::size_t>(size.width()) * static_cast<std::size_t>(size.height()));
        if (!scaleFrame(scaler, frame, size, reinterpret_cast<std::uint8_t*>(scaled.data()),
                size.width() * static_cast<int>(sizeof(QRgb))))
            break;

        const auto* bits = reinterpret_cast<const m3colours::Argb*>(scaled.data());
        pixels.insert(pixels.end(), bits, bits + scaled.size());
    }
    return pixels;
}

QImage videoFrame(const QString& path, const QSize& size, Qt::AspectRatioMode mode) {
    VideoDecoder decoder(path);
    if (!decoder.isValid())
        return {};

    const auto* frame = decoder.frameNear(0, 0);
    if (!frame)
        return {};

    const QSize native(frame->width, frame->height);
    auto target = size.isEmpty() ? native : native.scaled(size, mode);
    if (target.width() > native.width() || target.height() > native.height())
        target = native;

    QImage image(target, QImage::Format_ARGB32);
    Scaler scaler;
    if (image.isNull() || !scaleFrame(scaler, frame, target, image.bits(), static_cast<int>(image.bytesPerLine())))
        return {};
    return image;
}

QSize sampleSize(int width, int height, std::size_t targetPixels) {
    const auto area = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (area <= targetPixels)
        return { width, height };

    const auto factor = std::sqrt(static_cast<double>(targetPixels) / static_cast<double>(area));
    return { static_cast<int>(std::ceil(width * factor)), static_cast<int>(std::ceil(height * factor)) };
}

} // namespace caelestia::media
