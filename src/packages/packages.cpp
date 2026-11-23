#include "caelestia/packages/packages.hpp"

#ifdef CAELESTIA_WITH_ALPM
#include "arch/archbackend.hpp"
#endif

namespace caelestia::packages {

QList<Backend*> backends(QObject* parent) {
    QList<Backend*> list;

#ifdef CAELESTIA_WITH_ALPM
    if (auto* const arch = ArchBackend::create(parent))
        list << arch;
#endif

    return list;
}

} // namespace caelestia::packages
