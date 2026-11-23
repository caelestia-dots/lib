#include <qmetaobject.h>

#include "caelestia/packages/packages.hpp"

namespace caelestia::packages {

std::unique_ptr<Transaction> Transaction::failed(const QString& reason) {
    auto transaction = std::make_unique<Transaction>();
    transaction->append(reason);
    QMetaObject::invokeMethod(
        transaction.get(),
        [raw = transaction.get()] {
            raw->finish(false);
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

    m_running = false;
    m_succeeded = ok;
    emit finished(ok);
}

} // namespace caelestia::packages
