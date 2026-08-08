#include "history_text.h"

#include <QDateTime>
#include <QJsonArray>
#include <QRegExp>
#include <QStringList>
#include <QtGlobal>

namespace {

QString text(const char *value)
{
    return QString::fromUtf8(value);
}

QString builtInModeTitle(const QString &modeId)
{
    if (modeId == QStringLiteral("dictate")) {
        return text("听写");
    }
    if (modeId == QStringLiteral("translate")) {
        return text("翻译");
    }
    if (modeId == QStringLiteral("ask")) {
        return text("问答");
    }
    if (modeId == QStringLiteral("ocr")) {
        return text("图片识别");
    }
    return QString();
}

QString legacyBuiltInModeId(const QString &modeTitle)
{
    const QString title = modeTitle.trimmed();
    if (title == text("听写")
        || title == text("听写（Dictate）")
        || title == QStringLiteral("听写 (Dictate)")
        || title.compare(QStringLiteral("Dictate"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("dictate");
    }
    if (title == text("翻译")
        || title == text("翻译（Translate）")
        || title == QStringLiteral("翻译 (Translate)")
        || title.compare(QStringLiteral("Translate"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("translate");
    }
    if (title == text("问答")
        || title == text("问答（Ask）")
        || title == QStringLiteral("问答 (Ask)")
        || title.compare(QStringLiteral("Ask"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("ask");
    }
    if (title == text("图片识别")) {
        return QStringLiteral("ocr");
    }
    return QString();
}

} // namespace

QString historyElapsedDurationText(qint64 elapsedMs)
{
    if (elapsedMs < 0) {
        return text("未记录");
    }
    if (elapsedMs < 1000) {
        return QString::number(elapsedMs) + text(" 毫秒");
    }
    if (elapsedMs < 60000) {
        return QString::number(
            static_cast<double>(elapsedMs) / 1000.0,
            'f',
            elapsedMs < 10000 ? 1 : 0
        ) + text(" 秒");
    }
    const qint64 minutes = elapsedMs / 60000;
    const qint64 seconds = (elapsedMs % 60000) / 1000;
    return QString::number(minutes) + text(" 分 ") + QString::number(seconds) + text(" 秒");
}

QString historyDisplayTimeText(const QString &iso)
{
    const QDateTime time = QDateTime::fromString(iso, Qt::ISODate);
    if (time.isValid()) {
        return time.toString(text("yyyy-MM-dd HH:mm:ss"));
    }
    return QString(iso).replace(QStringLiteral("T"), QStringLiteral(" "));
}

QString historyEntryModeText(const HistoryEntry &entry)
{
    const QString byId = builtInModeTitle(entry.modeId.trimmed());
    if (!byId.isEmpty()) {
        return byId;
    }
    const QString legacyId = legacyBuiltInModeId(entry.mode);
    const QString legacyTitle = builtInModeTitle(legacyId);
    return legacyTitle.isEmpty() ? entry.mode : legacyTitle;
}

QString historyEntryPreviewText(const HistoryEntry &entry, int maxLength)
{
    QString preview = entry.output.trimmed();
    if (preview.isEmpty()) {
        preview = entry.input.trimmed();
    }
    if (preview.isEmpty()) {
        preview = entry.error.trimmed();
    }
    if (preview.isEmpty()) {
        preview = text("无文本内容");
    }
    preview.replace(QRegExp(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (maxLength >= 0 && preview.size() > maxLength) {
        preview = preview.left(maxLength) + QStringLiteral("...");
    }
    return preview;
}

QString historyEntryTitleText(const HistoryEntry &entry)
{
    const QString errorMark = entry.error.trimmed().isEmpty()
        ? QString()
        : text(" · 有错误");
    const QString folder = entry.favoriteFolder.trimmed();
    const QString favoriteMark = entry.favorite
        ? (folder.isEmpty() ? text(" · 已收藏") : text(" · 已收藏：") + folder)
        : QString();
    const QString draftMark = entry.draft ? text(" · 草稿") : QString();
    return historyEntryModeText(entry)
        + text(" · ")
        + historyDisplayTimeText(entry.time)
        + favoriteMark
        + draftMark
        + errorMark;
}

QString historyEntryRecognizedText(const HistoryEntry &entry)
{
    if (entry.modeId == QStringLiteral("ocr")) {
        return entry.input.trimmed();
    }
    const QString input = entry.input.trimmed();
    const QString marker = text("语音输入：\n");
    const int index = input.indexOf(marker);
    if (index >= 0) {
        return input.mid(index + marker.size()).trimmed();
    }
    if (!entry.audio.trimmed().isEmpty()) {
        return input;
    }
    return QString();
}

QString historyEntryDetailPlainText(
    const HistoryEntry &entry,
    const QString &modelText
)
{
    const QString displayModel = modelText.trimmed().isEmpty()
        ? (entry.model.trimmed().isEmpty() ? text("未调用大模型") : entry.model.trimmed())
        : modelText;

    QStringList parts;
    parts << text("功能：") + historyEntryModeText(entry);
    parts << text("时间：") + historyDisplayTimeText(entry.time);
    parts << text("耗时：") + historyElapsedDurationText(entry.elapsedMs);
    parts << text("语音识别耗时：") + historyElapsedDurationText(entry.speechElapsedMs);
    parts << text("模型耗时：") + historyElapsedDurationText(entry.modelElapsedMs);
    if (entry.modeId == QStringLiteral("ocr")) {
        parts << text("图片识别引擎：") + (entry.ocrEngine.trimmed().isEmpty() ? text("未记录") : entry.ocrEngine);
        parts << text("图片识别语言：") + (entry.ocrLanguages.isEmpty() ? text("未记录") : entry.ocrLanguages.join(text("、")));
        parts << text("图片识别耗时：") + historyElapsedDurationText(entry.ocrElapsedMs);
        parts << text("自动回退：") + (entry.ocrUsedFallback ? text("是") : text("否"));
        parts << text("图片文件名：") + (entry.imageFileName.trimmed().isEmpty() ? text("未记录") : entry.imageFileName);
    }
    if (entry.longRecording || !entry.segments.isEmpty()) {
        int failedSegments = 0;
        QStringList segmentDetails;
        for (const RecordingSegment &segment : entry.segments) {
            if (segment.text.trimmed().isEmpty()) {
                ++failedSegments;
            }
            segmentDetails << text("第 %1 段：").arg(segment.index)
                + (segment.text.trimmed().isEmpty()
                    ? text("识别失败：") + segment.error
                    : segment.text);
        }
        parts << text("录音方式：")
            + (entry.recordingTriggerMode == QStringLiteral("hold")
                ? text("按住说话")
                : text("切换开始和结束"));
        parts << text("录音分段：")
            + text("%1 段，%2 段失败")
                .arg(entry.segments.size())
                .arg(failedSegments);
        parts << text("分段详情：\n") + segmentDetails.join(QStringLiteral("\n"));
    }
    parts << text("使用模型：") + displayModel;
    parts << text("提示词版本：") + (entry.promptVersion.trimmed().isEmpty() ? text("未记录") : entry.promptVersion);
    parts << text("状态：") + (entry.draft ? text("草稿") : text("正式记录"));
    parts << text("录音：") + (entry.audio.trimmed().isEmpty() ? text("本次没有录音") : entry.audio);
    const QString recognized = historyEntryRecognizedText(entry);
    parts << text("识别文本：\n") + (recognized.trimmed().isEmpty() ? text("无") : recognized);
    parts << text("输入内容：\n") + (entry.input.trimmed().isEmpty() ? text("无") : entry.input);
    parts << text("模型输出：\n") + (entry.output.trimmed().isEmpty() ? text("无") : entry.output);
    parts << text("错误：\n") + (entry.error.trimmed().isEmpty() ? text("无") : entry.error);
    return parts.join(QStringLiteral("\n\n"));
}

QJsonObject historyEntryExportObject(
    const HistoryEntry &entry,
    const QString &modelText,
    bool audioExists
)
{
    const QString displayModel = modelText.trimmed().isEmpty()
        ? (entry.model.trimmed().isEmpty() ? text("未调用大模型") : entry.model.trimmed())
        : modelText;

    QJsonObject item;
    item.insert(QStringLiteral("modeId"), entry.modeId);
    item.insert(QStringLiteral("mode"), entry.mode);
    item.insert(QStringLiteral("time"), entry.time);
    item.insert(QStringLiteral("timeText"), historyDisplayTimeText(entry.time));
    item.insert(QStringLiteral("elapsedMs"), static_cast<double>(entry.elapsedMs));
    item.insert(QStringLiteral("elapsedText"), historyElapsedDurationText(entry.elapsedMs));
    item.insert(QStringLiteral("speechElapsedMs"), static_cast<double>(entry.speechElapsedMs));
    item.insert(QStringLiteral("speechElapsedText"), historyElapsedDurationText(entry.speechElapsedMs));
    item.insert(QStringLiteral("modelElapsedMs"), static_cast<double>(entry.modelElapsedMs));
    item.insert(QStringLiteral("modelElapsedText"), historyElapsedDurationText(entry.modelElapsedMs));
    item.insert(QStringLiteral("ocrEngine"), entry.ocrEngine);
    item.insert(QStringLiteral("ocrLanguages"), QJsonArray::fromStringList(entry.ocrLanguages));
    item.insert(QStringLiteral("ocrElapsedMs"), static_cast<double>(entry.ocrElapsedMs));
    item.insert(QStringLiteral("ocrElapsedText"), historyElapsedDurationText(entry.ocrElapsedMs));
    item.insert(QStringLiteral("ocrUsedFallback"), entry.ocrUsedFallback);
    item.insert(QStringLiteral("imageFileName"), entry.imageFileName);
    item.insert(QStringLiteral("promptVersion"), entry.promptVersion);
    item.insert(QStringLiteral("model"), entry.model);
    item.insert(QStringLiteral("modelText"), displayModel);
    item.insert(QStringLiteral("recognizedText"), historyEntryRecognizedText(entry));
    item.insert(QStringLiteral("input"), entry.input);
    item.insert(QStringLiteral("output"), entry.output);
    item.insert(QStringLiteral("error"), entry.error);
    item.insert(QStringLiteral("audio"), entry.audio);
    item.insert(QStringLiteral("textFile"), entry.textFile);
    item.insert(QStringLiteral("allAudioFile"), entry.allAudioFile);
    item.insert(QStringLiteral("allTextFile"), entry.allTextFile);
    item.insert(QStringLiteral("allDetailFile"), entry.allDetailFile);
    item.insert(QStringLiteral("audioExists"), audioExists);
    item.insert(QStringLiteral("favorite"), entry.favorite);
    item.insert(QStringLiteral("favoriteFolder"), entry.favoriteFolder);
    item.insert(QStringLiteral("draft"), entry.draft);
    item.insert(QStringLiteral("sourceFile"), entry.filePath);
    return item;
}

bool historyEntryMatchesSearchText(
    const HistoryEntry &entry,
    const QString &keyword
)
{
    const QString trimmed = keyword.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }
    const QString searchable = (QStringList()
        << entry.mode
        << historyEntryModeText(entry)
        << historyDisplayTimeText(entry.time)
        << entry.input
        << entry.output
        << entry.error
        << entry.model
        << entry.ocrEngine
        << entry.ocrLanguages
        << entry.imageFileName
        << entry.favoriteFolder
        << entry.filePath
        << entry.audio
        << entry.textFile
        << entry.allAudioFile
        << entry.allTextFile
        << entry.allDetailFile)
        .join(QStringLiteral("\n"));
    return searchable.contains(trimmed, Qt::CaseInsensitive);
}

int historyTextDisplayUnits(const QString &value)
{
    int units = 0;
    for (int i = 0; i < value.size(); ++i) {
        units += value.at(i).unicode() < 0x80 ? 1 : 2;
    }
    return units;
}

int historyEntryRowHeight(const HistoryEntry &entry, int viewportWidth)
{
    const int titleWidth = qMax(240, viewportWidth - 180);
    const int previewWidth = qMax(280, viewportWidth - 90);
    const int titleUnitsPerLine = qBound(34, titleWidth / 7, 94);
    const int previewUnitsPerLine = qBound(42, previewWidth / 7, 110);
    const int titleLines = qBound(
        1,
        (historyTextDisplayUnits(historyEntryTitleText(entry)) + titleUnitsPerLine - 1)
            / titleUnitsPerLine,
        2
    );
    const int previewLines = qBound(
        3,
        (historyTextDisplayUnits(historyEntryPreviewText(entry)) + previewUnitsPerLine - 1)
            / previewUnitsPerLine
            + 1,
        7
    );
    return qBound(150, 46 + titleLines * 24 + previewLines * 24, 300);
}
