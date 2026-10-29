#include "protocol.hpp"

#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>

#include "caelestia/config/paths.hpp"

namespace caelestia::ipc::protocol {

namespace {

QLatin1StringView toString(Error error) {
    switch (error) {
    case Error::Busy:
        return "busy"_L1;
    case Error::Cancelled:
        return "cancelled"_L1;
    case Error::NotRunning:
    case Error::Failed:
        return "failed"_L1;
    }
    Q_UNREACHABLE_RETURN("failed"_L1);
}

Error errorFromString(const QString& name) {
    if (name == "busy"_L1)
        return Error::Busy;
    if (name == "cancelled"_L1)
        return Error::Cancelled;
    return Error::Failed;
}

QByteArray toLine(const QJsonObject& obj) {
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
}

} // namespace

QString socketPath() {
    return config::ConfigPaths::runtimeDir() + "/shell.sock"_L1;
}

QString lockPath() {
    return config::ConfigPaths::runtimeDir() + "/shell.lock"_L1;
}

QByteArray encode(const Request& request) {
    QJsonObject obj;
    const Overloaded overloads{
        [&obj](const Release&) {
            obj[u"method"] = Release::k_name;
        },
        [&obj](const PickRegion& pick) {
            obj[u"method"] = PickRegion::k_name;
            obj[u"freeze"] = pick.freeze;
        },
    };
    std::visit(overloads, request);
    return toLine(obj);
}

QByteArray encode(const Response& response) {
    QJsonObject obj;
    const Overloaded overloads{
        [&obj](Error error) {
            obj[u"error"] = toString(error);
        },
        [&obj](const QRect& r) {
            obj[u"region"] = QJsonArray{ r.x(), r.y(), r.width(), r.height() };
        },
    };
    std::visit(overloads, response);
    return toLine(obj);
}

std::optional<Request> decodeRequest(const QByteArray& line) {
    const auto obj = QJsonDocument::fromJson(line).object();
    const auto method = obj.value(u"method").toString();

    if (method == Release::k_name)
        return Release{};

    if (method == PickRegion::k_name)
        return PickRegion{ .freeze = obj.value(u"freeze").toBool() };

    return std::nullopt;
}

Response decodeResponse(const QByteArray& line) {
    const auto obj = QJsonDocument::fromJson(line).object();
    if (obj.contains(u"error"))
        return errorFromString(obj.value(u"error").toString());

    const auto region = obj.value(u"region").toArray();
    if (region.size() != 4)
        return Error::Failed;

    return QRect(region[0].toInt(), region[1].toInt(), region[2].toInt(), region[3].toInt());
}

} // namespace caelestia::ipc::protocol
