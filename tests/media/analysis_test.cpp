#include <qfile.h>
#include <qfileinfo.h>
#include <qimage.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include "caelestia/media/analysis.hpp"
#include "caelestia/media/video.hpp"
#include "analysiscache.hpp"
#include "common.hpp"
#include "videopixels.hpp"

using namespace caelestia::media;
using Qt::StringLiterals::operator""_s;

namespace {

const QColor k_sourceColour = QColor::fromRgba(0xFF2962FF);
const QColor k_otherColour = QColor::fromRgba(0xFFD50000);
constexpr int k_tolerance = 4;

bool closeTo(QColor a, QColor b) {
    return qAbs(a.red() - b.red()) <= k_tolerance && qAbs(a.green() - b.green()) <= k_tolerance &&
           qAbs(a.blue() - b.blue()) <= k_tolerance;
}

bool sameAnalysis(const Analysis& a, const Analysis& b) {
    return a.colours == b.colours && a.dominant == b.dominant &&
           qFuzzyCompare(a.colourfulness + 1, b.colourfulness + 1) && qFuzzyCompare(a.luminance + 1, b.luminance + 1) &&
           qFuzzyCompare(a.tone + 1, b.tone + 1);
}

QImage solid(QRgb colour, int width = 8, int height = 8) {
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(colour);
    return image;
}

} // namespace

