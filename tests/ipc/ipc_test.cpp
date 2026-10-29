#include <qlocalsocket.h>
#include <qpointer.h>
#include <qsignalspy.h>
#include <qtemporarydir.h>
#include <qtest.h>
#include <qthread.h>

#include <sys/stat.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <variant>

#include "caelestia/ipc/client.hpp"
#include "caelestia/ipc/server.hpp"
#include "common.hpp"

using namespace caelestia;
using Qt::StringLiterals::operator""_s;

namespace {

using Picked = ipc::Result<QRect>;

class TestIpc : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    std::unique_ptr<ipc::Server> m_server;
    QPointer<ipc::PickRegionRequest> m_request;

    [[nodiscard]] QString socketPath() const { return m_root.filePath(u"caelestia/shell.sock"_s); }

    [[nodiscard]] static std::future<Picked> pick(bool freeze) {
        return std::async(std::launch::async, [freeze] {
            return ipc::pickRegion(freeze);
        });
    }

    [[nodiscard]] static bool ready(const std::future<Picked>& future) {
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    // The server runs on this thread, so events must be pumped until the client has its reply.
    [[nodiscard]] static Picked await(std::future<Picked>& future) {
        static_cast<void>(QTest::qWaitFor([&future] {
            return ready(future);
        }));
        return future.get();
    }

    [[nodiscard]] static ipc::Error errorOf(const Picked& picked) {
        const auto* error = std::get_if<ipc::Error>(&picked);
        return error ? *error : ipc::Error{ 255 };
    }

    [[nodiscard]] std::unique_ptr<ipc::Server> startServer() {
        auto server = std::make_unique<ipc::Server>();
        connect(server.get(), &ipc::Server::pickRegionRequested, this, [this](ipc::PickRegionRequest* request) {
            m_request = request;
        });
        return server;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        QCOMPARE(chmod(m_root.path().toUtf8().constData(), 0700), 0);
        qputenv("XDG_RUNTIME_DIR", m_root.path().toUtf8());
    }

    void init() {
        m_server = startServer();
        QVERIFY(m_server->isListening());
    }

    void cleanup() {
        m_server.reset();
        m_request.clear();
    }

    void failsWithoutServer() {
        m_server.reset();
        QCOMPARE(errorOf(ipc::pickRegion(false)), ipc::Error::NotRunning);
    }

    void respondReturnsRegion() {
        auto future = pick(true);
        QTRY_VERIFY(m_request);
        QVERIFY(m_request->freeze());

        m_request->respond(QRect(10, 20, 300, 200));
        QTRY_VERIFY(m_request.isNull());

        QCOMPARE(std::get<QRect>(await(future)), QRect(10, 20, 300, 200));
    }

    void rejectReportsCancelled() {
        auto future = pick(false);
        QTRY_VERIFY(m_request);
        QVERIFY(!m_request->freeze());

        m_request->reject();
        QTRY_VERIFY(m_request.isNull());

        QCOMPARE(errorOf(await(future)), ipc::Error::Cancelled);
    }

    void secondPickIsBusy() {
        auto first = pick(false);
        QTRY_VERIFY(m_request);

        auto second = pick(false);
        QCOMPARE(errorOf(await(second)), ipc::Error::Busy);

        m_request->reject();
        QCOMPARE(errorOf(await(first)), ipc::Error::Cancelled);
    }

    void respondTwiceSendsOnce() {
        auto future = pick(false);
        QTRY_VERIFY(m_request);

        auto* request = m_request.data();
        request->respond(QRect(1, 2, 3, 4));
        request->reject();
        QTRY_VERIFY(m_request.isNull());

        QCOMPARE(std::get<QRect>(await(future)), QRect(1, 2, 3, 4));
    }

    void serverFreesAfterResponse() {
        auto first = pick(false);
        QTRY_VERIFY(m_request);
        m_request->reject();
        QCOMPARE(errorOf(await(first)), ipc::Error::Cancelled);

        auto second = pick(false);
        QTRY_VERIFY(m_request);
        m_request->respond(QRect(0, 0, 1, 1));
        QCOMPARE(std::get<QRect>(await(second)), QRect(0, 0, 1, 1));
    }

    void clientDisconnectCancels() {
        QLocalSocket client;
        client.connectToServer(socketPath());
        QVERIFY(client.waitForConnected());
        client.write("{\"method\":\"pickRegion\",\"freeze\":false}\n");
        QTRY_VERIFY(m_request);

        const QSignalSpy cancelled(m_request.data(), &ipc::Request::cancelled);
        client.disconnectFromServer();
        QTRY_COMPARE(cancelled.size(), 1);
        QTRY_VERIFY(m_request.isNull());

        auto future = pick(false);
        QTRY_VERIFY(m_request);
        m_request->respond(QRect(0, 0, 1, 1));
        QCOMPARE(std::get<QRect>(await(future)), QRect(0, 0, 1, 1));
    }

    void unknownMethodFails() {
        QLocalSocket client;
        client.connectToServer(socketPath());
        QVERIFY(client.waitForConnected());
        client.write("{\"method\":\"nope\"}\n");
        QTRY_VERIFY(client.canReadLine());
        QCOMPARE(client.readLine(), "{\"error\":\"failed\"}\n");
    }

    // The old server lives on its own thread so the takeover handshake runs between two event loops.
    void newServerTakesOver() {
        m_server.reset();

        QThread thread;
        ipc::Server* old = nullptr;
        std::atomic<bool> picking = false;
        std::atomic<bool> released = false;
        auto* pickingFlag = &picking;
        auto* releasedFlag = &released;
        connect(
            &thread, &QThread::started, &thread,
            [&old, pickingFlag, releasedFlag] {
                old = new ipc::Server();
                connect(old, &ipc::Server::pickRegionRequested, old, [pickingFlag] {
                    *pickingFlag = true;
                });
                connect(old, &ipc::Server::released, old, [releasedFlag] {
                    *releasedFlag = true;
                });
            },
            Qt::DirectConnection);
        thread.start();
        QTRY_VERIFY(old != nullptr && old->isListening());

        auto pending = pick(false);
        QTRY_VERIFY(picking);

        m_server = startServer();
        QVERIFY(m_server->isListening());
        QTRY_VERIFY(released);
        QVERIFY(!old->isListening());
        QCOMPARE(errorOf(await(pending)), ipc::Error::Cancelled);

        QMetaObject::invokeMethod(
            old,
            [old] {
                delete old;
            },
            Qt::BlockingQueuedConnection);
        thread.quit();
        QVERIFY(thread.wait());

        auto future = pick(true);
        QTRY_VERIFY(m_request);
        m_request->respond(QRect(5, 6, 7, 8));
        QCOMPARE(std::get<QRect>(await(future)), QRect(5, 6, 7, 8));
    }

    void staleSocketIsReplaced() {
        m_server.reset();
        writeFile(socketPath(), "stale");

        m_server = startServer();
        QVERIFY(m_server->isListening());
    }
};

} // namespace

QTEST_GUILESS_MAIN(TestIpc)

#include "ipc_test.moc"
