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

struct ScreenshotRequestForeign {
    Q_GADGET
    QML_FOREIGN(caelestia::ipc::ScreenshotRequest)
    QML_NAMED_ELEMENT(ScreenshotRequest)
    QML_UNCREATABLE("Created by IpcServer when a client requests a screenshot")
};

struct AnnotateRequestForeign {
    Q_GADGET
    QML_FOREIGN(caelestia::ipc::AnnotateRequest)
    QML_NAMED_ELEMENT(AnnotateRequest)
    QML_UNCREATABLE("Created by IpcServer when a client requests an annotation")
};

struct FileFilterForeign {
    Q_GADGET
    QML_FOREIGN_NAMESPACE(caelestia::ipc::FileFilter)
    QML_NAMED_ELEMENT(FileFilter)
};

struct PickFileModeForeign {
    Q_GADGET
    QML_FOREIGN_NAMESPACE(caelestia::ipc::PickFileMode)
    QML_NAMED_ELEMENT(PickFileMode)
};

struct PickFileRequestForeign {
    Q_GADGET
    QML_FOREIGN(caelestia::ipc::PickFileRequest)
    QML_NAMED_ELEMENT(PickFileRequest)
    QML_UNCREATABLE("Created by IpcServer when a client requests a file")
};

struct ServerForeign {
    Q_GADGET
    QML_FOREIGN(caelestia::ipc::Server)
    QML_NAMED_ELEMENT(IpcServer)
    QML_SINGLETON
};

} // namespace caelestia::ipc::qml
