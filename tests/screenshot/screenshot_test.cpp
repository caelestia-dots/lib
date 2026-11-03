#include <qguiapplication.h>
#include <qscreen.h>
#include <qtest.h>

#include "caelestia/screenshot/screenshot.hpp"

using namespace caelestia;

class TestScreenshot : public QObject {
    Q_OBJECT

private slots:

    void unsupportedOffscreen() { QVERIFY(!screenshot::isSupported()); }

    void screenFailsOffscreen() {
        auto future = screenshot::capture(QGuiApplication::primaryScreen());
        QVERIFY(future.isFinished());
        QVERIFY(future.result().isNull());
    }

    void nullScreenFails() {
        auto future = screenshot::capture(static_cast<QScreen*>(nullptr));
        QVERIFY(future.isFinished());
        QVERIFY(future.result().isNull());
    }

    void regionOutsideScreensFails() {
        auto future = screenshot::capture(QRect(-10000, -10000, 10, 10));
        QVERIFY(future.isFinished());
        QVERIFY(future.result().isNull());
    }

    void regionFailsOffscreen() {
        auto future = screenshot::capture(QGuiApplication::primaryScreen()->geometry());
        QVERIFY(future.isFinished());
        QVERIFY(future.result().isNull());
    }
};

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    const QGuiApplication app(argc, argv);
    TestScreenshot test;
    return QTest::qExec(&test, argc, argv);
}

#include "screenshot_test.moc"
