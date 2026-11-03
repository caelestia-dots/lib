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

PickRegionRequest::PickRegionRequest(QLocalSocket* socket)
    : Request(socket) {}

void PickRegionRequest::respond(const QRect& region) {
    reply(region);
}

ScreenshotRequest::ScreenshotRequest(bool region, bool annotate, QLocalSocket* socket)
    : Request(socket)
    , m_region(region)
    , m_annotate(annotate) {}

[[nodiscard]] bool ScreenshotRequest::region() const {
    return m_region;
}

[[nodiscard]] bool ScreenshotRequest::annotate() const {
    return m_annotate;
}

void ScreenshotRequest::respond(const QString& file) {
    reply(file);
}

AnnotateRequest::AnnotateRequest(QString file, QLocalSocket* socket)
    : Request(socket)
    , m_file(std::move(file)) {}

[[nodiscard]] QString AnnotateRequest::file() const {
    return m_file;
}

void AnnotateRequest::respond(const QString& file) {
    reply(file);
}

PickFileRequest::PickFileRequest(PickFileOptions options, QLocalSocket* socket)
    : Request(socket)
    , m_options(std::move(options)) {}

[[nodiscard]] QString PickFileRequest::title() const {
    return m_options.title;
}

[[nodiscard]] QStringList PickFileRequest::nameFilters() const {
    return m_options.nameFilters;
}

[[nodiscard]] FileFilter::Flags PickFileRequest::filter() const {
    return m_options.filter;
}

[[nodiscard]] QString PickFileRequest::initialDir() const {
    return m_options.initialDir;
}

[[nodiscard]] QString PickFileRequest::initialName() const {
    return m_options.initialName;
}

[[nodiscard]] PickFileMode::Enum PickFileRequest::mode() const {
    return m_options.mode;
}

void PickFileRequest::respond(const QString& file) {
    reply(file);
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
        [this, socket](const protocol::PickRegion&) {
            // Pick/screenshot are exclusive
            if (m_pendingPick || m_pendingScreenshot) {
                answer(socket, Error::Busy);
                return;
            }

            m_pendingPick = new PickRegionRequest(socket);
            emit pickRegionRequested(m_pendingPick);
        },
        [this, socket](const protocol::Screenshot& sc) {
            if (m_pendingPick || m_pendingScreenshot) {
                answer(socket, Error::Busy);
                return;
            }

            m_pendingScreenshot = new ScreenshotRequest(sc.region, sc.annotate, socket);
            emit screenshotRequested(m_pendingScreenshot);
        },
        [this, socket](const protocol::Annotate& an) {
            if (m_pendingAnnotate) {
                answer(socket, Error::Busy);
                return;
            }

            m_pendingAnnotate = new AnnotateRequest(an.file, socket);
            emit annotateRequested(m_pendingAnnotate);
        },
        [this, socket](const protocol::PickFile& pf) {
            auto* const request = new PickFileRequest(pf, socket);
            m_pendingPickFiles.removeAll(nullptr);
            m_pendingPickFiles.append(request);
            emit pickFileRequested(request);
        },
    };
    std::visit(handlers, *request);
}

void Server::release(QLocalSocket* socket) {
    m_server.close();
    if (m_pendingPick)
        m_pendingPick->reject();
    if (m_pendingScreenshot)
        m_pendingScreenshot->reject();
    if (m_pendingAnnotate)
        m_pendingAnnotate->reject();
    for (const auto& request : std::as_const(m_pendingPickFiles)) {
        if (request)
            request->reject();
    }
    m_lock.unlock();

    socket->disconnectFromServer();
    emit released();
}

} // namespace caelestia::ipc
