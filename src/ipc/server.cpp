#include "caelestia/ipc/server.hpp"

#include <qdir.h>
#include <qlocalsocket.h>
#include <qloggingcategory.h>

#include <utility>
#include <variant>

#include "caelestia/config/paths.hpp"
#include "protocol.hpp"

namespace caelestia::ipc {

namespace {

Q_LOGGING_CATEGORY(lcIpc, "caelestia.ipc", QtInfoMsg)

constexpr int k_releaseTimeoutMs = 1000;
constexpr int k_lockTimeoutMs = 5000;

void answer(QLocalSocket* socket, const Response& response) {
    socket->write(protocol::encode(response));
    socket->disconnectFromServer();
}

} // namespace

Request::Request(QLocalSocket* socket)
    : QObject(socket)
    , m_socket(socket) {
    connect(socket, &QLocalSocket::disconnected, this, [this] {
        if (!std::exchange(m_done, true))
            emit cancelled();
    });
}

void Request::reject() {
    reply(Error::Cancelled);
}

void Request::reply(const Response& response) {
    if (std::exchange(m_done, true))
        return;

    answer(m_socket, response);
}

PickRegionRequest::PickRegionRequest(bool freeze, QLocalSocket* socket)
    : Request(socket)
    , m_freeze(freeze) {}

bool PickRegionRequest::freeze() const {
    return m_freeze;
}

void PickRegionRequest::respond(const QRect& region) {
    reply(region);
}

Server::Server(QObject* parent)
    : QObject(parent)
    , m_lock(protocol::lockPath()) {
    QDir().mkpath(config::ConfigPaths::runtimeDir());
    releaseExisting();

    if (!m_lock.tryLock(k_lockTimeoutMs)) {
        qCWarning(lcIpc) << "Another shell holds" << protocol::lockPath();
        return;
    }

    QLocalServer::removeServer(protocol::socketPath());
    if (!m_server.listen(protocol::socketPath())) {
        qCWarning(lcIpc) << "Failed to listen on" << protocol::socketPath() << m_server.errorString();
        return;
    }

    connect(&m_server, &QLocalServer::newConnection, this, &Server::accept);
}

bool Server::isListening() const {
    return m_server.isListening();
}

void Server::releaseExisting() {
    QLocalSocket socket;
    socket.connectToServer(protocol::socketPath());
    if (!socket.waitForConnected(k_releaseTimeoutMs))
        return;

    socket.write(protocol::encode(protocol::Release{}));
    if (!socket.waitForDisconnected(k_releaseTimeoutMs))
        qCWarning(lcIpc) << "Existing shell did not release the socket";
}

void Server::accept() {
    while (auto* const socket = m_server.nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            if (socket->canReadLine())
                dispatch(socket);
        });
    }
}

void Server::dispatch(QLocalSocket* socket) {
    socket->disconnect(this);
    const auto request = protocol::decodeRequest(socket->readLine());
    if (!request) {
        answer(socket, Error::Failed);
        return;
    }

    const protocol::Overloaded handlers{
        [this, socket](const protocol::Release&) {
            release(socket);
        },
        [this, socket](const protocol::PickRegion& pick) {
            if (m_pendingPick) {
                answer(socket, Error::Busy);
                return;
            }

            m_pendingPick = new PickRegionRequest(pick.freeze, socket);
            emit pickRegionRequested(m_pendingPick);
        },
    };
    std::visit(handlers, *request);
}

void Server::release(QLocalSocket* socket) {
    m_server.close();
    if (m_pendingPick)
        m_pendingPick->reject();
    m_lock.unlock();

    socket->disconnectFromServer();
    emit released();
}

} // namespace caelestia::ipc
