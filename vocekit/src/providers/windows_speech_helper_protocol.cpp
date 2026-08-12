#include "windows_speech_helper_protocol.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTextCodec>

namespace {

const int MaximumProtocolLineBytes = 64 * 1024;

bool decodeStrictUtf8(const QByteArray &bytes, QString *decoded)
{
    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    if (!codec) {
        return false;
    }

    QTextCodec::ConverterState state;
    const QString text = codec->toUnicode(
        bytes.constData(), bytes.size(), &state
    );
    if (state.invalidChars != 0 || state.remainingChars != 0) {
        return false;
    }
    if (decoded) {
        *decoded = text;
    }
    return true;
}

bool isPresentString(
    const QJsonObject &object,
    const QString &name,
    bool requireNonBlank = false)
{
    const QJsonValue value = object.value(name);
    return value.isString()
        && (!requireNonBlank || !value.toString().trimmed().isEmpty());
}

bool parseInstalledLanguages(
    const QJsonObject &object,
    QStringList *languages)
{
    const QJsonValue value = object.value(
        QStringLiteral("installedLanguages")
    );
    if (!value.isArray()) {
        return false;
    }

    QStringList parsed;
    const QJsonArray items = value.toArray();
    for (const QJsonValue &item : items) {
        if (!item.isString()) {
            return false;
        }
        parsed.append(item.toString());
    }
    if (languages) {
        *languages = parsed;
    }
    return true;
}

bool parseOptionalBoolean(
    const QJsonObject &object,
    const QString &name,
    bool *parsed)
{
    if (!object.contains(name)) {
        return true;
    }
    const QJsonValue value = object.value(name);
    if (!value.isBool()) {
        return false;
    }
    if (parsed) {
        *parsed = value.toBool();
    }
    return true;
}

bool parseOptionalPcmBytes(
    const QJsonObject &object,
    qint64 *parsed)
{
    QString field;
    if (object.contains(QStringLiteral("pcmBytes"))) {
        field = QStringLiteral("pcmBytes");
    } else if (object.contains(QStringLiteral("pcmBytesObserved"))) {
        field = QStringLiteral("pcmBytesObserved");
    } else {
        return true;
    }

    const QJsonValue value = object.value(field);
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    const qint64 integer = static_cast<qint64>(number);
    if (number < 0.0 || number != static_cast<double>(integer)) {
        return false;
    }
    if (parsed) {
        *parsed = integer;
    }
    return true;
}

WindowsSpeechHelperEventType eventTypeForName(const QString &name)
{
    if (name == QStringLiteral("ready")) {
        return WindowsSpeechHelperEventType::Ready;
    }
    if (name == QStringLiteral("hypothesis")) {
        return WindowsSpeechHelperEventType::Hypothesis;
    }
    if (name == QStringLiteral("recognized")) {
        return WindowsSpeechHelperEventType::Recognized;
    }
    if (name == QStringLiteral("final")) {
        return WindowsSpeechHelperEventType::Final;
    }
    if (name == QStringLiteral("probe")) {
        return WindowsSpeechHelperEventType::Probe;
    }
    if (name == QStringLiteral("error")) {
        return WindowsSpeechHelperEventType::Error;
    }
    if (name == QStringLiteral("self-test")) {
        return WindowsSpeechHelperEventType::SelfTest;
    }
    return WindowsSpeechHelperEventType::Invalid;
}

bool isLatinLetterOrDigit(const QChar character)
{
    return character.isDigit()
        || (character.isLetter()
            && character.script() == QChar::Script_Latin);
}

} // namespace

QString windowsSpeechHelperPathForApplicationDir(
    const QString &applicationDir)
{
    return QDir::cleanPath(
        QDir(applicationDir).filePath(
            QStringLiteral(
                "speech/windows/vocekit-windows-speech.exe"
            )
        )
    );
}

QStringList windowsSpeechHelperArguments(
    const QString &mode,
    const QString &runId,
    const QString &language,
    int sampleRate,
    int channelCount,
    int bits)
{
    return QStringList()
        << QStringLiteral("--mode") << mode
        << QStringLiteral("--run-id") << runId
        << QStringLiteral("--language") << language
        << QStringLiteral("--sample-rate") << QString::number(sampleRate)
        << QStringLiteral("--channels") << QString::number(channelCount)
        << QStringLiteral("--bits") << QString::number(bits);
}

