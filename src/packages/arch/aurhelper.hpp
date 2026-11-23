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

class AurUpdateQuery {
public:
    Q_DISABLE_COPY_MOVE(AurUpdateQuery)
    explicit AurUpdateQuery(AurHelper helper);
    ~AurUpdateQuery() = default;

    [[nodiscard]] QList<Update> finish();

private:
    QProcess m_process;
};

[[nodiscard]] std::optional<AurHelper> findAurHelper();
[[nodiscard]] QProcessEnvironment transactionEnvironment();
[[nodiscard]] Command command(std::optional<AurHelper> helper, const QStringList& operation);
[[nodiscard]] QList<Update> parseAurUpdates(const QString& output);

} // namespace caelestia::packages
