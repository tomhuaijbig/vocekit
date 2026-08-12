#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>

#include <cstdlib>

namespace {

QString argumentValue(
    const QStringList &arguments,
    const QString &name,
    const QString &fallback = QString())
{
    const int index = arguments.indexOf(name);
    if (index < 0 || index + 1 >= arguments.size()) {
        return fallback;
    }
    return arguments.at(index + 1);
}

void writeBytes(const QByteArray &bytes)
{
    QFile output;
    output.open(stdout, QIODevice::WriteOnly);
    output.write(bytes);
    output.flush();
}

void writeErrorBytes(const QByteArray &bytes)
{
    QFile output;
    output.open(stderr, QIODevice::WriteOnly);
    output.write(bytes);
    output.flush();
}

QByteArray eventBytes(
    const QString &type,
    const QString &runId,
    const QJsonObject &fields = QJsonObject())
{
    QJsonObject event = fields;
    event.insert(QStringLiteral("protocolVersion"), 1);
    event.insert(QStringLiteral("runId"), runId);
    event.insert(QStringLiteral("type"), type);
    return QJsonDocument(event).toJson(QJsonDocument::Compact) + '\n';
}

void writeEvent(
    const QString &type,
    const QString &runId,
    const QJsonObject &fields = QJsonObject())
{
    writeBytes(eventBytes(type, runId, fields));
}

QByteArray readStandardInput()
{
    QFile input;
    input.open(stdin, QIODevice::ReadOnly);
    return input.readAll();
}

void writePidFile(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QByteArray::number(QCoreApplication::applicationPid()));
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments().mid(1);
    const QString scenario = argumentValue(
        arguments, QStringLiteral("--scenario")
    );
    const QString runId = argumentValue(
        arguments, QStringLiteral("--run-id")
    );
    if (scenario.isEmpty() || runId.trimmed().isEmpty()) {
        QTextStream(stderr) << "--scenario and --run-id are required\n";
        return 64;
    }

    writePidFile(argumentValue(arguments, QStringLiteral("--pid-file")));

    if (scenario == QStringLiteral("invalid-json")) {
        writeEvent(QStringLiteral("ready"), runId);
        writeBytes(QByteArray("{not-json}\n"));
        return 0;
    }
    if (scenario == QStringLiteral("wrong-run-id")) {
        writeEvent(QStringLiteral("ready"), runId + QStringLiteral("-wrong"));
        return 0;
    }
    if (scenario == QStringLiteral("crash")) {
        std::abort();
    }
    if (scenario == QStringLiteral("timeout")) {
        QThread::msleep(5000);
        return 0;
    }
    if (scenario == QStringLiteral("startup-error")) {
        const QString errorCode = argumentValue(
            arguments,
            QStringLiteral("--error-code"),
            QStringLiteral("RECOGNIZER_MISSING")
        );
        QJsonObject fields;
        fields.insert(QStringLiteral("ok"), false);
        fields.insert(QStringLiteral("errorCode"), errorCode);
        fields.insert(
            QStringLiteral("message"),
            QStringLiteral("Recognizer missing.")
        );
        writeEvent(QStringLiteral("error"), runId, fields);
        return 4;
    }
    if (scenario == QStringLiteral("startup-invalid-json")) {
        writeBytes(QByteArray("{not-json}\n"));
        return 0;
    }
    if (scenario == QStringLiteral("oversize-output")) {
        writeBytes(QByteArray(1024 * 1024 + 1, 'x'));
        QThread::msleep(1000);
        return 0;
    }
    if (scenario == QStringLiteral("oversize-stderr")) {
        writeErrorBytes(QByteArray(1024 * 1024 + 1, 'e'));
        QThread::msleep(1000);
        return 0;
    }
    if (scenario == QStringLiteral("probe")) {
        QJsonObject fields;
        fields.insert(QStringLiteral("ok"), true);
        fields.insert(QStringLiteral("resolvedLanguage"), QStringLiteral("zh-CN"));
        fields.insert(
            QStringLiteral("installedLanguages"),
            QJsonArray()
                << QStringLiteral("zh-CN")
                << QStringLiteral("en-US")
        );
        writeEvent(QStringLiteral("probe"), runId, fields);
        return 0;
    }
    if (scenario == QStringLiteral("error")) {
        QJsonObject fields;
        fields.insert(QStringLiteral("ok"), false);
        fields.insert(
            QStringLiteral("errorCode"),
            QStringLiteral("RECOGNIZER_MISSING")
        );
        fields.insert(
            QStringLiteral("message"),
            QStringLiteral("Recognizer missing.")
        );
        fields.insert(QStringLiteral("inputStreamEnded"), false);
        writeEvent(QStringLiteral("error"), runId, fields);
        return 4;
    }
    if (scenario == QStringLiteral("no-ready")) {
        QJsonObject fields;
        fields.insert(QStringLiteral("ok"), true);
        fields.insert(QStringLiteral("text"), QStringLiteral("unexpected"));
        fields.insert(QStringLiteral("pcmBytes"), 0);
        writeEvent(QStringLiteral("final"), runId, fields);
        return 0;
    }

