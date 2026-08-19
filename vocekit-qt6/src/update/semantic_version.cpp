#include "semantic_version.h"

#include <QRegularExpression>

SemanticVersion SemanticVersion::parse(const QString &text)
{
    SemanticVersion version;
    static const QRegularExpression expression(QStringLiteral(
        R"(^[vV]?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?$)"
    ));
    const QRegularExpressionMatch match = expression.match(text.trimmed());
    if (!match.hasMatch()) {
        return version;
    }

    bool majorOk = false;
    bool minorOk = false;
    bool patchOk = false;
    version.major = match.captured(1).toInt(&majorOk);
    version.minor = match.captured(2).toInt(&minorOk);
    version.patch = match.captured(3).toInt(&patchOk);
    if (!majorOk || !minorOk || !patchOk) {
        return SemanticVersion();
    }
    const QString prerelease = match.captured(4);
    if (!prerelease.isEmpty()) {
        version.prerelease = prerelease.split(QLatin1Char('.'));
        for (const QString &part : version.prerelease) {
            if (part.isEmpty()) {
                return SemanticVersion();
            }
        }
    }
    version.valid = true;
    return version;
}

namespace {

int comparePrereleaseIdentifier(const QString &left, const QString &right)
{
    static const QRegularExpression numeric(QStringLiteral(R"(^\d+$)"));
    const bool leftNumeric = numeric.match(left).hasMatch();
    const bool rightNumeric = numeric.match(right).hasMatch();
    if (leftNumeric && rightNumeric) {
        bool leftOk = false;
        bool rightOk = false;
        const qulonglong leftValue = left.toULongLong(&leftOk);
        const qulonglong rightValue = right.toULongLong(&rightOk);
        if (leftOk && rightOk) {
            return leftValue < rightValue ? -1 : (leftValue > rightValue ? 1 : 0);
        }
    }
    if (leftNumeric != rightNumeric) {
        return leftNumeric ? -1 : 1;
    }
    return QString::compare(left, right, Qt::CaseSensitive);
}

} // namespace

int compareSemanticVersions(
    const SemanticVersion &left,
    const SemanticVersion &right)
{
    if (!left.valid || !right.valid) {
        return 0;
    }
    if (left.major != right.major) {
        return left.major < right.major ? -1 : 1;
    }
    if (left.minor != right.minor) {
        return left.minor < right.minor ? -1 : 1;
    }
    if (left.patch != right.patch) {
        return left.patch < right.patch ? -1 : 1;
    }
    if (left.prerelease.isEmpty() != right.prerelease.isEmpty()) {
        return left.prerelease.isEmpty() ? 1 : -1;
    }
    const int count = qMin(left.prerelease.size(), right.prerelease.size());
    for (int index = 0; index < count; ++index) {
        const int compared = comparePrereleaseIdentifier(
            left.prerelease.at(index),
            right.prerelease.at(index)
        );
        if (compared != 0) {
            return compared < 0 ? -1 : 1;
        }
    }
    if (left.prerelease.size() == right.prerelease.size()) {
        return 0;
    }
    return left.prerelease.size() < right.prerelease.size() ? -1 : 1;
}

bool isVersionNewer(const QString &candidate, const QString &current)
{
    const SemanticVersion candidateVersion = SemanticVersion::parse(candidate);
    const SemanticVersion currentVersion = SemanticVersion::parse(current);
    return candidateVersion.valid
        && currentVersion.valid
        && compareSemanticVersions(candidateVersion, currentVersion) > 0;
}
