#pragma once

#include <qbytearray.h>
#include <qprocess.h>
#include <qstring.h>
#include <qstringlist.h>

#include "caelestia/packages/packages.hpp"

namespace caelestia::packages {

class ProcessTransaction final : public Transaction {
    Q_OBJECT

public:
    ProcessTransaction(const QString& program, const QStringList& arguments, const QProcessEnvironment& environment,
        QObject* parent = nullptr);

    void cancel() override;

private:
    void readOutput();
    void flush();

    QProcess m_process;
    QByteArray m_pending;
};

} // namespace caelestia::packages
