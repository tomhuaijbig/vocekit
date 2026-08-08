#include "../../src/domain/history_text.h"

#include <QtTest>

#include <QJsonObject>

namespace {

HistoryEntry sampleEntry()
{
    HistoryEntry entry;
    entry.mode = QStringLiteral("听写");
    entry.time = QStringLiteral("2026-07-03T12:34:56");
    entry.input = QStringLiteral("原始输入");
    entry.output = QStringLiteral("整理后的输出");
    entry.model = QStringLiteral("deepseek-v4-flash");
    return entry;
}

} // namespace

class HistoryTextTests : public QObject
{
    Q_OBJECT

private slots:
    void formatsIsoTime();
    void formatsElapsedDurations();
    void previewPrefersOutputThenInputThenError();
    void legacyBuiltInTitlesDisplayInChineseWithoutChangingStoredValue();
    void titleIncludesFavoriteDraftAndError();
    void extractsRecognizedText();
    void buildsDetailPlainText();
    void buildsExportObject();
    void searchMatchesAcrossImportantFields();
    void rowHeightStaysWithinExpectedBounds();
};

void HistoryTextTests::formatsIsoTime()
{
    QCOMPARE(
        historyDisplayTimeText(QStringLiteral("2026-07-03T12:34:56")),
        QStringLiteral("2026-07-03 12:34:56")
    );
    QCOMPARE(
        historyDisplayTimeText(QStringLiteral("2026-07-03 12:34:56")),
        QStringLiteral("2026-07-03 12:34:56")
    );
}

void HistoryTextTests::formatsElapsedDurations()
{
    QCOMPARE(historyElapsedDurationText(-1), QStringLiteral("未记录"));
    QCOMPARE(historyElapsedDurationText(800), QStringLiteral("800 毫秒"));
    QCOMPARE(historyElapsedDurationText(1500), QStringLiteral("1.5 秒"));
    QCOMPARE(historyElapsedDurationText(65000), QStringLiteral("1 分 5 秒"));
}

void HistoryTextTests::previewPrefersOutputThenInputThenError()
{
    HistoryEntry entry = sampleEntry();
    QCOMPARE(historyEntryPreviewText(entry), QStringLiteral("整理后的输出"));

    entry.output.clear();
    QCOMPARE(historyEntryPreviewText(entry), QStringLiteral("原始输入"));

    entry.input.clear();
    entry.error = QStringLiteral("网络错误");
    QCOMPARE(historyEntryPreviewText(entry), QStringLiteral("网络错误"));

    entry.error.clear();
    QCOMPARE(historyEntryPreviewText(entry), QStringLiteral("无文本内容"));
}

void HistoryTextTests::
legacyBuiltInTitlesDisplayInChineseWithoutChangingStoredValue()
{
    HistoryEntry entry = sampleEntry();
    entry.modeId = QStringLiteral("dictate");
    entry.mode = QString::fromUtf8("听写（Dictate）");
    const QString storedMode = entry.mode;

    QVERIFY(!historyEntryTitleText(entry).contains(
        QStringLiteral("Dictate")
    ));
    QVERIFY(historyEntryDetailPlainText(entry).contains(
        QString::fromUtf8("功能：听写")
    ));
    QCOMPARE(entry.mode, storedMode);
}

void HistoryTextTests::titleIncludesFavoriteDraftAndError()
{
    HistoryEntry entry = sampleEntry();
    entry.favorite = true;
    entry.favoriteFolder = QStringLiteral("工作");
    entry.draft = true;
    entry.error = QStringLiteral("失败");

    const QString title = historyEntryTitleText(entry);
    QVERIFY(title.contains(QStringLiteral("听写")));
    QVERIFY(title.contains(QStringLiteral("2026-07-03 12:34:56")));
    QVERIFY(title.contains(QStringLiteral("已收藏：工作")));
    QVERIFY(title.contains(QStringLiteral("草稿")));
    QVERIFY(title.contains(QStringLiteral("有错误")));
}

void HistoryTextTests::extractsRecognizedText()
{
    HistoryEntry entry = sampleEntry();
    entry.input = QStringLiteral("前置内容\n语音输入：\n真正识别内容");
    QCOMPARE(historyEntryRecognizedText(entry), QStringLiteral("真正识别内容"));

    entry.input = QStringLiteral("没有标记");
    entry.audio = QStringLiteral("record.wav");
    QCOMPARE(historyEntryRecognizedText(entry), QStringLiteral("没有标记"));

    entry.modeId = QStringLiteral("ocr");
    entry.input = QStringLiteral("图片文字");
    QCOMPARE(historyEntryRecognizedText(entry), QStringLiteral("图片文字"));
}

void HistoryTextTests::buildsDetailPlainText()
{
    HistoryEntry entry = sampleEntry();
    entry.elapsedMs = 1200;
    entry.speechElapsedMs = 500;
    entry.modelElapsedMs = 700;
    entry.audio = QStringLiteral("record.wav");
    entry.input = QStringLiteral("语音输入：\nhello");
    entry.output = QStringLiteral("你好");

    const QString detail = historyEntryDetailPlainText(
        entry,
        QStringLiteral("DeepSeek 快速模型")
    );

    QVERIFY(detail.contains(QStringLiteral("功能：")));
    QVERIFY(detail.contains(QStringLiteral("DeepSeek 快速模型")));
    QVERIFY(detail.contains(QStringLiteral("hello")));
    QVERIFY(detail.contains(QStringLiteral("你好")));
}

void HistoryTextTests::buildsExportObject()
{
    HistoryEntry entry = sampleEntry();
    entry.audio = QStringLiteral("record.wav");
    entry.favorite = true;

    const QJsonObject object = historyEntryExportObject(
        entry,
        QStringLiteral("DeepSeek 快速模型"),
        true
    );

    QCOMPARE(object.value(QStringLiteral("mode")).toString(), entry.mode);
    QCOMPARE(object.value(QStringLiteral("modelText")).toString(), QStringLiteral("DeepSeek 快速模型"));
    QCOMPARE(object.value(QStringLiteral("recognizedText")).toString(), entry.input);
    QVERIFY(object.value(QStringLiteral("audioExists")).toBool());
    QVERIFY(object.value(QStringLiteral("favorite")).toBool());
}

void HistoryTextTests::searchMatchesAcrossImportantFields()
{
    HistoryEntry entry = sampleEntry();
    entry.favoriteFolder = QStringLiteral("重要收藏");
    entry.filePath = QStringLiteral("records/detail.json");

    QVERIFY(historyEntryMatchesSearchText(entry, QStringLiteral("deepseek")));
    QVERIFY(historyEntryMatchesSearchText(entry, QStringLiteral("重要收藏")));
    QVERIFY(historyEntryMatchesSearchText(entry, QStringLiteral("detail.json")));
    QVERIFY(!historyEntryMatchesSearchText(entry, QStringLiteral("完全不存在")));
}

void HistoryTextTests::rowHeightStaysWithinExpectedBounds()
{
    HistoryEntry entry = sampleEntry();
    entry.output = QString(220, QLatin1Char('A'));
    const int height = historyEntryRowHeight(entry, 520);
    QVERIFY(height >= 150);
    QVERIFY(height <= 300);
    QCOMPARE(historyTextDisplayUnits(QStringLiteral("ab中文")), 6);
}

QTEST_MAIN(HistoryTextTests)

#include "history_text_tests.moc"