class TestAnalysis : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QImage m_image;
    QString m_path;

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        qputenv("XDG_CACHE_HOME", m_root.path().toUtf8());

        m_image = solid(k_sourceColour.rgba());
        m_path = m_root.filePath(u"wallpaper.png"_s);
        QVERIFY(m_image.save(m_path));
    }

    void supportedFormats() {
        QVERIFY(isSupported(m_path));
        QVERIFY(!isSupported(m_root.path()));
        QVERIFY(!isSupported(m_root.filePath(u"missing.png"_s)));

        const auto text = m_root.filePath(u"notes.txt"_s);
        writeFile(text, "hello");
        QVERIFY(!isSupported(text));
    }

    void coloursFromImage() {
        const auto analysis = analyseImage(m_image);
        QVERIFY(!analysis.colours.isEmpty());
        QCOMPARE(analysis.colours.first(), k_sourceColour);
    }

    void dominantPrefersMostCommon() {
        auto image = solid(k_sourceColour.rgba());
        for (int x = 0; x < 2; ++x)
            image.setPixel(x, 0, k_otherColour.rgba());
        QCOMPARE(analyseImage(image).dominant, k_sourceColour);
    }

    void coloursRankDominantFirst() {
        auto image = solid(k_sourceColour.rgba());
        for (int x = 0; x < 2; ++x)
            image.setPixel(x, 0, k_otherColour.rgba());

        const auto colours = analyseImage(image).colours;
        QVERIFY(colours.size() >= 2);
        QCOMPARE(colours.first(), k_sourceColour);
    }

    void analysisConvertsFormat() {
        QImage rgb888(8, 8, QImage::Format_RGB888);
        rgb888.fill(k_sourceColour.rgba());
        QVERIFY(sameAnalysis(analyseImage(rgb888), analyseImage(m_image)));
    }

    void colourfulnessIsZeroForGrey() { QVERIFY(qFuzzyIsNull(analyseImage(solid(0xFF808080)).colourfulness)); }

    void colourfulnessIsPositiveForColour() { QVERIFY(analyseImage(m_image).colourfulness > 0); }

    void luminanceSpansBlackToWhite() {
        QVERIFY(qFuzzyIsNull(analyseImage(solid(0xFF000000)).luminance));
        QVERIFY(qFuzzyCompare(analyseImage(solid(0xFFFFFFFF)).luminance, 1.0));
    }

    void toneSpansBlackToWhite() {
        QVERIFY(analyseImage(solid(0xFF000000)).tone < 1);
        QVERIFY(analyseImage(solid(0xFFFFFFFF)).tone > 99);
    }

    void ignoresTransparentPixels() {
        QImage image(8, 8, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        image.setPixel(0, 0, k_sourceColour.rgba());
        QCOMPARE(analyseImage(image).dominant, k_sourceColour);
    }

    void nullImageIsEmpty() { QVERIFY(sameAnalysis(analyseImage(QImage()), Analysis{})); }

    void analysisFromPath() { QVERIFY(sameAnalysis(analyseForPath(m_path).result(), analyseImage(m_image))); }

    void analysisFromAnimatedImage() {
        const auto path = QFINDTESTDATA("data/animated.gif");
        QVERIFY(!path.isEmpty());
        const auto analysis = analyseForPath(path).result();
        QVERIFY(!analysis.colours.isEmpty());
        QVERIFY(analysis.colours.first() != k_otherColour);
        QVERIFY(closeTo(analysis.colours.first(), k_sourceColour));
    }

    void analysisFromVideo() {
        const auto path = QFINDTESTDATA("data/video.mp4");
        QVERIFY(!path.isEmpty());
        const auto analysis = analyseForPath(path).result();
        QVERIFY(!analysis.colours.isEmpty());
        QVERIFY(analysis.colours.first() != k_otherColour);
        QVERIFY(closeTo(analysis.colours.first(), k_sourceColour));
    }

    void videoFrameFromVideo() {
        const auto path = QFINDTESTDATA("data/video.mp4");
        QVERIFY(!path.isEmpty());

        const auto native = videoFrame(path);
        QVERIFY(!native.isNull());
        QVERIFY(closeTo(native.pixelColor(native.width() / 2, native.height() / 2), k_otherColour));

        const auto fit = videoFrame(path, QSize(4, 4), Qt::IgnoreAspectRatio);
        QCOMPARE(fit.size(), QSize(4, 4));

        const auto large = videoFrame(path, native.size() * 2);
        QCOMPARE(large.size(), native.size());
    }

    void videoFrameFromMissingPath() { QVERIFY(videoFrame(m_root.filePath(u"missing.mp4"_s)).isNull()); }

    void analysisFromMissingPath() {
        QVERIFY(sameAnalysis(analyseForPath(m_root.filePath(u"missing.png"_s)).result(), Analysis{}));
    }

    void analysisFromPathUsesCache() {
        const auto path = m_root.filePath(u"cached.png"_s);
        QVERIFY(QFile::copy(m_path, path));
        const auto analysis = analyseForPath(path).result();
        QVERIFY(!analysis.colours.isEmpty());

        QFile file(path);
        const auto modified = QFileInfo(file).lastModified();
        const auto size = file.size();
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(QByteArray(size, '\0')), size);
        QVERIFY(file.setFileTime(modified, QFileDevice::FileModificationTime));
        file.close();

        QVERIFY(sameAnalysis(analyseForPath(path).result(), analysis));
    }

    void sampleSizeKeepsSmallImages() {
        QCOMPARE(sampleSize(8, 8, 1000), QSize(8, 8));
        QCOMPARE(sampleSize(40, 25, 1000), QSize(40, 25));
    }

    void sampleSizeDownscalesPreservingAspect() {
        constexpr std::size_t k_target = 1'000'000;
        const auto size = sampleSize(4000, 2000, k_target);
        const auto area = static_cast<std::size_t>(size.width()) * static_cast<std::size_t>(size.height());
        QVERIFY(area >= k_target);
        QVERIFY(area <= k_target + static_cast<std::size_t>(size.width() + size.height()));
        QVERIFY(qAbs(static_cast<double>(size.width()) / size.height() - 2.0) < 0.02);

        QVERIFY(sampleSize(1, 100'000, 1000).width() >= 1);
    }

    void analysisIsIndependentOfImageSize() {
        const auto small = analyseImage(solid(k_sourceColour.rgba()));
        const auto large = analyseImage(solid(k_sourceColour.rgba(), 2048, 1024));
        QVERIFY(!large.colours.isEmpty());
        QCOMPARE(large.dominant, small.dominant);
        QCOMPARE(large.colours.first(), small.colours.first());
        QVERIFY(qAbs(large.luminance - small.luminance) < 1e-6);
        QVERIFY(qAbs(large.tone - small.tone) < 1e-6);
    }

    void nonImageFileIsEmpty() {
        const auto path = m_root.filePath(u"not_an_image.txt"_s);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("hello"), 5);
        file.close();

        QVERIFY(sameAnalysis(analyseForPath(path).result(), Analysis{}));
    }

    void corruptCacheEntryIsReanalysed() {
        const auto path = m_root.filePath(u"corrupt.png"_s);
        QVERIFY(QFile::copy(m_path, path));
        const auto key = AnalysisCache::keyForPath(path);
        const QByteArray garbage(7, '\x01');

        AnalysisCache::instance().store(key, garbage);
        QCOMPARE(AnalysisCache::instance().lookup(key).result(), garbage);

        QVERIFY(sameAnalysis(analyseForPath(path).result(), analyseImage(m_image)));
        QCOMPARE_NE(AnalysisCache::instance().lookup(key).result(), garbage);
    }

    void truncatedCacheEntryIsReanalysed() {
        const auto path = m_root.filePath(u"truncated.png"_s);
        QVERIFY(QFile::copy(m_path, path));
        const auto key = AnalysisCache::keyForPath(path);

        QVERIFY(!analyseForPath(path).result().colours.isEmpty());
        const auto valid = AnalysisCache::instance().lookup(key).result();
        QVERIFY(valid.size() > 4);

        const auto truncated = valid.chopped(4);
        AnalysisCache::instance().store(key, truncated);
        QCOMPARE(AnalysisCache::instance().lookup(key).result(), truncated);

        QVERIFY(sameAnalysis(analyseForPath(path).result(), analyseImage(m_image)));
        QCOMPARE(AnalysisCache::instance().lookup(key).result(), valid);
    }
};

QTEST_GUILESS_MAIN(TestAnalysis)

#include "analysis_test.moc"
