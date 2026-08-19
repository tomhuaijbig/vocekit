#ifndef VOCEKIT_SEMANTIC_VERSION_H
#define VOCEKIT_SEMANTIC_VERSION_H

#include <QString>
#include <QStringList>

struct SemanticVersion
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    QStringList prerelease;
    bool valid = false;

    static SemanticVersion parse(const QString &text);
};

int compareSemanticVersions(
    const SemanticVersion &left,
    const SemanticVersion &right
);
bool isVersionNewer(const QString &candidate, const QString &current);

#endif // VOCEKIT_SEMANTIC_VERSION_H
