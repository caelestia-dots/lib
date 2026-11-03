#include <qtest.h>

#include <wayland-client-protocol.h>

#include <algorithm>

#include "image.hpp"

using namespace caelestia;

namespace {

constexpr QRgb k_colours[] = {
    0xffff0000,
    0xff00ff00,
    0xff0000ff,
    0xffffff00,
    0xffff00ff,
    0xff00ffff,
};

QImage pattern() {
    QImage image(3, 2, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 3; ++x)
            image.setPixel(x, y, k_colours[y * 3 + x]);
    return image;
}

QString layout(const QImage& image) {
    QString result;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto* found = std::ranges::find(k_colours, image.pixel(x, y));
            result += QChar(u'A' + static_cast<int>(found - std::begin(k_colours)));
        }
        result += u'/';
    }
    return result;
}

QImage solid(int width, int height, QRgb colour, qreal dpr) {
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(colour);
    image.setDevicePixelRatio(dpr);
    return image;
}

} // namespace

class TestImage : public QObject {
    Q_OBJECT

private slots:

    void preferredFormatFollowsOrder() {
        QCOMPARE(screenshot::preferredShmFormat({ WL_SHM_FORMAT_ABGR8888, WL_SHM_FORMAT_ARGB8888 }),
            std::optional<uint32_t>(WL_SHM_FORMAT_ARGB8888));
        QCOMPARE(screenshot::preferredShmFormat({ WL_SHM_FORMAT_XRGB8888, WL_SHM_FORMAT_ARGB8888 }),
            std::optional<uint32_t>(WL_SHM_FORMAT_XRGB8888));
        QVERIFY(!screenshot::preferredShmFormat({ WL_SHM_FORMAT_RGB565 }).has_value());
        QVERIFY(!screenshot::preferredShmFormat({}).has_value());
    }

    void imageFormatMapping() {
        QCOMPARE(screenshot::imageFormat(WL_SHM_FORMAT_XRGB8888), QImage::Format_RGB32);
        QCOMPARE(screenshot::imageFormat(WL_SHM_FORMAT_ARGB8888), QImage::Format_ARGB32_Premultiplied);
        QCOMPARE(screenshot::imageFormat(WL_SHM_FORMAT_XBGR8888), QImage::Format_RGBX8888);
        QCOMPARE(screenshot::imageFormat(WL_SHM_FORMAT_ABGR8888), QImage::Format_RGBA8888_Premultiplied);
        QCOMPARE(screenshot::imageFormat(WL_SHM_FORMAT_RGB565), QImage::Format_Invalid);
    }

    void transform_data() {
        QTest::addColumn<uint32_t>("transform");
        QTest::addColumn<QString>("expected");

        QTest::newRow("normal") << static_cast<uint32_t>(WL_OUTPUT_TRANSFORM_NORMAL) << "ABC/DEF/";
        QTest::newRow("90") << static_cast<uint32_t>(WL_OUTPUT_TRANSFORM_90) << "DA/EB/FC/";
        QTest::newRow("180") << static_cast<uint32_t>(WL_OUTPUT_TRANSFORM_180) << "FED/CBA/";
        QTest::newRow("270") << static_cast<uint32_t>(WL_OUTPUT_TRANSFORM_270) << "CF/BE/AD/";
        QTest::newRow("flipped") << static_cast<uint32_t>(WL_OUTPUT_TRANSFORM_FLIPPED) << "CBA/FED/";
        QTest::newRow("flipped-90") << static_cast<uint32_t>(WL_OUTPUT_TRANSFORM_FLIPPED_90) << "AD/BE/CF/";
        QTest::newRow("flipped-180") << static_cast<uint32_t>(WL_OUTPUT_TRANSFORM_FLIPPED_180) << "DEF/ABC/";
        QTest::newRow("flipped-270") << static_cast<uint32_t>(WL_OUTPUT_TRANSFORM_FLIPPED_270) << "FC/EB/DA/";
    }

    void transform() {
        QFETCH(const uint32_t, transform);
        QFETCH(const QString, expected);
        QCOMPARE(layout(screenshot::applyTransform(pattern(), transform)), expected);
    }

    void compositeMixedScale() {
        const QList<QRect> geometries{ QRect(0, 0, 4, 4), QRect(4, 0, 4, 4) };
        const QList<QImage> parts{ solid(4, 4, 0xffff0000, 1), solid(8, 8, 0xff0000ff, 2) };

        const auto image = screenshot::composite(QRect(2, 1, 4, 2), geometries, parts);
        QCOMPARE(image.size(), QSize(8, 4));
        QCOMPARE(image.devicePixelRatio(), 2.0);
        QCOMPARE(image.pixel(0, 0), 0xffff0000);
        QCOMPARE(image.pixel(3, 3), 0xffff0000);
        QCOMPARE(image.pixel(4, 0), 0xff0000ff);
        QCOMPARE(image.pixel(7, 3), 0xff0000ff);
    }

    void compositeLeavesGapsTransparent() {
        const auto image =
            screenshot::composite(QRect(0, 0, 4, 2), { QRect(0, 0, 2, 2) }, { solid(2, 2, 0xffff0000, 1) });
        QCOMPARE(image.size(), QSize(4, 2));
        QCOMPARE(image.pixel(1, 1), 0xffff0000);
        QCOMPARE(image.pixel(3, 1), 0u);
    }

    void compositeFailsOnNullPart() {
        QVERIFY(screenshot::composite(QRect(0, 0, 2, 2), { QRect(0, 0, 2, 2) }, { QImage() }).isNull());
    }
};

QTEST_GUILESS_MAIN(TestImage)

#include "image_test.moc"
