#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class CompatibilityTests : public QObject
{
    Q_OBJECT

private:
    static QString fixturePath(const QString &relativePath)
    {
        return QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(
                QStringLiteral("../../fixtures/") + relativePath
            );
    }

    static QJsonDocument loadDocument(const QString &relativePath)
    {
        QFile file(fixturePath(relativePath));
        if (!file.open(QIODevice::ReadOnly)) {
            return QJsonDocument();
        }
        QJsonParseError error;
        const QJsonDocument document =
            QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError) {
            return QJsonDocument();
        }
        return document;
    }

private slots:
    void settingsFixtureKeepsCurrentShape()
    {
        const QJsonDocument document =
            loadDocument(QStringLiteral("settings/current_settings.json"));
        QVERIFY(document.isObject());
        const QJsonObject root = document.object();
        QCOMPARE(
            root.value(QStringLiteral("functionOrder")).toArray().size(),
            4
        );
        QCOMPARE(
            root.value(QStringLiteral("models"))
                .toObject()
                .value(QStringLiteral("ask"))
                .toString(),
            QStringLiteral("deepseek-v4-pro")
        );
        const QJsonObject dictate = root
            .value(QStringLiteral("inputModes"))
            .toObject()
            .value(QStringLiteral("dictate"))
            .toObject();
        QVERIFY(dictate.value(QStringLiteral("useVoice")).toBool());
        QVERIFY(!dictate.value(QStringLiteral("useSelection")).toBool());
        QCOMPARE(
            root.value(QStringLiteral("customFunctions"))
                .toArray()
                .at(0)
                .toObject()
                .value(QStringLiteral("id"))
                .toString(),
            QStringLiteral("custom_1")
        );
    }

    void historyFixtureKeepsDetailedFields()
    {
        const QJsonDocument document =
            loadDocument(QStringLiteral("history/history_record.json"));
        QVERIFY(document.isObject());
        const QJsonObject record = document.object();
        QCOMPARE(
            record.value(QStringLiteral("modeId")).toString(),
            QStringLiteral("dictate")
        );
        QVERIFY(record.value(QStringLiteral("longRecording")).toBool());
        QCOMPARE(
            record.value(QStringLiteral("segments")).toArray().size(),
            2
        );
        QVERIFY(record.value(QStringLiteral("favorite")).toBool());
        QVERIFY(record.value(QStringLiteral("elapsedMs")).toInt() > 0);
    }

    void historyIndexFixtureIsSearchable()
    {
        const QJsonDocument document =
            loadDocument(QStringLiteral("history/history_index.json"));
        QVERIFY(document.isObject());
        const QJsonArray records = document
            .object()
            .value(QStringLiteral("records"))
            .toArray();
        QCOMPARE(records.size(), 1);
        QVERIFY(
            records.at(0)
                .toObject()
                .value(QStringLiteral("detailFile"))
                .toString()
                .contains(QStringLiteral("详细记录"))
        );
    }

    void vocabularyFixtureKeepsScopesAndAliases()
    {
        const QJsonDocument document =
            loadDocument(QStringLiteral("vocabulary/entries.json"));
        QVERIFY(document.isObject());
        const QJsonArray entries = document
            .object()
            .value(QStringLiteral("entries"))
            .toArray();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(
            entries.at(0)
                .toObject()
                .value(QStringLiteral("scopeId"))
                .toString(),
            QStringLiteral("__global")
        );
        QCOMPARE(
            entries.at(0)
                .toObject()
                .value(QStringLiteral("aliases"))
                .toArray()
                .size(),
            1
        );
    }

    void emptySecretsFixtureContainsNoValues()
    {
        const QJsonDocument document =
            loadDocument(QStringLiteral("secrets/empty_secrets.json"));
        QVERIFY(document.isObject());
        const QJsonObject root = document.object();
        for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
            if (it.value().isString()) {
                QVERIFY2(
                    it.value().toString().isEmpty(),
                    qPrintable(it.key())
                );
            }
        }
    }
};

QTEST_MAIN(CompatibilityTests)
#include "compatibility_tests.moc"
