#pragma once

#include <qqmlintegration.h>

#include "caelestia/ipc/server.hpp"

namespace caelestia::ipc::qml {

struct RequestForeign {
    Q_GADGET
    QML_FOREIGN(caelestia::ipc::Request)
    QML_ANONYMOUS
};

struct PickRegionRequestForeign {
    Q_GADGET
    QML_FOREIGN(caelestia::ipc::PickRegionRequest)
    QML_NAMED_ELEMENT(PickRegionRequest)
    QML_UNCREATABLE("Created by IpcServer when a client requests a region")
};

struct ServerForeign {
    Q_GADGET
    QML_FOREIGN(caelestia::ipc::Server)
    QML_NAMED_ELEMENT(IpcServer)
    QML_SINGLETON
};

} // namespace caelestia::ipc::qml
