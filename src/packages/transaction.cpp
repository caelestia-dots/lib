#include <qloggingcategory.h>
#include <qmetaobject.h>

#include "caelestia/packages/packages.hpp"

namespace caelestia::packages {

namespace {

Q_LOGGING_CATEGORY(lcTransaction, "caelestia.packages.transaction", QtInfoMsg)

} // namespace

Transaction::Transaction(QObject* parent)
    : QObject(parent) {}

Transaction* Transaction::run(
    const QString& program, const QStringList& arguments, const QProcessEnvironment& environment, QObject* parent) {
    auto* transaction = new Transaction(parent);
    auto* process = new QProcess(transaction);
    transaction->m_process = process;

    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setStandardInputFile(QProcess::nullDevice());

    connect(process, &QProcess::readyReadStandardOutput, transaction, &Transaction::readOutput);
    connect(process, &QProcess::errorOccurred, transaction, [transaction, process](QProcess::ProcessError) {
        if (process->state() == QProcess::NotRunning) {
            transaction->append(process->errorString());
            transaction->finish(false);
        }
    });
    connect(process, &QProcess::finished, transaction, [transaction](int exitCode, QProcess::ExitStatus status) {
        transaction->readOutput();
        transaction->finish(status == QProcess::NormalExit && exitCode == 0);
    });

    qCDebug(lcTransaction) << "Running" << program << arguments;
    process->start(program, arguments);
    return transaction;
}

Transaction* Transaction::failed(const QString& reason, QObject* parent) {
    auto* transaction = new Transaction(parent);
    transaction->append(reason);
    QMetaObject::invokeMethod(
        transaction,
        [transaction] {
            transaction->finish(false);
        },
        Qt::QueuedConnection);
    return transaction;
}

bool Transaction::running() const {
    return m_running;
}

bool Transaction::succeeded() const {
    return m_succeeded;
}

const QStringList& Transaction::output() const {
    return m_output;
}

void Transaction::append(const QString& line) {
    m_output.append(line);
    emit outputLine(line);
}

void Transaction::finish(bool ok) {
    if (!m_running)
        return;

    if (!m_pending.isEmpty()) {
        append(QString::fromUtf8(m_pending));
        m_pending.clear();
    }

    m_running = false;
    m_succeeded = ok;
    emit finished(ok);
}

void Transaction::readOutput() {
    m_pending += m_process->readAllStandardOutput();
    qsizetype newline = 0;
    while ((newline = m_pending.indexOf('\n')) != -1) {
        append(QString::fromUtf8(m_pending.first(newline)));
        m_pending = m_pending.sliced(newline + 1);
    }
}

} // namespace caelestia::packages
