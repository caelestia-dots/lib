#pragma once

#include <qbytearray.h>
#include <qpointer.h>
#include <qprocess.h>
#include <qstring.h>
#include <qstringlist.h>

#include "caelestia/packages/packages.hpp"

namespace caelestia::packages {

class ProcessTransaction final : public Transaction {
    Q_OBJECT

public:
    ProcessTransaction(const QString& program, const QStringList& arguments, const QProcessEnvironment& environment);
    ~ProcessTransaction() override;

    void cancel() override;

private:
    void readOutput();
    void flush();

    const QPointer<QProcess> m_process;
    QByteArray m_pending;
};

} // namespace caelestia::packages
