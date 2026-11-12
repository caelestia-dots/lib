#include <qbytearray.h>
#include <qcommandlineoption.h>
#include <qcommandlineparser.h>
#include <qcoreapplication.h>
#include <qhash.h>
#include <qmetaobject.h>
#include <qobject.h>
#include <qpointer.h>
#include <qrect.h>
#include <qsocketnotifier.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtextstream.h>

#include <unistd.h>

#include <array>
#include <variant>

#include "caelestia/ipc/client.hpp"
#include "caelestia/ipc/server.hpp"

using namespace caelestia;
using namespace Qt::StringLiterals;

namespace {

QTextStream& out() {
    static QTextStream s_stream(stdout);
    return s_stream;
}

QString toString(ipc::Error error) {
    switch (error) {
    case ipc::Error::NotRunning:
        return u"not running"_s;
    case ipc::Error::Busy:
        return u"busy"_s;
    case ipc::Error::Cancelled:
        return u"cancelled"_s;
    case ipc::Error::Failed:
        return u"failed"_s;
    }
    Q_UNREACHABLE_RETURN({});
}

QString toString(const QRect& rect) {
    return u"%1 %2 %3 %4"_s.arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

QString toString(const QString& string) {
    return string;
}

QString toString(const ipc::SaveResult& result) {
    return result.copied ? u"copied %1"_s.arg(result.file) : result.file;
}

template <typename T> int report(const ipc::Result<T>& result) {
    if (const auto* value = std::get_if<T>(&result)) {
        out() << toString(*value) << Qt::endl;
        return 0;
    }

    if (const auto* error = std::get_if<ipc::Error>(&result))
        out() << "error: " << toString(*error) << Qt::endl;
    return 1;
}

QString describe(const ipc::ScreenshotRequest* request) {
    return u"screenshot region=[%1] annotate=%2"_s.arg(toString(request->region())).arg(request->annotate());
}

QString describe(const ipc::AnnotateRequest* request) {
    return u"annotate file=%1"_s.arg(request->file());
}

QString describe(const ipc::PickFileRequest* request) {
    const auto filter = QMetaEnum::fromType<ipc::FileFilter::Flags>().valueToKeys(request->filter());
    const auto* const mode = QMetaEnum::fromType<ipc::PickFileMode::Enum>().valueToKey(request->mode());
    return u"pick-file title=\"%1\" nameFilters=[%2] filter=%3 dir=%4 name=%5 mode=%6"_s.arg(request->title(),
        request->nameFilters().join(u", "_s), QString::fromLatin1(filter), request->initialDir(),
        request->initialName(), QString::fromLatin1(mode));
}

class FakeShell : public QObject {
public:
    explicit FakeShell(ipc::Server& server)
        : m_stdin(fileno(stdin), QSocketNotifier::Read) {
        connect(&server, &ipc::Server::pickRegionRequested, this, [this](ipc::PickRegionRequest* request) {
            add(request, u"pick-region"_s);
        });
        connect(&server, &ipc::Server::screenshotRequested, this, [this](ipc::ScreenshotRequest* request) {
            add(request, describe(request));
        });
        connect(&server, &ipc::Server::annotateRequested, this, [this](ipc::AnnotateRequest* request) {
            add(request, describe(request));
        });
        connect(&server, &ipc::Server::pickFileRequested, this, [this](ipc::PickFileRequest* request) {
            add(request, describe(request));
        });
        connect(&server, &ipc::Server::released, this, [] {
            out() << "released by another shell, quitting" << Qt::endl;
            QCoreApplication::quit();
        });
        connect(&m_stdin, &QSocketNotifier::activated, this, &FakeShell::readInput);
    }

private:
    QSocketNotifier m_stdin;
    QByteArray m_input;
    QHash<int, QPointer<ipc::Request>> m_pending;
    int m_nextId = 0;

    void add(ipc::Request* request, const QString& description) {
        const auto id = ++m_nextId;
        m_pending.insert(id, request);
        out() << "[" << id << "] " << description << Qt::endl;
        connect(request, &ipc::Request::cancelled, this, [this, id] {
            out() << "[" << id << "] cancelled by client" << Qt::endl;
            m_pending.remove(id);
        });
    }

    void readInput() {
        std::array<char, 4096> chunk{};
        const auto count = read(STDIN_FILENO, chunk.data(), chunk.size());
        if (count <= 0) {
            QCoreApplication::quit();
            return;
        }

        m_input.append(chunk.data(), count);
        for (auto end = m_input.indexOf('\n'); end >= 0; end = m_input.indexOf('\n')) {
            handle(QString::fromUtf8(m_input.first(end)).split(u' ', Qt::SkipEmptyParts));
            m_input.remove(0, end + 1);
        }
    }

    void handle(const QStringList& words) {
        if (words.isEmpty())
            return;

        if (words[0] == "quit"_L1) {
            QCoreApplication::quit();
            return;
        }

        bool ok = false;
        const auto id = words[0].toInt(&ok);
        auto* const request = m_pending.value(id).data();
        if (!ok || !request) {
            out() << "no pending request " << words[0] << Qt::endl;
            return;
        }

        if (!reply(request, words.mid(1))) {
            out() << "cannot apply that reply to [" << id << "]" << Qt::endl;
            return;
        }

        m_pending.remove(id);
    }

    static bool reply(ipc::Request* request, const QStringList& args) {
        if (args.isEmpty())
            return false;

        if (args[0] == "reject"_L1) {
            request->reject();
            return true;
        }

        if (args[0] == "region"_L1 && args.size() == 5) {
            if (auto* const pick = qobject_cast<ipc::PickRegionRequest*>(request)) {
                pick->respond(QRect(args[1].toInt(), args[2].toInt(), args[3].toInt(), args[4].toInt()));
                return true;
            }
        }

        if (args[0] == "copied"_L1 && args.size() == 2) {
            if (auto* const screenshot = qobject_cast<ipc::ScreenshotRequest*>(request)) {
                screenshot->respondCopied(args[1]);
                return true;
            }
            if (auto* const annotate = qobject_cast<ipc::AnnotateRequest*>(request)) {
                annotate->respondCopied(args[1]);
                return true;
            }
        }

        if (args[0] == "file"_L1 && args.size() == 2) {
            if (auto* const screenshot = qobject_cast<ipc::ScreenshotRequest*>(request)) {
                screenshot->respond(args[1]);
                return true;
            }
            if (auto* const annotate = qobject_cast<ipc::AnnotateRequest*>(request)) {
                annotate->respond(args[1]);
                return true;
            }
            if (auto* const pickFile = qobject_cast<ipc::PickFileRequest*>(request)) {
                pickFile->respond(args[1]);
                return true;
            }
        }

        return false;
    }
};

int serve() {
    ipc::Server server;
    if (!server.isListening()) {
        out() << "failed to listen on the ipc socket" << Qt::endl;
        return 1;
    }

    const FakeShell shell(server);
    out() << "listening (any running shell has been released)\n"
             "reply with: <id> reject | <id> region <x> <y> <w> <h> | <id> file <path> | <id> copied <path> | quit"
          << Qt::endl;
    return QCoreApplication::exec();
}

} // namespace

int main(int argc, char* argv[]) {
    const QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(u"Manual ipc tester: drive a running shell, or stand in for one with `serve`"_s);
    parser.addHelpOption();
    parser.addPositionalArgument(u"command"_s, u"serve | pick-region | screenshot | annotate <file> | pick-file"_s);

    const QCommandLineOption annotate(u"annotate"_s, u"screenshot: annotate afterwards"_s);
    const QCommandLineOption region(
        u"region"_s, u"screenshot: capture this region instead of picking one"_s, u"x,y,w,h"_s);
    const QCommandLineOption title(u"title"_s, u"pick-file: dialog title"_s, u"title"_s);
    const QCommandLineOption nameFilter(u"name-filter"_s, u"pick-file: name filter (repeatable)"_s, u"pattern"_s);
    const QCommandLineOption filter(u"filter"_s, u"pick-file: Images|Videos|Files|Dirs"_s, u"flags"_s, u"NoFilter"_s);
    const QCommandLineOption dir(u"dir"_s, u"pick-file: initial directory"_s, u"path"_s);
    const QCommandLineOption name(u"name"_s, u"pick-file: initial file name"_s, u"name"_s);
    const QCommandLineOption mode(u"mode"_s, u"pick-file: Select|Open|Save"_s, u"mode"_s, u"Select"_s);
    parser.addOptions({ annotate, region, title, nameFilter, filter, dir, name, mode });
    parser.process(app);

    const auto args = parser.positionalArguments();
    if (args.isEmpty())
        parser.showHelp(1);
    const auto& command = args[0];

    if (command == "serve"_L1)
        return serve();

    if (command == "pick-region"_L1)
        return report(ipc::pickRegion());

    if (command == "screenshot"_L1) {
        if (parser.isSet(region)) {
            const auto parts = parser.value(region).split(u',');
            if (parts.size() != 4)
                parser.showHelp(1);
            return report(ipc::screenshot(
                QRect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt()), parser.isSet(annotate)));
        }
        return report(ipc::screenshot(parser.isSet(annotate)));
    }

    if (command == "annotate"_L1) {
        if (args.size() < 2)
            parser.showHelp(1);
        return report(ipc::annotate(args[1]));
    }

    if (command == "pick-file"_L1) {
        bool filterOk = false;
        bool modeOk = false;
        const auto filterValue = QMetaEnum::fromType<ipc::FileFilter::Flags>().keysToValue(
            parser.value(filter).toLatin1().constData(), &filterOk);
        const auto modeValue = QMetaEnum::fromType<ipc::PickFileMode::Enum>().keyToValue(
            parser.value(mode).toLatin1().constData(), &modeOk);
        if (!filterOk || !modeOk)
            parser.showHelp(1);

        return report(ipc::pickFile({
            .title = parser.value(title),
            .nameFilters = parser.values(nameFilter),
            .filter = ipc::FileFilter::Flags(filterValue),
            .initialDir = parser.value(dir),
            .initialName = parser.value(name),
            .mode = ipc::PickFileMode::Enum(modeValue),
        }));
    }

    parser.showHelp(1);
}
