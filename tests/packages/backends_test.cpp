#include <qfileinfo.h>
#include <qstandardpaths.h>
#include <qtest.h>

#include "caelestia/packages/packages.hpp"

using namespace caelestia::packages;
using Qt::StringLiterals::operator""_s;

class TestBackends : public QObject {
    Q_OBJECT

private slots:

    void listsUsableBackends() {
        const auto list = backends(this);
#ifdef CAELESTIA_WITH_ALPM
        const bool arch =
            QFileInfo::exists(u"/etc/pacman.conf"_s) && !QStandardPaths::findExecutable(u"pacman-conf"_s).isEmpty();
        QCOMPARE(list.size(), arch ? 1 : 0);
        if (arch)
            QCOMPARE(list.first()->name(), u"Arch"_s);
#else
        QVERIFY(list.isEmpty());
#endif
    }
};

QTEST_GUILESS_MAIN(TestBackends)
#include "backends_test.moc"
