#include "processtransaction.hpp"

#include <qloggingcategory.h>

namespace caelestia::packages {

namespace {

Q_LOGGING_CATEGORY(lcTransaction, "caelestia.packages.transaction", QtInfoMsg)

} // namespace

ProcessTransaction::ProcessTransaction(
    const QString& program, const QStringList& arguments, const QProcessEnvironment& environment, QObject* parent)
    : Transaction(parent) {
    m_process.setProcessEnvironment(environment);
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_process.setStandardInputFile(QProcess::nullDevice());

    connect(&m_process, &QProcess::readyReadStandardOutput, this, &ProcessTransaction::readOutput);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process.state() == QProcess::NotRunning) {
            readOutput();
            append(m_process.errorString());
            flush();
            finish(false);
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        readOutput();
        flush();
        finish(status == QProcess::NormalExit && exitCode == 0);
    });

    qCDebug(lcTransaction) << "Running" << program << arguments;
    m_process.start(program, arguments);
}

void ProcessTransaction::cancel() {
    if (m_process.state() != QProcess::NotRunning)
        m_process.terminate();
}

void ProcessTransaction::readOutput() {
    m_pending += m_process.readAllStandardOutput();
    qsizetype end = 0;
    while ((end = m_pending.indexOf('\n')) != -1) {
        auto line = m_pending.first(end);
        if (line.endsWith('\r'))
            line.chop(1);
        append(QString::fromUtf8(line));
        m_pending.remove(0, end + 1);
    }
}

void ProcessTransaction::flush() {
    if (m_pending.isEmpty())
        return;
    append(QString::fromUtf8(m_pending));
    m_pending.clear();
}

} // namespace caelestia::packages
