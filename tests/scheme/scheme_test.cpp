#include <qdatastream.h>
#include <qdir.h>
#include <qdiriterator.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qimage.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <utility>

#include "caelestia/config/paths.hpp"
#include "caelestia/scheme/generator.hpp"
#include "caelestia/scheme/scheme.hpp"
#include "common.hpp"
#include "helpers.hpp"
#include "schemecache.hpp"

using namespace caelestia::scheme;
using Qt::StringLiterals::operator""_s;

namespace {

const QColor k_sourceColour = QColor::fromRgba(0xFF2962FF);

constexpr std::array k_variantNames = {
    std::pair{ DynamicVariant::Monochrome, QStringView(u"monochrome") },
    std::pair{ DynamicVariant::Neutral, QStringView(u"neutral") },
    std::pair{ DynamicVariant::TonalSpot, QStringView(u"tonalspot") },
    std::pair{ DynamicVariant::Vibrant, QStringView(u"vibrant") },
    std::pair{ DynamicVariant::Expressive, QStringView(u"expressive") },
    std::pair{ DynamicVariant::Fidelity, QStringView(u"fidelity") },
    std::pair{ DynamicVariant::Content, QStringView(u"content") },
    std::pair{ DynamicVariant::Rainbow, QStringView(u"rainbow") },
    std::pair{ DynamicVariant::FruitSalad, QStringView(u"fruitsalad") },
    std::pair{ DynamicVariant::Cmf, QStringView(u"cmf") },
};

QByteArray toBytes(const SchemeColours& colours) {
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    const auto named = namedColours(colours);
    for (const auto& [name, colour] : named)
        stream << colour.rgba();
    return bytes;
}

QStringList names(const QList<std::shared_ptr<Scheme>>& schemes) {
    QStringList out;
    for (const auto& scheme : schemes)
        out.append(scheme->name());
    return out;
}

QStringList names(const QList<Flavour>& flavours) {
    QStringList out;
    for (const auto& flavour : flavours)
        out.append(flavour.name());
    return out;
}

Flavour flavourFor(const std::shared_ptr<Scheme>& scheme, const QString& name) {
    if (!scheme)
        throw std::invalid_argument("no such scheme");
    const auto flavour = scheme->flavour(name);
    if (!flavour)
        throw std::invalid_argument("no such flavour");
    return *flavour;
}

SchemeData dataFor(const QString& name, const QString& flavour, Mode mode) {
    return flavourFor(Scheme::forName(name).result(), flavour).data(mode).result();
}

} // namespace

