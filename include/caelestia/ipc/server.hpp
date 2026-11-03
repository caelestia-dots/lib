#pragma once

#include <qlist.h>
#include <qlocalserver.h>
#include <qlockfile.h>
#include <qobject.h>
#include <qpointer.h>
#include <qrect.h>
#include <qstring.h>

#include "caelestia/ipc/common.hpp"

class QLocalSocket;

namespace caelestia::ipc {

class Request : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE void reject();

signals:
    void cancelled();

protected:
    explicit Request(QLocalSocket* socket);

    void reply(const Response& response);

private:
    QLocalSocket* m_socket;
    bool m_done = false;
};

class PickRegionRequest : public Request {
    Q_OBJECT

public:
    Q_INVOKABLE void respond(const QRect& region);

private:
    friend class Server;

    explicit PickRegionRequest(QLocalSocket* socket);
};

class ScreenshotRequest : public Request {
    Q_OBJECT

    Q_PROPERTY(bool region READ region CONSTANT)
    Q_PROPERTY(bool annotate READ annotate CONSTANT)

public:
    [[nodiscard]] bool region() const;
    [[nodiscard]] bool annotate() const;

    Q_INVOKABLE void respond(const QString& file);

private:
    friend class Server;

    ScreenshotRequest(bool region, bool annotate, QLocalSocket* socket);

    bool m_region;
    bool m_annotate;
};

class AnnotateRequest : public Request {
    Q_OBJECT

    Q_PROPERTY(QString file READ file CONSTANT)

public:
    [[nodiscard]] QString file() const;

    Q_INVOKABLE void respond(const QString& file);

private:
    friend class Server;

    AnnotateRequest(QString file, QLocalSocket* socket);

    QString m_file;
};

class PickFileRequest : public Request {
    Q_OBJECT

    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QStringList nameFilters READ nameFilters CONSTANT)
    Q_PROPERTY(caelestia::ipc::FileFilter::Flags filter READ filter CONSTANT)
    Q_PROPERTY(QString initialDir READ initialDir CONSTANT)
    Q_PROPERTY(QString initialName READ initialName CONSTANT)
    Q_PROPERTY(caelestia::ipc::PickFileMode::Enum mode READ mode CONSTANT)

public:
    [[nodiscard]] QString title() const;
    [[nodiscard]] QStringList nameFilters() const;
    [[nodiscard]] FileFilter::Flags filter() const;
    [[nodiscard]] QString initialDir() const;
    [[nodiscard]] QString initialName() const;
    [[nodiscard]] PickFileMode::Enum mode() const;

    Q_INVOKABLE void respond(const QString& file);

private:
    friend class Server;

    PickFileRequest(PickFileOptions options, QLocalSocket* socket);

    PickFileOptions m_options;
};

class Server : public QObject {
    Q_OBJECT

public:
    explicit Server(QObject* parent = nullptr);

    [[nodiscard]] bool isListening() const;

signals:
    void pickRegionRequested(caelestia::ipc::PickRegionRequest* request);
    void screenshotRequested(caelestia::ipc::ScreenshotRequest* request);
    void annotateRequested(caelestia::ipc::AnnotateRequest* request);
    void pickFileRequested(caelestia::ipc::PickFileRequest* request);
    void released();

private:
    QLockFile m_lock;
    QLocalServer m_server;
    QPointer<PickRegionRequest> m_pendingPick;
    QPointer<ScreenshotRequest> m_pendingScreenshot;
    QPointer<AnnotateRequest> m_pendingAnnotate;
    QList<QPointer<PickFileRequest>> m_pendingPickFiles;

    static void releaseExisting();
    void accept();
    void dispatch(QLocalSocket* socket);
    void release(QLocalSocket* socket);
};

} // namespace caelestia::ipc
