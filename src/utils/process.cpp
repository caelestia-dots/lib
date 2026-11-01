#include "caelestia/utils/process.hpp"

#include <qprocess.h>

namespace caelestia::utils {

bool execDetached(const QString& program, const QStringList& arguments, qint64* pid) {
    QProcess proc;
    proc.setProgram(program);
    proc.setArguments(arguments);
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.setStandardOutputFile(QProcess::nullDevice());
    proc.setStandardErrorFile(QProcess::nullDevice());
    proc.setUnixProcessParameters(QProcess::UnixProcessFlag::CloseFileDescriptors);
    return proc.startDetached(pid);
}

} // namespace caelestia::utils