WindowsSpeechHelperEvent parseWindowsSpeechHelperEvent(
    const QByteArray &line)
{
    WindowsSpeechHelperEvent event;
    if (line.isEmpty()
        || line.size() > MaximumProtocolLineBytes
        || line.contains('\r')
        || line.contains('\n')) {
        return event;
    }

    QString decoded;
    if (!decodeStrictUtf8(line, &decoded)) {
        return event;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        decoded.toUtf8(), &parseError
    );
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        return event;
    }

    const QJsonObject object = document.object();
    const QJsonValue version = object.value(
        QStringLiteral("protocolVersion")
    );
    if (!version.isDouble() || version.toDouble() != 1.0
        || !isPresentString(object, QStringLiteral("runId"), true)
        || !isPresentString(object, QStringLiteral("type"), true)) {
        return event;
    }

    event.type = eventTypeForName(
        object.value(QStringLiteral("type")).toString()
    );
    if (event.type == WindowsSpeechHelperEventType::Invalid) {
        return WindowsSpeechHelperEvent();
    }

    if (!parseOptionalBoolean(
            object,
            QStringLiteral("inputStreamEnded"),
            &event.inputStreamEnded
        )
        || !parseOptionalPcmBytes(object, &event.pcmBytesObserved)) {
        return WindowsSpeechHelperEvent();
    }

    event.runId = object.value(QStringLiteral("runId")).toString();
    switch (event.type) {
    case WindowsSpeechHelperEventType::Hypothesis:
    case WindowsSpeechHelperEventType::Recognized:
    case WindowsSpeechHelperEventType::Final:
        if (!isPresentString(object, QStringLiteral("text"))) {
            return WindowsSpeechHelperEvent();
        }
        event.text = object.value(QStringLiteral("text")).toString();
        break;
    case WindowsSpeechHelperEventType::Error:
        if (!isPresentString(
                object, QStringLiteral("errorCode"), true
            )
            || !isPresentString(
                object, QStringLiteral("message"), true
            )) {
            return WindowsSpeechHelperEvent();
        }
        event.errorCode = object.value(
            QStringLiteral("errorCode")
        ).toString();
        event.errorMessage = object.value(
            QStringLiteral("message")
        ).toString();
        break;
    case WindowsSpeechHelperEventType::Probe:
        if (!isPresentString(
                object, QStringLiteral("resolvedLanguage"), true
            )
            || !parseInstalledLanguages(
                object, &event.installedLanguages
            )) {
            return WindowsSpeechHelperEvent();
        }
        event.resolvedLanguage = object.value(
            QStringLiteral("resolvedLanguage")
        ).toString();
        break;
    case WindowsSpeechHelperEventType::Ready:
        if (object.contains(QStringLiteral("resolvedLanguage"))) {
            if (!isPresentString(
                    object, QStringLiteral("resolvedLanguage"), true
                )) {
                return WindowsSpeechHelperEvent();
            }
            event.resolvedLanguage = object.value(
                QStringLiteral("resolvedLanguage")
            ).toString();
        }
        break;
    case WindowsSpeechHelperEventType::SelfTest:
    case WindowsSpeechHelperEventType::Invalid:
        break;
    }

    event.valid = true;
    return event;
}

QString windowsSpeechOperationErrorCode(const QString &helperErrorCode)
{
    if (helperErrorCode == QStringLiteral("PROGRAM_MISSING")) {
        return QStringLiteral("speech.windows.program_missing");
    }
    if (helperErrorCode == QStringLiteral("RECOGNIZER_MISSING")) {
        return QStringLiteral("speech.windows.recognizer_missing");
    }
    if (helperErrorCode == QStringLiteral("SYSTEM_SPEECH_UNAVAILABLE")) {
        return QStringLiteral("speech.windows.runtime_missing");
    }
    if (helperErrorCode == QStringLiteral("GRAMMAR_LOAD_FAILED")) {
        return QStringLiteral("speech.windows.grammar_load_failed");
    }
    if (helperErrorCode == QStringLiteral("NO_SPEECH")) {
        return QStringLiteral("speech.empty_result");
    }
    if (helperErrorCode == QStringLiteral("CANCELLED")) {
        return QStringLiteral("operation.cancelled");
    }
    return QStringLiteral("speech.windows.local");
}

bool isWindowsSpeechConfigurationErrorCode(
    const QString &operationErrorCode)
{
    return operationErrorCode == QStringLiteral(
               "speech.windows.program_missing"
           )
        || operationErrorCode == QStringLiteral(
               "speech.windows.recognizer_missing"
           )
        || operationErrorCode == QStringLiteral(
               "speech.windows.runtime_missing"
           )
        || operationErrorCode == QStringLiteral(
               "speech.windows.grammar_load_failed"
           );
}

QString appendWindowsSpeechRecognizedSegment(
    const QString &committed,
    const QString &segment)
{
    if (committed.isEmpty()) {
        return segment;
    }
    if (segment.isEmpty()) {
        return committed;
    }
    if (committed.at(committed.size() - 1).isSpace()
        || segment.at(0).isSpace()
        || !isLatinLetterOrDigit(committed.at(committed.size() - 1))
        || !isLatinLetterOrDigit(segment.at(0))) {
        return committed + segment;
    }
    return committed + QLatin1Char(' ') + segment;
}
