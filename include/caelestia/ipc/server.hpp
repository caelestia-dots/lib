#pragma once

#include <qlocalserver.h>
#include <qlockfile.h>
#include <qobject.h>
#include <qpointer.h>
#include <qrect.h>

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

class Server : public QObject {
    Q_OBJECT

public:
    explicit Server(QObject* parent = nullptr);

    [[nodiscard]] bool isListening() const;

signals:
    void pickRegionRequested(caelestia::ipc::PickRegionRequest* request);
    void screenshotRequested(caelestia::ipc::ScreenshotRequest* request);
    void released();

private:
    QLockFile m_lock;
    QLocalServer m_server;
    QPointer<PickRegionRequest> m_pendingPick;
    QPointer<ScreenshotRequest> m_pendingScreenshot;

    static void releaseExisting();
    void accept();
    void dispatch(QLocalSocket* socket);
    void release(QLocalSocket* socket);
};

} // namespace caelestia::ipc
