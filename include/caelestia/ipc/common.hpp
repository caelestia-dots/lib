#pragma once

#include <qobjectdefs.h>
#include <qrect.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtypes.h>

#include <variant>

namespace caelestia::ipc {

enum class Error : quint8 {
    NotRunning,
    Busy,
    Cancelled,
    Failed,
};

namespace FileFilter {

Q_NAMESPACE

enum Enum : quint8 {
    NoFilter = 0x0,
    Images = 0x1,
    Videos = 0x2,
    Files = 0x4,
    Dirs = 0x8,
};
Q_DECLARE_FLAGS(Flags, Enum)
Q_FLAG_NS(Flags)

} // namespace FileFilter

namespace PickFileMode {

Q_NAMESPACE

enum Enum : quint8 {
    Select,
    Open,
    Save,
};
Q_ENUM_NS(Enum)

} // namespace PickFileMode

struct PickFileOptions {
    QString title;
    QStringList nameFilters;
    FileFilter::Flags filter;
    QString initialDir;
    QString initialName;
    PickFileMode::Enum mode = PickFileMode::Select;
};

using Response = std::variant<Error, QRect, QString>;

} // namespace caelestia::ipc

Q_DECLARE_OPERATORS_FOR_FLAGS(caelestia::ipc::FileFilter::Flags)
