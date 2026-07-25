#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QString mode = application.arguments().value(1);

    QFile input;
    input.open(stdin, QIODevice::ReadOnly);
    input.readLine();

    QFile output;
    output.open(stdout, QIODevice::WriteOnly);

    if (mode == QStringLiteral("timeout")) {
        QThread::msleep(1000);
        return 0;
    }

    if (mode == QStringLiteral("malformed")) {
        output.write("{not-json\n");
        output.flush();
        return 0;
    }

    QJsonObject root;
    root.insert(QStringLiteral("requestId"), QStringLiteral("request-1"));
    if (mode == QStringLiteral("failure")) {
        root.insert(QStringLiteral("ok"), false);
        root.insert(QStringLiteral("errorCode"), QStringLiteral("MODEL_MISSING"));
        root.insert(QStringLiteral("errorMessage"), QStringLiteral("model file is missing"));
    } else {
        root.insert(QStringLiteral("ok"), true);
        root.insert(QStringLiteral("text"), QString::fromUtf8("测试 ABC"));
        root.insert(QStringLiteral("elapsedMs"), 5);
    }

    output.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    output.write("\n");
    output.flush();
    return 0;
}