    const QByteArray ready = eventBytes(
        QStringLiteral("ready"),
        runId,
        QJsonObject{{QStringLiteral("ok"), true},
                    {QStringLiteral("resolvedLanguage"),
                     QStringLiteral("zh-CN")}}
    );
    if (scenario == QStringLiteral("delayed-ready")) {
        QThread::msleep(75);
    }
    if (scenario == QStringLiteral("split-lines")) {
        writeBytes(ready.left(ready.size() / 2));
        QThread::msleep(20);
        writeBytes(ready.mid(ready.size() / 2));
    } else {
        writeBytes(ready);
    }

    if (scenario == QStringLiteral("wrong-after-ready")) {
        writeEvent(
            QStringLiteral("hypothesis"),
            runId + QStringLiteral("-wrong"),
            QJsonObject{{QStringLiteral("ok"), true},
                        {QStringLiteral("text"), QStringLiteral("wrong")}}
        );
        return 0;
    }
    if (scenario == QStringLiteral("ready-crash")) {
        QThread::msleep(75);
        std::abort();
    }
    if (scenario == QStringLiteral("hypothesis-replacement")) {
        writeEvent(
            QStringLiteral("hypothesis"),
            runId,
            QJsonObject{{QStringLiteral("ok"), true},
                        {QStringLiteral("text"), QString::fromUtf8("你")}}
        );
        writeEvent(
            QStringLiteral("hypothesis"),
            runId,
            QJsonObject{{QStringLiteral("ok"), true},
                        {QStringLiteral("text"), QString::fromUtf8("你好")}}
        );
        writeEvent(
            QStringLiteral("recognized"),
            runId,
            QJsonObject{{QStringLiteral("ok"), true},
                        {QStringLiteral("text"), QString::fromUtf8("你好")}}
        );
    }

    if (scenario == QStringLiteral("slow-read")
        || scenario == QStringLiteral("backpressure")) {
        QThread::msleep(5000);
    }

    const QByteArray pcm = readStandardInput();
    if (scenario == QStringLiteral("final-timeout")) {
        QThread::msleep(5000);
        return 0;
    }
    if (scenario == QStringLiteral("no-final")) {
        return 0;
    }

    QJsonObject finalFields;
    finalFields.insert(QStringLiteral("ok"), true);
    finalFields.insert(QStringLiteral("inputStreamEnded"), true);
    finalFields.insert(QStringLiteral("pcmBytes"), pcm.size());
    finalFields.insert(
        QStringLiteral("text"),
        scenario == QStringLiteral("empty-final")
            ? QString()
            : scenario == QStringLiteral("partial-write")
                ? QStringLiteral("bytes:") + QString::number(pcm.size())
                : scenario == QStringLiteral("hypothesis-replacement")
                    ? QString::fromUtf8(" 你好 世界 ")
                    : scenario == QStringLiteral("echo-language")
                        ? argumentValue(
                            arguments,
                            QStringLiteral("--language"),
                            QStringLiteral("missing")
                        )
                    : QString::fromUtf8("你好")
    );

    if (scenario == QStringLiteral("ready-final")
        || scenario == QStringLiteral("echo-pcm-size")) {
        writeEvent(
            QStringLiteral("hypothesis"),
            runId,
            QJsonObject{{QStringLiteral("ok"), true},
                        {QStringLiteral("text"),
                         QString::fromUtf8("你")}}
        );
        writeEvent(
            QStringLiteral("recognized"),
            runId,
            QJsonObject{{QStringLiteral("ok"), true},
                        {QStringLiteral("text"),
                         QString::fromUtf8("你好")}}
        );
    }

    const QByteArray final = eventBytes(
        QStringLiteral("final"), runId, finalFields
    );
    if (scenario == QStringLiteral("split-lines")) {
        writeBytes(final.left(final.size() / 2));
        QThread::msleep(20);
        writeBytes(final.mid(final.size() / 2));
    } else {
        writeBytes(final);
    }
    if (scenario == QStringLiteral("duplicate-final")) {
        writeBytes(final);
    }
    return 0;
}
