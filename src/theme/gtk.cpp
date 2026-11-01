#include <qdir.h>
#include <qfileinfo.h>
#include <qstandardpaths.h>

#include <algorithm>
#include <array>
#include <cstdlib>

#include "caelestia/theme/theme.hpp"
#include "caelestia/utils/process.hpp"
#include "util.hpp"

namespace caelestia::theme {

using Qt::StringLiterals::operator""_s;

namespace {

QString blueDominant(int r, int g, int b) {
    const int rRatio = r * 100 / b;
    const int gRatio = g * 100 / b;
    if (rRatio > 70 && gRatio > 70) {
        if (std::abs(r - g) < 15)
            return u"blue"_s;
        return r > g ? u"violet"_s : u"cyan"_s;
    }
    if (rRatio > 60 && r > g)
        return u"violet"_s;
    if (gRatio > 60 && g > r)
        return u"cyan"_s;
    return u"blue"_s;
}

QString redDominant(int r, int g, int b, int brightness, bool pale) {
    if (g > b + 30) {
        const int rgRatio = g * 100 / r;
        if (pale)
            return rgRatio > 70 && brightness < 220 ? u"palebrown"_s : u"paleorange"_s;
        return rgRatio > 70 && brightness < 180 ? u"brown"_s : u"orange"_s;
    }
    if (b > g + 20)
        return u"pink"_s;
    return pale ? u"pink"_s : u"red"_s;
}

QString hueColour(int r, int g, int b, int brightness, bool pale) {
    if (b > r && b > g)
        return blueDominant(r, g, b);
    if (r > g && r > b)
        return redDominant(r, g, b, brightness, pale);
    if (g > r && g > b)
        return r > b + 30 ? u"yellow"_s : u"green"_s;
    return u"grey"_s;
}

QString papirusColour(QColor colour) {
    const int r = colour.red();
    const int g = colour.green();
    const int b = colour.blue();
    const int brightness = std::max({ r, g, b });
    const int minVal = std::min({ r, g, b });
    const int saturation = brightness == 0 ? 0 : (brightness - minVal) * 100 / brightness;

    if (saturation < 20) {
        if (brightness < 85)
            return u"black"_s;
        return brightness < 170 ? u"grey"_s : u"white"_s;
    }
    return hueColour(r, g, b, brightness, saturation < 60 && brightness > 180);
}

void syncPapirus(QColor primary) {
    if (QStandardPaths::findExecutable(u"papirus-folders"_s).isEmpty())
        return;

    const std::array paths = {
        u"/usr/share/icons/Papirus"_s,
        u"/usr/share/icons/Papirus-Dark"_s,
        QDir::homePath() + u"/.local/share/icons/Papirus"_s,
        QDir::homePath() + u"/.icons/Papirus"_s,
    };
    if (std::ranges::none_of(paths, [](const QString& p) {
            return QFileInfo::exists(p);
        }))
        return;

    utils::execDetached(u"sudo"_s, { u"-n"_s, u"papirus-folders"_s, u"-C"_s, papirusColour(primary), u"-u"_s });
}

void dconfWrite(const QString& key, const QString& value) {
    run(u"dconf"_s, { u"write"_s, key, u"'%1'"_s.arg(value) });
}

} // namespace

void Theme::applyGtk() const {
    const auto gtk = renderTemplate(u"gtk.css"_s);
    const auto thunar = renderTemplate(u"thunar.css"_s);
    for (const auto& version : { u"gtk-3.0"_s, u"gtk-4.0"_s }) {
        writeFile(u"%1/%2/gtk.css"_s.arg(configDir(), version), gtk);
        writeFile(u"%1/%2/thunar.css"_s.arg(configDir(), version), thunar);
    }

    dconfWrite(u"/org/gnome/desktop/interface/gtk-theme"_s, u"adw-gtk3-dark"_s);
    dconfWrite(u"/org/gnome/desktop/interface/color-scheme"_s, u"prefer-"_s + scheme::modeName(m_scheme.mode));
    dconfWrite(u"/org/gnome/desktop/interface/icon-theme"_s, m_iconTheme);

    syncPapirus(m_scheme.colours.primary);
}

} // namespace caelestia::theme
