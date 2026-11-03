#include <qqmlcomponent.h>
#include <qqmlengine.h>
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
    property bool freeze

    Component.onCompleted: IpcServer.pickRegionRequested.connect(request => {
        freeze = request.freeze;
        request.respond(Qt.rect(10, 20, 300, 200));
    })
}
)";

class TestQml : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        QCOMPARE(chmod(m_root.path().toUtf8().constData(), 0700), 0);
        qputenv("XDG_RUNTIME_DIR", m_root.path().toUtf8());
    }

    void respondsThroughQml() {
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(CAELESTIA_QML_DIR));

        QQmlComponent component(&engine);
        component.setData(k_shell, QUrl());
        const std::unique_ptr<QObject> shell(component.create());
        QVERIFY2(shell, qPrintable(component.errorString()));

        auto future = std::async(std::launch::async, [] {
            return ipc::pickRegion(true);
        });
        QTRY_VERIFY(future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);

        const auto result = future.get();
        QVERIFY(std::holds_alternative<QRect>(result));
        QCOMPARE(std::get<QRect>(result), QRect(10, 20, 300, 200));
        QVERIFY(shell->property("freeze").toBool());
    }
};

} // namespace

QTEST_GUILESS_MAIN(TestQml)

#include "qml_test.moc"
