#pragma once

#include <qstring.h>

#include <optional>

#include "caelestia/packages/packages.hpp"
#include "aurhelper.hpp"

namespace caelestia::packages {

class ArchBackend final : public Backend {
    Q_OBJECT

public:
    [[nodiscard]] static ArchBackend* create(QObject* parent = nullptr, const QString& configPath = {});

    [[nodiscard]] QString name() const override;
    [[nodiscard]] Capabilities capabilities() const override;
    [[nodiscard]] QFuture<QList<Package>> installed() const override;
    [[nodiscard]] QFuture<QList<Update>> updates(bool refresh) const override;

    [[nodiscard]] Transaction* upgrade(QObject* parent = nullptr) const override;
    [[nodiscard]] Transaction* install(const QStringList& names, QObject* parent = nullptr) const override;
    [[nodiscard]] Transaction* remove(const QStringList& names, QObject* parent = nullptr) const override;

private:
    ArchBackend(QString configPath, std::optional<AurHelper> helper, QObject* parent);

    const QString m_configPath;
    const std::optional<AurHelper> m_helper;
};

} // namespace caelestia::packages
