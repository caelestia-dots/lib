#pragma once

#include <qstring.h>
#include <qstringlist.h>

namespace caelestia::utils {

bool execDetached(const QString& program, const QStringList& arguments, qint64* pid = nullptr);

} // namespace caelestia::utils
