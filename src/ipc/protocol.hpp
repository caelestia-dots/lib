#pragma once

#include <qbytearray.h>
#include <qlatin1stringview.h>
#include <qrect.h>
#include <qstring.h>

#include <optional>
#include <variant>

#include "caelestia/ipc/common.hpp"

namespace caelestia::ipc::protocol {

using Qt::StringLiterals::operator""_L1;

template <typename... Ts> struct Overloaded : Ts... {
    using Ts::operator()...;
};

struct Release {
    static constexpr auto k_name = "release"_L1;
};

struct PickRegion {
    static constexpr auto k_name = "pickRegion"_L1;

    bool freeze = false;
};

using Request = std::variant<Release, PickRegion>;

[[nodiscard]] QString socketPath();
[[nodiscard]] QString lockPath();

[[nodiscard]] QByteArray encode(const Request& request);
[[nodiscard]] QByteArray encode(const Response& response);
[[nodiscard]] std::optional<Request> decodeRequest(const QByteArray& line);
[[nodiscard]] Response decodeResponse(const QByteArray& line);

} // namespace caelestia::ipc::protocol
