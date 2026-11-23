#include "processtransaction.hpp"

#include <qloggingcategory.h>

namespace caelestia::packages {

namespace {

Q_LOGGING_CATEGORY(lcTransaction, "caelestia.packages.transaction", QtInfoMsg)

} // namespace

ProcessTransaction::ProcessTransaction(
    const QString& program, const QStringList& arguments, const QProcessEnvironment& environment)
    : m_process(new QProcess()) {
    auto* const process = m_process.get();
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());

    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError) {
        if (process->state() == QProcess::NotRunning)
            process->deleteLater();
    });

    connect(process, &QProcess::readyReadStandardOutput, this, &ProcessTransaction::readOutput);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        if (process->state() == QProcess::NotRunning) {
            readOutput();
            append(process->errorString());
            flush();
            finish(false);
        }
    });
    connect(process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        readOutput();
        flush();
        finish(status == QProcess::NormalExit && exitCode == 0);
    });

    qCDebug(lcTransaction) << "Running" << program << arguments;
    process->start(program, arguments);
}

ProcessTransaction::~ProcessTransaction() {
    if (m_process)
        m_process->disconnect(this);
}

void ProcessTransaction::cancel() {
    if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->terminate();
}

void ProcessTransaction::readOutput() {
    if (!m_process)
        return;

    m_pending += m_process->readAllStandardOutput();
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
