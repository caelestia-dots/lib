#include "protocol.hpp"

#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>

#include <utility>

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

QStringList toStringList(const QJsonValue& value) {
    QStringList list;
    const auto array = value.toArray();
    for (const auto& item : array)
        list << item.toString();
    return list;
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
        [&obj](const PickRegion&) {
            obj[u"method"] = PickRegion::k_name;
        },
        [&obj](const Screenshot& sc) {
            obj[u"method"] = Screenshot::k_name;
            obj[u"region"] = sc.region;
            obj[u"annotate"] = sc.annotate;
        },
        [&obj](const Annotate& an) {
            obj[u"method"] = Annotate::k_name;
            obj[u"file"] = an.file;
        },
        [&obj](const PickFile& pf) {
            obj[u"method"] = PickFile::k_name;
            obj[u"title"] = pf.title;
            obj[u"nameFilters"] = QJsonArray::fromStringList(pf.nameFilters);
            obj[u"filter"] = static_cast<int>(pf.filter);
            obj[u"initialDir"] = pf.initialDir;
            obj[u"initialName"] = pf.initialName;
            obj[u"mode"] = pf.mode;
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
        [&obj](const QString& file) {
            obj[u"file"] = file;
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
        return PickRegion{};

    if (method == Screenshot::k_name) {
        return Screenshot{
            .region = obj.value(u"region").toBool(),
            .annotate = obj.value(u"annotate").toBool(),
        };
    }

    if (method == Annotate::k_name)
        return Annotate{ .file = obj.value(u"file").toString() };

    if (method == PickFile::k_name) {
        PickFileOptions options{
            .title = obj.value(u"title").toString(),
            .nameFilters = toStringList(obj.value(u"nameFilters")),
            .filter = FileFilter::Flags(obj.value(u"filter").toInt()),
            .initialDir = obj.value(u"initialDir").toString(),
            .initialName = obj.value(u"initialName").toString(),
            .mode = PickFileMode::Enum(obj.value(u"mode").toInt()),
        };
        return PickFile{ std::move(options) };
    }

    return std::nullopt;
}

Response decodeResponse(const QByteArray& line) {
    const auto obj = QJsonDocument::fromJson(line).object();
    if (obj.contains(u"error"))
        return errorFromString(obj.value(u"error").toString());

    if (obj.contains(u"region")) {
        const auto region = obj.value(u"region").toArray();
        if (region.size() != 4)
            return Error::Failed;

        return QRect(region[0].toInt(), region[1].toInt(), region[2].toInt(), region[3].toInt());
    }

    if (obj.contains(u"file"))
        return obj.value(u"file").toString();

    return Error::Failed;
}

} // namespace caelestia::ipc::protocol
