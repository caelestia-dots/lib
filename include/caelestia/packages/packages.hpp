#pragma once

#include <qfuture.h>
#include <qlist.h>
#include <qobject.h>
#include <qprocess.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtypes.h>

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

class Transaction : public QObject {
    Q_OBJECT

public:
    [[nodiscard]] static Transaction* run(const QString& program, const QStringList& arguments,
        const QProcessEnvironment& environment, QObject* parent = nullptr);
    [[nodiscard]] static Transaction* failed(const QString& reason, QObject* parent = nullptr);

    [[nodiscard]] bool running() const;
    [[nodiscard]] bool succeeded() const;
    [[nodiscard]] const QStringList& output() const;

signals:
    void outputLine(const QString& line);
    void finished(bool ok);

private:
    explicit Transaction(QObject* parent);

    void append(const QString& line);
    void finish(bool ok);
    void readOutput();

    QProcess* m_process = nullptr;
    QByteArray m_pending;
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
    [[nodiscard]] virtual QFuture<QList<Package>> installed() const = 0;
    [[nodiscard]] virtual QFuture<QList<Update>> updates(bool refresh) const = 0;

    [[nodiscard]] virtual Transaction* upgrade(QObject* parent = nullptr) const = 0;
    [[nodiscard]] virtual Transaction* install(const QStringList& names, QObject* parent = nullptr) const = 0;
    [[nodiscard]] virtual Transaction* remove(const QStringList& names, QObject* parent = nullptr) const = 0;
};

[[nodiscard]] QList<Backend*> backends(QObject* parent = nullptr);

} // namespace caelestia::packages

Q_DECLARE_OPERATORS_FOR_FLAGS(caelestia::packages::Capabilities)
