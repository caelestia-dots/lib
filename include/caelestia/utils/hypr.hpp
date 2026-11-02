#pragma once

#include <qbytearray.h>
#include <qjsondocument.h>
#include <qstring.h>
#include <qstringlist.h>

#include <optional>

namespace caelestia::utils::hypr {

constexpr int k_timeoutMs = 5000;

[[nodiscard]] QString socketDir();

[[nodiscard]] std::optional<QByteArray> request(const QString& message);
[[nodiscard]] QJsonDocument requestJson(const QString& message);
[[nodiscard]] bool eval(const QString& expression);
[[nodiscard]] bool dispatch(const QString& dispatcher, const QStringList& args = {});

} // namespace caelestia::utils::hypr
