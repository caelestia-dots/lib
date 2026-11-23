#include <qdir.h>
#include <qsignalspy.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include <sys/stat.h>

#include "caelestia/packages/packages.hpp"
#include "common.hpp"

using namespace caelestia::packages;
using Qt::StringLiterals::operator""_s;

namespace {

constexpr auto k_fakeProgram = R"(#!/bin/sh
echo "line one"
echo "line two" >&2
printf 'no newline'
exit "${FAKE_EXIT:-0}"
)";

} // namespace

class TestTransaction : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QString m_program;

    [[nodiscard]] static QProcessEnvironment env(const QString& exit) {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(u"FAKE_EXIT"_s, exit);
        return env;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_root.isValid());
        m_program = m_root.filePath(u"fake"_s);
        writeFile(m_program, k_fakeProgram);
        QVERIFY(::chmod(m_program.toUtf8().constData(), 0755) == 0);
    }

    void streamsMergedOutputAndSucceeds() {
        const auto* transaction = Transaction::run(m_program, {}, env(u"0"_s), this);
        const QSignalSpy lines(transaction, &Transaction::outputLine);
        QSignalSpy finished(transaction, &Transaction::finished);

        QVERIFY(transaction->running());
        QVERIFY(finished.wait());
        QVERIFY(!transaction->running());
        QVERIFY(transaction->succeeded());
        QCOMPARE(finished.first().first().toBool(), true);
        QCOMPARE(transaction->output(), (QStringList{ u"line one"_s, u"line two"_s, u"no newline"_s }));
        QCOMPARE(lines.size(), 3);
    }

    void reportsNonZeroExit() {
        const auto* transaction = Transaction::run(m_program, {}, env(u"3"_s), this);
        QSignalSpy finished(transaction, &Transaction::finished);
        QVERIFY(finished.wait());
        QVERIFY(!transaction->succeeded());
        QCOMPARE(transaction->output().size(), 3);
    }

    void reportsMissingProgram() {
        const auto* transaction = Transaction::run(m_root.filePath(u"missing"_s), {}, env(u"0"_s), this);
        QSignalSpy finished(transaction, &Transaction::finished);
        QVERIFY(finished.wait());
        QVERIFY(!transaction->succeeded());
        QVERIFY(!transaction->output().isEmpty());
    }

    void failedFinishesAsynchronously() {
        const auto* transaction = Transaction::failed(u"nope"_s, this);
        QSignalSpy finished(transaction, &Transaction::finished);
        QVERIFY(transaction->running());
        QVERIFY(finished.wait());
        QVERIFY(!transaction->succeeded());
        QCOMPARE(transaction->output(), QStringList{ u"nope"_s });
    }
};

QTEST_GUILESS_MAIN(TestTransaction)
#include "transaction_test.moc"
