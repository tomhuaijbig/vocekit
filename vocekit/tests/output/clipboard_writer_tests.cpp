#include <QtTest>

#include "../../src/output/clipboard_writer.h"

#include <QApplication>
#include <QClipboard>
#include <QFuture>
#include <QtConcurrent>

class ClipboardWriterTests : public QObject
{
    Q_OBJECT

private slots:
    void persistentCopyWritesTextWithoutPasteOrRestoreLease()
    {
        QApplication::clipboard()->setText(QStringLiteral("before"));

        QVERIFY(ClipboardWriter::copyText(QStringLiteral("persistent")));
        QCOMPARE(
            QApplication::clipboard()->text(),
            QStringLiteral("persistent")
        );
        QVERIFY(!QApplication::clipboard()->mimeData()->hasFormat(
            QStringLiteral("application/x-vocekit-clipboard-lease")
        ));

        QTest::qWait(650);
        QCOMPARE(
            QApplication::clipboard()->text(),
            QStringLiteral("persistent")
        );
    }

    void persistentCopyRejectsWorkerThreadWithoutChangingClipboard()
    {
        QApplication::clipboard()->setText(QStringLiteral("owner-value"));
        QFuture<bool> copied = QtConcurrent::run([]() {
            return ClipboardWriter::copyText(QStringLiteral("worker-value"));
        });
        copied.waitForFinished();

        QVERIFY(!copied.result());
        QCOMPARE(
            QApplication::clipboard()->text(),
            QStringLiteral("owner-value")
        );
    }
};

QTEST_MAIN(ClipboardWriterTests)
#include "clipboard_writer_tests.moc"
