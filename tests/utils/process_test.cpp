#include <qdir.h>
#include <qtemporarydir.h>
#include <qtest.h>

#include "caelestia/utils/process.hpp"
#include "common.hpp"

using namespace caelestia::utils;
using Qt::StringLiterals::operator""_s;

class TestProcess : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_root;

private slots:

    void initTestCase() { QVERIFY(m_root.isValid()); }

    void runsDetachedWithoutInheritedStdio() {
        const auto out = m_root.filePath(u"out"_s);
        qint64 pid = 0;
        QVERIFY(
            execDetached(u"sh"_s, { u"-c"_s, u"ls -l /proc/$$/fd/0 /proc/$$/fd/1 > \"$1\""_s, u"sh"_s, out }, &pid));
        QVERIFY(pid > 0);
        QTRY_VERIFY(readFile(out).count(u"/dev/null"_s) == 2);
    }

    void failsForMissingProgram() { QVERIFY(!execDetached(m_root.filePath(u"missing"_s), {})); }
};

QTEST_GUILESS_MAIN(TestProcess)

#include "process_test.moc"