class TestScheme : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QString m_path;

    void installCustomScheme(const QString& source, const QString& name, const QString& flavour) {
        const auto dir = caelestia::config::ConfigPaths::customSchemesDir() + u"/%1/%2"_s.arg(name, flavour);
        QVERIFY(QDir().mkpath(dir));
        QVERIFY(QFile::copy(source, dir + u"/dark.txt"_s));
    }

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        qputenv("XDG_CACHE_HOME", m_root.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", m_root.path().toUtf8());
        qputenv("XDG_STATE_HOME", m_root.path().toUtf8());

        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(k_sourceColour.rgba());
        m_path = m_root.filePath(u"wallpaper.png"_s);
        QVERIFY(image.save(m_path));
    }

    void bundledSchemeLoads() {
        const auto scheme = Scheme::forName(u"caelestia"_s).result();
        QVERIFY(scheme);
        QCOMPARE(scheme->name(), u"caelestia"_s);
        QCOMPARE(names(scheme->flavours()), QStringList{ u"default"_s });

        const auto flavour = flavourFor(scheme, u"default"_s);
        QCOMPARE(flavour.name(), u"default"_s);
        QCOMPARE(flavour.modes(), (QList<Mode>{ Mode::Light, Mode::Dark }));

        const auto data = flavour.data(Mode::Dark).result();
        QCOMPARE(data.name, u"caelestia"_s);
        QCOMPARE(data.flavour, u"default"_s);
        QCOMPARE(data.mode, Mode::Dark);
        QCOMPARE(data.colours.background, QColor::fromRgba(0xFF0A0F0F));
        QCOMPARE(data.colours.onBackground, QColor::fromRgba(0xFFDCE8E6));
    }

    void bundledSchemeLightMode() {
        const auto data = dataFor(u"caelestia"_s, u"default"_s, Mode::Light);
        QCOMPARE(data.mode, Mode::Light);
        QVERIFY(data.colours.background.red() > 0x80);
    }

    void bundledSchemesHaveAllColours() {
        QDirIterator it(u":/schemes"_s, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info(it.next());
            if (!info.isFile())
                continue;

            const auto parts = info.path().split(u'/', Qt::SkipEmptyParts);
            const auto& name = parts[parts.size() - 2];
            const auto& flavour = parts.last();
            const auto mode = info.baseName() == u"light" ? Mode::Light : Mode::Dark;
            const auto tag = info.filePath().toUtf8();

            const auto data = dataFor(name, flavour, mode);
            QCOMPARE(data.name, name);
            QCOMPARE(data.flavour, flavour);
            QCOMPARE(data.mode, mode);
            const auto all = allColours(data.colours);
            for (const auto colour : all)
                QVERIFY2(colour.alpha() == 0xFF, tag.constData());
        }
    }

    void unknownSchemeIsNull() {
        QVERIFY(!Scheme::forName(u"missing"_s).result());
        QVERIFY(!Scheme::forName(QString()).result());
        QVERIFY(!Scheme::forName(u"caelestia/default"_s).result());
    }

    void unknownFlavourIsNullopt() {
        const auto scheme = Scheme::forName(u"caelestia"_s).result();
        QVERIFY(scheme);
        QVERIFY(!scheme->flavour(u"missing"_s).has_value());
    }

    void allListsBundledAndCustom() {
        installCustomScheme(u":/schemes/dracula/medium/dark.txt"_s, u"listed"_s, u"custom"_s);
        QVERIFY(QDir().mkpath(caelestia::config::ConfigPaths::customSchemesDir() + u"/empty/custom"_s));

        const auto schemes = Scheme::all().result();
        const auto listed = names(schemes);
        QVERIFY(listed.contains(u"caelestia"_s));
        QVERIFY(listed.contains(u"gruvbox"_s));
        QVERIFY(listed.contains(u"listed"_s));
        QVERIFY(!listed.contains(u"empty"_s));
        QCOMPARE(listed.size(), QSet<QString>(listed.cbegin(), listed.cend()).size());
        for (const auto& scheme : schemes)
            QVERIFY2(!scheme->flavours().isEmpty(), scheme->name().toUtf8().constData());
    }

    void allSnapshotsCatalogue() {
        installCustomScheme(u":/schemes/dracula/medium/dark.txt"_s, u"snapshot"_s, u"custom"_s);
        const auto schemes = Scheme::all().result();
        QVERIFY(names(schemes).contains(u"snapshot"_s));
        QVERIFY(QDir(caelestia::config::ConfigPaths::customSchemesDir() + u"/snapshot"_s).removeRecursively());
        QVERIFY(names(schemes).contains(u"snapshot"_s));
        QVERIFY(!names(Scheme::all().result()).contains(u"snapshot"_s));
    }

    void customSchemeLoads() {
        installCustomScheme(u":/schemes/dracula/medium/dark.txt"_s, u"mine"_s, u"custom"_s);
        const auto custom = dataFor(u"mine"_s, u"custom"_s, Mode::Dark);
        const auto bundled = dataFor(u"dracula"_s, u"medium"_s, Mode::Dark);
        QCOMPARE(custom.name, u"mine"_s);
        QCOMPARE(custom.flavour, u"custom"_s);
        QCOMPARE(custom.mode, Mode::Dark);
        QCOMPARE(allColours(custom.colours), allColours(bundled.colours));
    }

    void customSchemeMissingModeThrows() {
        installCustomScheme(u":/schemes/dracula/medium/dark.txt"_s, u"darkonly"_s, u"custom"_s);
        const auto scheme = Scheme::forName(u"darkonly"_s).result();
        QVERIFY(scheme);
        const auto flavour = flavourFor(scheme, u"custom"_s);
        QCOMPARE(flavour.modes(), QList<Mode>{ Mode::Dark });
        const auto future = flavour.data(Mode::Light);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, future.result());
    }

    void customSchemeOverridesBundled() {
        installCustomScheme(u":/schemes/dracula/medium/dark.txt"_s, u"caelestia"_s, u"default"_s);
        const auto data = dataFor(u"caelestia"_s, u"default"_s, Mode::Dark);
        const auto dracula = dataFor(u"dracula"_s, u"medium"_s, Mode::Dark);
        QCOMPARE(allColours(data.colours), allColours(dracula.colours));
        QCOMPARE(dataFor(u"caelestia"_s, u"default"_s, Mode::Light).colours.background, QColor::fromRgba(0xFFF6FAF9));
    }

    void customFlavourMergesIntoBundled() {
        installCustomScheme(u":/schemes/dracula/medium/dark.txt"_s, u"gruvbox"_s, u"extra"_s);
        const auto scheme = Scheme::forName(u"gruvbox"_s).result();
        QVERIFY(scheme);
        QCOMPARE(names(scheme->flavours()), (QStringList{ u"extra"_s, u"hard"_s, u"medium"_s, u"soft"_s }));
        QCOMPARE(flavourFor(scheme, u"extra"_s).modes(), QList<Mode>{ Mode::Dark });
        QCOMPARE(flavourFor(scheme, u"hard"_s).modes(), (QList<Mode>{ Mode::Light, Mode::Dark }));
    }

    void unreadableCustomFallsBackToBundled() {
        const auto dir = caelestia::config::ConfigPaths::customSchemesDir() + u"/nord/medium"_s;
        QVERIFY(QDir().mkpath(dir + u"/dark.txt"_s));
        const auto data = dataFor(u"nord"_s, u"medium"_s, Mode::Dark);
        QCOMPARE(data.colours.background, QColor::fromRgba(0xFF2E3440));
    }

    void customSchemeParsing() {
        const auto dir = caelestia::config::ConfigPaths::customSchemesDir() + u"/handwritten/custom"_s;
        QVERIFY(QDir().mkpath(dir));
        QVERIFY(QFile::copy(u":/schemes/dracula/medium/dark.txt"_s, dir + u"/dark.txt"_s));
        QFile file(dir + u"/dark.txt"_s);
        QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        QVERIFY(file.open(QIODevice::Append | QIODevice::Text));
        file.write("\n// secondary 0000ff\n"
                   "background #112233\n"
                   "onBackground 445566\n"
                   "surface\n"
                   "outline 778899 aabbcc\n"
                   "primary #ABCDEF\n");
        file.close();

        const auto data = dataFor(u"handwritten"_s, u"custom"_s, Mode::Dark);
        const auto dracula = dataFor(u"dracula"_s, u"medium"_s, Mode::Dark);
        QCOMPARE(data.colours.background, QColor::fromRgba(0xFF112233));
        QCOMPARE(data.colours.onBackground, QColor::fromRgba(0xFF445566));
        QCOMPARE(data.colours.primary, QColor::fromRgba(0xFFABCDEF));
        QCOMPARE(data.colours.secondary, dracula.colours.secondary);
        QCOMPARE(data.colours.surface, dracula.colours.surface);
        QCOMPARE(data.colours.outline, dracula.colours.outline);
    }

    void incompleteCustomSchemeIsRejected() {
        const auto dir = caelestia::config::ConfigPaths::customSchemesDir() + u"/partial/custom"_s;
        QVERIFY(QDir().mkpath(dir));
        QFile file(dir + u"/dark.txt"_s);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("background 112233\n");
        file.close();

        const auto scheme = Scheme::forName(u"partial"_s).result();
        QVERIFY(scheme);
        const auto future = flavourFor(scheme, u"custom"_s).data(Mode::Dark);
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, future.result());
    }

    void incompleteCustomFallsBackToBundled() {
        const auto dir = caelestia::config::ConfigPaths::customSchemesDir() + u"/onedark/default"_s;
        QVERIFY(QDir().mkpath(dir));
        QFile file(dir + u"/dark.txt"_s);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("background 112233\n");
        file.close();

        const auto data = dataFor(u"onedark"_s, u"default"_s, Mode::Dark);
        QCOMPARE(data.colours.background, QColor::fromRgba(0xFF121317));
    }

    void namedColoursMatchStruct() {
        const auto colours = generate(k_sourceColour, DynamicVariant::TonalSpot, Mode::Dark);
        const auto named = namedColours(colours);
        const auto all = allColours(colours);
        QCOMPARE(named.size(), static_cast<qsizetype>(all.size()));
        for (qsizetype i = 0; i < named.size(); ++i)
            QVERIFY2(named[i].colour == all[i], named[i].name.toUtf8().constData());
    }

    void modeNames() {
        QCOMPARE(modeName(Mode::Light), u"light"_s);
        QCOMPARE(modeName(Mode::Dark), u"dark"_s);
    }

    void dynamicSchemeFlavours() {
        const auto scheme = Scheme::forColour(k_sourceColour);
        QVERIFY(scheme);
        QCOMPARE(scheme->name(), u"dynamic"_s);

        const auto flavours = scheme->flavours();
        QCOMPARE(flavours.size(), static_cast<qsizetype>(k_variantNames.size()));
        for (qsizetype i = 0; i < flavours.size(); ++i) {
            QCOMPARE(flavours[i].name(), k_variantNames[static_cast<size_t>(i)].second.toString());
            QCOMPARE(flavours[i].modes(), (QList<Mode>{ Mode::Light, Mode::Dark }));
        }
        QVERIFY(!scheme->flavour(u"bogus"_s).has_value());
    }

    void schemeForColour() {
        const auto data = flavourFor(Scheme::forColour(k_sourceColour), u"tonalspot"_s).data(Mode::Dark).result();
        QCOMPARE(data.name, u"dynamic"_s);
        QCOMPARE(data.flavour, u"tonalspot"_s);
        QCOMPARE(data.mode, Mode::Dark);
        QCOMPARE(allColours(data.colours), allColours(generate(k_sourceColour, DynamicVariant::TonalSpot, Mode::Dark)));
    }

    void schemeForColourLightMode() {
        const auto data = flavourFor(Scheme::forColour(k_sourceColour), u"vibrant"_s).data(Mode::Light).result();
        QCOMPARE(data.flavour, u"vibrant"_s);
        QCOMPARE(data.mode, Mode::Light);
        QCOMPARE(allColours(data.colours), allColours(generate(k_sourceColour, DynamicVariant::Vibrant, Mode::Light)));
    }

    void schemeForMediaMatchesColour() {
        const auto scheme = Scheme::forMedia(m_path).result();
        QVERIFY(scheme);
        QCOMPARE(scheme->name(), u"dynamic"_s);
        const auto fromMedia = flavourFor(scheme, u"monochrome"_s).data(Mode::Light).result();
        const auto fromColour =
            flavourFor(Scheme::forColour(k_sourceColour), u"monochrome"_s).data(Mode::Light).result();
        QCOMPARE(fromMedia.flavour, fromColour.flavour);
        QCOMPARE(fromMedia.mode, fromColour.mode);
        QCOMPARE(allColours(fromMedia.colours), allColours(fromColour.colours));
    }

    void schemeForMissingMediaIsNull() { QVERIFY(!Scheme::forMedia(m_root.filePath(u"missing.png"_s)).result()); }

    void dynamicSchemeUsesCache() {
        constexpr auto k_variant = DynamicVariant::FruitSalad;
        constexpr auto k_mode = Mode::Dark;
        const auto seeded = generate(QColor::fromRgba(0xFFFF0000), DynamicVariant::Vibrant, Mode::Light);
        const auto key = SchemeCache::keyFor(k_sourceColour, k_variant, k_mode);
        SchemeCache::instance().store(key, toBytes(seeded));
        QCOMPARE(SchemeCache::instance().lookup(key).result(), toBytes(seeded));

        const auto data = flavourFor(Scheme::forColour(k_sourceColour), u"fruitsalad"_s).data(k_mode).result();
        QCOMPARE(allColours(data.colours), allColours(seeded));
        QVERIFY(allColours(data.colours) != allColours(generate(k_sourceColour, k_variant, k_mode)));
    }

    void defaultFlavourAndMode() {
        const auto scheme = Scheme::forName(u"catppuccin"_s).result();
        QVERIFY(scheme);
        QCOMPARE(scheme->defaultFlavour().name(), scheme->flavours().constFirst().name());
        QCOMPARE(flavourFor(scheme, u"mocha"_s).defaultMode(), Mode::Dark);
        QCOMPARE(flavourFor(scheme, u"latte"_s).defaultMode(), Mode::Light);
        QCOMPARE(Scheme::forColour(k_sourceColour)->defaultFlavour().defaultMode(), Mode::Dark);
    }

    void stateRoundTrips() {
        const auto data = dataFor(u"catppuccin"_s, u"mocha"_s, Mode::Dark);
        QVERIFY(saveState(data));

        const auto json = readJson(statePath());
        QCOMPARE(json[u"name"_s].toString(), u"catppuccin"_s);
        QCOMPARE(json[u"flavour"_s].toString(), u"mocha"_s);
        QCOMPARE(json[u"mode"_s].toString(), u"dark"_s);
        QCOMPARE(json[u"colours"_s][u"primary"_s].toString(), u"cba6f7"_s);

        const auto loaded = loadState();
        if (!loaded)
            QFAIL("loadState returned nullopt");
        QCOMPARE(loaded->name, data.name);
        QCOMPARE(loaded->flavour, data.flavour);
        QCOMPARE(loaded->mode, data.mode);
        QCOMPARE(allColours(loaded->colours), allColours(data.colours));
    }

    void invalidStateIsRejected() {
        QFile::remove(statePath());
        QVERIFY(!loadState().has_value());

        writeFile(statePath(), "garbage");
        QVERIFY(!loadState().has_value());

        auto json = toJson(dataFor(u"catppuccin"_s, u"mocha"_s, Mode::Dark));
        auto colours = json[u"colours"_s].toObject();
        colours.remove(u"primary"_s);
        json[u"colours"_s] = colours;
        writeFile(statePath(), QJsonDocument(json).toJson());
        QVERIFY(!loadState().has_value());
        const auto ref = loadStateRef();
        if (!ref)
            QFAIL("loadStateRef returned nullopt");
        QCOMPARE(ref->name, u"catppuccin"_s);
        QCOMPARE(ref->flavour, u"mocha"_s);
        QCOMPARE(ref->mode, Mode::Dark);

        json[u"mode"_s] = u"dusk"_s;
        writeFile(statePath(), QJsonDocument(json).toJson());
        QVERIFY(!loadState().has_value());
        QVERIFY(!loadStateRef().has_value());
    }

    void dynamicSchemeIgnoresWrongSizedCache() {
        constexpr auto k_variant = DynamicVariant::Rainbow;
        constexpr auto k_mode = Mode::Light;
        const auto key = SchemeCache::keyFor(k_sourceColour, k_variant, k_mode);
        const QByteArray bad(7, '\x7f');
        SchemeCache::instance().store(key, bad);
        QCOMPARE(SchemeCache::instance().lookup(key).result(), bad);

        const auto data = flavourFor(Scheme::forColour(k_sourceColour), u"rainbow"_s).data(k_mode).result();
        QCOMPARE(allColours(data.colours), allColours(generate(k_sourceColour, k_variant, k_mode)));
    }
};

QTEST_GUILESS_MAIN(TestScheme)

#include "scheme_test.moc"
