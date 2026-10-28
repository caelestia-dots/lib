#include "caelestia/utils/components.hpp"

#include <qdir.h>
#include <qfileinfo.h>
#include <qsettings.h>
#include <qstandardpaths.h>

namespace caelestia::utils {

using Qt::StringLiterals::operator""_s;

QHash<QString, ComponentInfo> installedComponents() {
    QHash<QString, ComponentInfo> components;

    const auto dirs = QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation, u"caelestia/components"_s, QStandardPaths::LocateDirectory);

    for (const auto& dir : dirs) {
        const auto files = QDir(dir).entryInfoList({ u"*.ini"_s }, QDir::Files);
        for (const auto& file : files) {
            const auto name = file.completeBaseName();
            if (components.contains(name))
                continue;

            const QSettings settings(file.filePath(), QSettings::IniFormat);
            const ComponentInfo info{
                .version = settings.value(u"Component/Version"_s).toString(),
                .distributor = settings.value(u"Component/Distributor"_s).toString(),
                .gitRevision = settings.value(u"Component/GitRevision"_s).toString(),
            };
            components.insert(name, info);
        }
    }

    return components;
}

} // namespace caelestia::utils
