#pragma once

#include <qlist.h>
#include <qprocess.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtclasshelpermacros.h>
#include <qtypes.h>

#include <optional>

#include "caelestia/packages/packages.hpp"

namespace caelestia::packages {

enum class AurHelper : quint8 {
    Paru,
    Yay,
};

struct Command {
    QString program;
    QStringList arguments;

    bool operator==(const Command&) const = default;
};

struct AurQueryResult {
    QList<Update> updates;
    QString error;

    [[nodiscard]] bool ok() const { return error.isEmpty(); }
};

class AurUpdateQuery {
public:
    Q_DISABLE_COPY_MOVE(AurUpdateQuery)
    explicit AurUpdateQuery(AurHelper helper);
    ~AurUpdateQuery() = default;

    [[nodiscard]] AurQueryResult finish();

private:
    QProcess m_process;
};

[[nodiscard]] std::optional<AurHelper> findAurHelper();
[[nodiscard]] bool isPackageName(const QString& name);
[[nodiscard]] QProcessEnvironment transactionEnvironment();
[[nodiscard]] Command command(std::optional<AurHelper> helper, const QStringList& operation);
[[nodiscard]] QList<Update> parseAurUpdates(const QString& output);

[[nodiscard]] QList<Update> cachedAurUpdates();
void cacheAurUpdates(const QList<Update>& updates);

} // namespace caelestia::packages
