#include <QtTest>

#include "../../src/app/application_events.h"

class ApplicationEventsTests : public QObject
{
    Q_OBJECT

private slots:
    void publishesEachChangeOnce()
    {
        ApplicationEvents events;
        QSignalSpy historySpy(
            &events,
            SIGNAL(historyChanged(HistoryChangeSet))
        );
        QSignalSpy settingsSpy(
            &events,
            SIGNAL(settingsChanged(SettingsChangeSet))
        );
        QSignalSpy vocabularySpy(
            &events,
            SIGNAL(vocabularyChanged(VocabularyChangeSet))
        );

        HistoryChangeSet history;
        history.recordIds << QStringLiteral("record-1");
        events.publishHistoryChanged(history);

        SettingsChangeSet settings;
        settings.keys << QStringLiteral("speechProvider");
        settings.functionIds << QStringLiteral("dictate");
        events.publishSettingsChanged(settings);

        VocabularyChangeSet vocabulary;
        vocabulary.entryIds << QStringLiteral("entry-1");
        vocabulary.resetRequired = true;
        events.publishVocabularyChanged(vocabulary);

        QCOMPARE(historySpy.count(), 1);
        QCOMPARE(settingsSpy.count(), 1);
        QCOMPARE(vocabularySpy.count(), 1);

        const HistoryChangeSet emittedHistory =
            qvariant_cast<HistoryChangeSet>(
                historySpy.takeFirst().at(0)
            );
        QCOMPARE(
            emittedHistory.recordIds,
            QStringList() << QStringLiteral("record-1")
        );
        const VocabularyChangeSet emittedVocabulary =
            qvariant_cast<VocabularyChangeSet>(
                vocabularySpy.takeFirst().at(0)
            );
        QVERIFY(emittedVocabulary.resetRequired);
    }
};

QTEST_MAIN(ApplicationEventsTests)
#include "application_events_tests.moc"
