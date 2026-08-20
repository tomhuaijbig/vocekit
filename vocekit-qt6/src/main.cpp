#include "voiceassistant.h"
#include "update/update_service.h"

#include <cstdio>
#include <cstring>
#include <QJsonDocument>
#include <QJsonObject>

#ifndef VOCEKIT_VERSION
#define VOCEKIT_VERSION "0.0.0"
#endif

#ifndef VOCEKIT_SOURCE_COMMIT
#define VOCEKIT_SOURCE_COMMIT "unknown"
#endif

#ifndef VOCEKIT_SOURCE_TREE_CLEAN
#define VOCEKIT_SOURCE_TREE_CLEAN 0
#endif

#ifndef VOCEKIT_BUILD_CONFIGURATION
#define VOCEKIT_BUILD_CONFIGURATION "unknown"
#endif

int main(int argc, char *argv[])
{
    if (argc == 2
        && std::strcmp(argv[1], "--build-provenance-json") == 0) {
        const QJsonObject provenance{
            {QStringLiteral("schema_version"), 3},
            {QStringLiteral("source_commit"), QString::fromLatin1(VOCEKIT_SOURCE_COMMIT)},
            {QStringLiteral("source_tree_clean"), VOCEKIT_SOURCE_TREE_CLEAN != 0},
            {QStringLiteral("configuration"), QString::fromLatin1(VOCEKIT_BUILD_CONFIGURATION)},
            {QStringLiteral("version"), QString::fromLatin1(VOCEKIT_VERSION)},
            {QStringLiteral("update_feed_url"), UpdateService::defaultFeedUrl().toString()}
        };
        const QByteArray json = QJsonDocument(provenance).toJson(QJsonDocument::Compact);
        std::fwrite(json.constData(), 1, static_cast<std::size_t>(json.size()), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        return 0;
    }
    return runVocekit(argc, argv);
}
