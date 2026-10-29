#include <qtest.h>

#include <m3colours/common.hpp>
#include <m3colours/hct.hpp>

#include <array>
#include <utility>

#include "caelestia/scheme/generator.hpp"
#include "caelestia/scheme/scheme.hpp"
#include "helpers.hpp"

using namespace caelestia::scheme;
using m3colours::Argb;
using m3colours::Hct;

namespace {

const QColor k_sourceColour = QColor::fromRgba(0xFF2962FF);

constexpr std::array k_variants = {
    std::pair{ DynamicVariant::Monochrome, "Monochrome" },
    std::pair{ DynamicVariant::Neutral, "Neutral" },
    std::pair{ DynamicVariant::TonalSpot, "TonalSpot" },
    std::pair{ DynamicVariant::Vibrant, "Vibrant" },
    std::pair{ DynamicVariant::Expressive, "Expressive" },
    std::pair{ DynamicVariant::Fidelity, "Fidelity" },
    std::pair{ DynamicVariant::Content, "Content" },
    std::pair{ DynamicVariant::Rainbow, "Rainbow" },
    std::pair{ DynamicVariant::FruitSalad, "FruitSalad" },
    std::pair{ DynamicVariant::Cmf, "Cmf" },
};

double chroma(QColor colour) {
    return Hct(Argb{ colour.rgba() }).chroma();
}

bool isGrayscale(QColor colour) {
    return colour.red() == colour.green() && colour.green() == colour.blue();
}

} // namespace

class TestGenerator : public QObject {
    Q_OBJECT

private slots:

    void darkMode() {
        const auto colours = generate(k_sourceColour, DynamicVariant::TonalSpot, Mode::Dark);
        const auto all = allColours(colours);
        for (const auto colour : all)
            QCOMPARE(colour.alpha(), 0xFF);
        QVERIFY(colours.background.red() < 0x80);
        QVERIFY(colours.onBackground.red() > 0x80);
    }

    void lightMode() {
        const auto colours = generate(k_sourceColour, DynamicVariant::TonalSpot, Mode::Light);
        const auto all = allColours(colours);
        for (const auto colour : all)
            QCOMPARE(colour.alpha(), 0xFF);
        QVERIFY(colours.background.red() > 0x80);
        QVERIFY(colours.onBackground.red() < 0x80);
    }

    void monochrome() {
        const auto colours = generate(k_sourceColour, DynamicVariant::Monochrome, Mode::Dark);
        QVERIFY(isGrayscale(colours.primary));
        QVERIFY(isGrayscale(colours.term1));
        QVERIFY(isGrayscale(colours.red));
    }

    void variantsDiffer() {
        const auto tonal = generate(k_sourceColour, DynamicVariant::TonalSpot, Mode::Dark);
        const auto vibrant = generate(k_sourceColour, DynamicVariant::Vibrant, Mode::Dark);
        QVERIFY(allColours(tonal) != allColours(vibrant));
    }

    void everyVariantIsOpaque_data() {
        QTest::addColumn<DynamicVariant>("variant");
        QTest::addColumn<Mode>("mode");

        for (const auto& [variant, name] : k_variants) {
            QTest::addRow("%s-dark", name) << variant << Mode::Dark;
            QTest::addRow("%s-light", name) << variant << Mode::Light;
        }
    }

    void everyVariantIsOpaque() {
        QFETCH(const DynamicVariant, variant);
        QFETCH(const Mode, mode);

        const auto colours = generate(k_sourceColour, variant, mode);
        const auto all = allColours(colours);
        for (const auto colour : all)
            QCOMPARE(colour.alpha(), 0xFF);
    }

    void neutralReducesChroma() {
        const auto tonal = generate(k_sourceColour, DynamicVariant::TonalSpot, Mode::Dark);
        const auto neutral = generate(k_sourceColour, DynamicVariant::Neutral, Mode::Dark);
        QVERIFY(chroma(neutral.term1) < chroma(tonal.term1));
        QVERIFY(chroma(neutral.red) < chroma(tonal.red));
    }

    void isStable() {
        const auto first = generate(k_sourceColour, DynamicVariant::Vibrant, Mode::Dark);
        const auto second = generate(k_sourceColour, DynamicVariant::Vibrant, Mode::Dark);
        QCOMPARE(allColours(first), allColours(second));
    }
};

QTEST_GUILESS_MAIN(TestGenerator)

#include "generator_test.moc"
