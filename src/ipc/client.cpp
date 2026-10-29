#include "caelestia/ipc/client.hpp"

#include <qlocalsocket.h>
#include <qloggingcategory.h>

#include <variant>

#include "protocol.hpp"

namespace caelestia::ipc {

namespace {

Q_LOGGING_CATEGORY(lcIpc, "caelestia.ipc", QtInfoMsg)

constexpr int k_connectTimeoutMs = 1000;

template <typename T> Result<T> expect(const Response& response) {
    if (const auto* error = std::get_if<Error>(&response))
        return *error;
    if (const auto* value = std::get_if<T>(&response))
        return *value;

    qCWarning(lcIpc) << "Unexpected reply payload";
    return Error::Failed;
}

bool waitForLine(QLocalSocket& socket) {
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(-1))
            return false;
    }
    return true;
}

} // namespace

Result<QRect> pickRegion(bool freeze) {
    QLocalSocket socket;
    socket.connectToServer(protocol::socketPath());
    if (!socket.waitForConnected(k_connectTimeoutMs))
        return Error::NotRunning;

    socket.write(protocol::encode(protocol::PickRegion{ .freeze = freeze }));
    if (!waitForLine(socket)) {
        qCWarning(lcIpc) << "No reply from the shell:" << socket.errorString();
        return Error::Failed;
    }

    return expect<QRect>(protocol::decodeResponse(socket.readLine()));
}

} // namespace caelestia::ipc
