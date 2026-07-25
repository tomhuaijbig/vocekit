#ifndef VOCEKIT_OCR_TYPES_H
#define VOCEKIT_OCR_TYPES_H

#include <QFileInfo>
#include <QImageReader>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

enum class OcrEngine {
    Automatic,
    RapidOcr,
    WindowsOcr,
    CustomCloud,
    VisionModel
};

struct OcrRequest
{
    QString requestId;
    QString imagePath;
    QStringList languages;
    OcrEngine engine = OcrEngine::Automatic;
};

struct OcrTextBlock
{
    QString text;
    QVector<QPoint> points;
    double confidence = -1.0;

    QRect boundingRect() const
    {
        if (points.isEmpty()) {
            return QRect();
        }
        int left = points.first().x();
        int right = left;
        int top = points.first().y();
        int bottom = top;
        for (const QPoint &point : points) {
            left = qMin(left, point.x());
            right = qMax(right, point.x());
            top = qMin(top, point.y());
            bottom = qMax(bottom, point.y());
        }
        return QRect(QPoint(left, top), QPoint(right, bottom));
    }
};

struct OcrResult
{
    bool ok = false;
    OcrEngine engine = OcrEngine::Automatic;
    QString text;
    QString errorCode;
    QString errorMessage;
    qint64 elapsedMs = -1;
    bool usedFallback = false;
    QSize imageSize;
    QVector<OcrTextBlock> blocks;
};

inline bool validateOcrImage(const QString &path, QString *error)
{
    if (error) {
        error->clear();
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (error) {
            *error = QStringLiteral("图片文件不存在或无法访问。");
        }
        return false;
    }

    const qint64 maximumBytes = 200LL * 1024LL * 1024LL;
    if (fileInfo.size() > maximumBytes) {
        if (error) {
            *error = QStringLiteral("单张图片不能超过 200 MB。");
        }
        return false;
    }

    QImageReader reader(path);
    if (!reader.canRead()) {
        if (error) {
            *error = QStringLiteral("无法读取图片，文件可能已损坏或格式不受支持。");
        }
        return false;
    }

    const QSize imageSize = reader.size();
    if (imageSize.isValid()) {
        const qint64 pixelCount = qint64(imageSize.width()) * qint64(imageSize.height());
        if (imageSize.width() > 50000 || imageSize.height() > 50000) {
            if (error) {
                *error = QStringLiteral("图片单边不能超过 50000 像素。");
            }
            return false;
        }
        if (pixelCount > 120000000LL) {
            if (error) {
                *error = QStringLiteral("单张图片不能超过 1.2 亿像素。");
            }
            return false;
        }
    }

    return true;
}

#endif // VOCEKIT_OCR_TYPES_H
