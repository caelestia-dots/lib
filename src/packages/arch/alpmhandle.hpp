#pragma once

#include <qlist.h>
#include <qstring.h>
#include <qtclasshelpermacros.h>

#include <alpm.h>

#include <memory>
#include <optional>

#include "caelestia/packages/packages.hpp"
#include "pacmanconf.hpp"

namespace caelestia::packages {

class AlpmHandle {
public:
    Q_DISABLE_COPY_MOVE(AlpmHandle)
    ~AlpmHandle();

    [[nodiscard]] static std::unique_ptr<AlpmHandle> open(const PacmanConf& conf);

    [[nodiscard]] std::optional<QString> refresh(); // Error message on failure
    [[nodiscard]] QList<Package> installed() const;
    [[nodiscard]] QList<Update> updates() const;

private:
    explicit AlpmHandle(alpm_handle_t* handle);

    [[nodiscard]] QString error() const;
    [[nodiscard]] alpm_db_t* syncDbFor(alpm_pkg_t* local) const;

    alpm_handle_t* const m_handle;
};

} // namespace caelestia::packages
