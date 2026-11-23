#include "aurhelper.hpp"

#include <qloggingcategory.h>
#include <qregularexpression.h>
#include <qstandardpaths.h>

namespace caelestia::packages {

using Qt::StringLiterals::operator""_s;

namespace {

Q_LOGGING_CATEGORY(lcAurHelper, "caelestia.packages.aur", QtInfoMsg)

constexpr QStringView k_pkexec = u"/usr/bin/pkexec";
constexpr QStringView k_pacman = u"/usr/bin/pacman";
constexpr int k_queryTimeoutMs = 120000;

QString programName(AurHelper helper) {
    switch (helper) {
    case AurHelper::Paru:
        return u"paru"_s;
    case AurHelper::Yay:
        return u"yay"_s;
    }
    Q_UNREACHABLE_RETURN({});
}

QStringList headlessFlags(AurHelper helper) {
    QStringList flags{ u"--sudo"_s, k_pkexec.toString(), u"--noconfirm"_s, u"--color"_s, u"never"_s };
    switch (helper) {
    case AurHelper::Paru:
        flags << u"--skipreview"_s;
        break;
    case AurHelper::Yay:
        flags << u"--answerdiff"_s << u"None"_s << u"--answerclean"_s << u"None"_s << u"--answeredit"_s << u"None"_s
              << u"--answerupgrade"_s << u"None"_s;
        break;
    }
    return flags;
}

} // namespace

AurUpdateQuery::AurUpdateQuery(AurHelper helper) {
    m_process.setProcessEnvironment(transactionEnvironment());
    m_process.setStandardInputFile(QProcess::nullDevice());
    m_process.start(programName(helper), { u"-Qua"_s, u"--color"_s, u"never"_s });
}

QList<Update> AurUpdateQuery::finish() {
    if (!m_process.waitForFinished(k_queryTimeoutMs) || m_process.exitStatus() != QProcess::NormalExit) {
        qCWarning(lcAurHelper) << "AUR update check failed:" << m_process.readAllStandardError().trimmed();
        return {};
    }
    return parseAurUpdates(QString::fromUtf8(m_process.readAllStandardOutput()));
}

std::optional<AurHelper> findAurHelper() {
    for (const auto helper : { AurHelper::Paru, AurHelper::Yay }) {
        if (!QStandardPaths::findExecutable(programName(helper)).isEmpty())
            return helper;
    }
    return std::nullopt;
}

bool isPackageName(const QString& name) {
    static const QRegularExpression k_name(
        uR"(^(?:[A-Za-z0-9@_+][A-Za-z0-9@._+-]*/)?[A-Za-z0-9@_+][A-Za-z0-9@._+-]*$)"_s);
    return k_name.match(name).hasMatch();
}

QProcessEnvironment transactionEnvironment() {
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(u"LC_ALL"_s, u"C"_s);
    env.insert(u"PACMAN_AUTH"_s, k_pkexec.toString());
    return env;
}

Command command(std::optional<AurHelper> helper, const QStringList& operation) {
    if (helper)
        return { .program = programName(*helper), .arguments = headlessFlags(*helper) + operation };
    return {
        .program = k_pkexec.toString(),
        .arguments = QStringList{ k_pacman.toString(), u"--noconfirm"_s, u"--color"_s, u"never"_s } + operation,
    };
}

QList<Update> parseAurUpdates(const QString& output) {
    static const QRegularExpression k_line(uR"(^(\S+)\s+(\S+)\s+->\s+(\S+)$)"_s);

    QList<Update> updates;
    const auto lines = output.split(u'\n', Qt::SkipEmptyParts);
    for (const auto& line : lines) {
        const auto match = k_line.match(line.trimmed());
        if (!match.hasMatch())
            continue;

        updates << Update{
            .package = { .name = match.captured(1), .version = match.captured(2), .source = u"aur"_s },
            .newVersion = match.captured(3),
        };
    }
    return updates;
}

} // namespace caelestia::packages
