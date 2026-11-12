#include <qqmlcomponent.h>
#include <qqmlengine.h>
#include <qrect.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include <sys/stat.h>

#include <chrono>
#include <future>
#include <memory>
#include <variant>

#include "caelestia/ipc/client.hpp"

using namespace caelestia;

namespace {

constexpr auto k_shell = R"(
import QtQml
import Caelestia.Ipc

QtObject {
    property bool annotate
    property rect region
    property string annotated
    property string pickTitle
    property list<string> pickNameFilters
    property int pickFilter
    property string pickInitialDir
    property string pickInitialName
    property int pickMode
    property bool failPick

    property IpcServer server: IpcServer {
        onPickRegionRequested: request => {
            if (failPick)
                request.reject(IpcError.Failed);
            else
                request.respond(Qt.rect(10, 20, 300, 200));
        }
        onScreenshotRequested: request => {
            annotate = request.annotate;
            region = request.region;
            request.respond("/tmp/shot.png");
        }
        onAnnotateRequested: request => {
            annotated = request.file;
            request.respond(request.file + ".edited.png");
        }
        onPickFileRequested: request => {
            pickTitle = request.title;
            pickNameFilters = request.nameFilters;
            pickFilter = request.filter;
            pickInitialDir = request.initialDir;
            pickInitialName = request.initialName;
            pickMode = request.mode;
            request.respond("/tmp/picked.png");
        }
    }
}
)";

class TestQml : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QObject> m_shell;

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        QCOMPARE(chmod(m_root.path().toUtf8().constData(), 0700), 0);
        qputenv("XDG_RUNTIME_DIR", m_root.path().toUtf8());
    }

    void init() {
        m_engine = std::make_unique<QQmlEngine>();
        m_engine->addImportPath(QStringLiteral(CAELESTIA_QML_DIR));

        QQmlComponent component(m_engine.get());
        component.setData(k_shell, QUrl());
        m_shell.reset(component.create());
        QVERIFY2(m_shell, qPrintable(component.errorString()));
    }

    void cleanup() {
        m_shell.reset();
        m_engine.reset();
    }

    void pickRegionThroughQml() {
        auto future = std::async(std::launch::async, [] {
            return ipc::pickRegion();
        });
        QTRY_VERIFY(future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

        const auto result = future.get();
        QVERIFY(std::holds_alternative<QRect>(result));
        QCOMPARE(std::get<QRect>(result), QRect(10, 20, 300, 200));
    }

    void rejectWithErrorThroughQml() {
        m_shell->setProperty("failPick", true);

        auto future = std::async(std::launch::async, [] {
            return ipc::pickRegion();
        });
        QTRY_VERIFY(future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

        const auto result = future.get();
        QVERIFY(std::holds_alternative<ipc::Error>(result));
        QCOMPARE(std::get<ipc::Error>(result), ipc::Error::Failed);
    }

    void screenshotThroughQml() {
        auto future = std::async(std::launch::async, [] {
            return ipc::screenshot(false);
        });
        QTRY_VERIFY(future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

        const auto result = future.get();
        QVERIFY(std::holds_alternative<ipc::SaveResult>(result));
        QCOMPARE(std::get<ipc::SaveResult>(result).file, QStringLiteral("/tmp/shot.png"));
        QVERIFY(!std::get<ipc::SaveResult>(result).copied);
        QVERIFY(!m_shell->property("annotate").toBool());
        QVERIFY(m_shell->property("region").toRect().isNull());
    }

    void screenshotRegionThroughQml() {
        auto future = std::async(std::launch::async, [] {
            return ipc::screenshot(QRect(1, 2, 30, 40), false);
        });
        QTRY_VERIFY(future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

        QVERIFY(std::holds_alternative<ipc::SaveResult>(future.get()));
        QCOMPARE(m_shell->property("region").toRect(), QRect(1, 2, 30, 40));
    }

    void annotateThroughQml() {
        auto future = std::async(std::launch::async, [] {
            return ipc::annotate(QStringLiteral("/tmp/in.png"));
        });
        QTRY_VERIFY(future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

        const auto result = future.get();
        QVERIFY(std::holds_alternative<ipc::SaveResult>(result));
        QCOMPARE(std::get<ipc::SaveResult>(result).file, QStringLiteral("/tmp/in.png.edited.png"));
        QVERIFY(!std::get<ipc::SaveResult>(result).copied);
        QCOMPARE(m_shell->property("annotated").toString(), QStringLiteral("/tmp/in.png"));
    }

    void pickFileThroughQml() {
        auto future = std::async(std::launch::async, [] {
            return ipc::pickFile({
                .title = QStringLiteral("Pick"),
                .nameFilters = { QStringLiteral("*.png") },
                .filter = ipc::FileFilter::Images | ipc::FileFilter::Files,
                .initialDir = QStringLiteral("/tmp"),
                .initialName = QStringLiteral("shot.png"),
                .mode = ipc::PickFileMode::Save,
            });
        });
        QTRY_VERIFY(future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

        const auto result = future.get();
        QVERIFY(std::holds_alternative<QString>(result));
        QCOMPARE(std::get<QString>(result), QStringLiteral("/tmp/picked.png"));
        QCOMPARE(m_shell->property("pickTitle").toString(), QStringLiteral("Pick"));
        QCOMPARE(m_shell->property("pickNameFilters").toStringList(), QStringList{ QStringLiteral("*.png") });
        QCOMPARE(m_shell->property("pickFilter").toInt(), (ipc::FileFilter::Images | ipc::FileFilter::Files).toInt());
        QCOMPARE(m_shell->property("pickInitialDir").toString(), QStringLiteral("/tmp"));
        QCOMPARE(m_shell->property("pickInitialName").toString(), QStringLiteral("shot.png"));
        QCOMPARE(m_shell->property("pickMode").toInt(), ipc::PickFileMode::Save);
    }
};

} // namespace

QTEST_GUILESS_MAIN(TestQml)

#include "qml_test.moc"
