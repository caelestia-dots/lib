#pragma once

#include <qfuture.h>
#include <qlist.h>
#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtypes.h>

#include <memory>

namespace caelestia::packages {

enum class Capability : quint8 {
    None = 0x0,
    ListInstalled = 0x1,
    ListUpdates = 0x2,
    Upgrade = 0x4,
    Install = 0x8,
    Remove = 0x10,
    ManageForeign = 0x20, // Packages outside the sync repos
};
Q_DECLARE_FLAGS(Capabilities, Capability)

struct Package {
    QString name;
    QString version;
    QString source; // Sync db name, "aur", or empty when foreign

    bool operator==(const Package&) const = default;
};

struct Update {
    Package package;
    QString newVersion;

    bool operator==(const Update&) const = default;
};

struct UpdateCheck {
    QList<Update> updates;
    QStringList warnings; // Non-fatal problems, e.g. a failed refresh; updates may be stale

    bool operator==(const UpdateCheck&) const = default;
};

class Transaction : public QObject {
    Q_OBJECT

public:
    Transaction() = default;

    [[nodiscard]] static std::unique_ptr<Transaction> failed(const QString& reason);

    [[nodiscard]] bool running() const;
    [[nodiscard]] bool succeeded() const;
    [[nodiscard]] const QStringList& output() const;

    virtual void cancel() {}

signals:
    void outputLine(const QString& line);
    void finished(bool ok);

protected:
    void append(const QString& line);
    void finish(bool ok);

private:
    QStringList m_output;
    bool m_running = true;
    bool m_succeeded = false;
};

class Backend : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual Capabilities capabilities() const = 0;
    // Futures fail with std::runtime_error when the backend cannot be used at all
    [[nodiscard]] virtual QFuture<QList<Package>> installed() const = 0;
    [[nodiscard]] virtual QFuture<UpdateCheck> updates(bool refresh) const = 0;

    [[nodiscard]] virtual std::unique_ptr<Transaction> upgrade() const = 0;
    [[nodiscard]] virtual std::unique_ptr<Transaction> install(const QStringList& names) const = 0;
    [[nodiscard]] virtual std::unique_ptr<Transaction> remove(const QStringList& names) const = 0;
};

[[nodiscard]] QList<Backend*> backends(QObject* parent = nullptr);

} // namespace caelestia::packages

Q_DECLARE_OPERATORS_FOR_FLAGS(caelestia::packages::Capabilities)
