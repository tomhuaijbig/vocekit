#ifndef VOCEKIT_AUDIO_RECORDER_LEGACY_H
#define VOCEKIT_AUDIO_RECORDER_LEGACY_H

#include <functional>

// 音频采集设备：边写入 PCM 数据边计算峰值，用来驱动浮动条里的声音波形。
class AudioCaptureDevice : public QIODevice
{
public:
    explicit AudioCaptureDevice(QFile *file, QObject *parent = nullptr)
        : QIODevice(parent), m_file(file)
    {
    }

    bool open(OpenMode mode) override
    {
        return QIODevice::open(mode);
    }

    int takePeakLevel()
    {
        const int value = m_peak;
        m_peak = 0;
        return value;
    }

    void setPcmListener(
        const std::function<void(const QByteArray &)> &listener
    )
    {
        m_pcmListener = listener;
    }

protected:
    qint64 readData(char *, qint64) override
    {
        return -1;
    }

    qint64 writeData(const char *data, qint64 len) override
    {
        if (!m_file || !m_file->isOpen()) {
            return -1;
        }
        const qint64 written = m_file->write(data, len);
        if (written <= 0) {
            return written;
        }
        const QByteArray accepted(data, static_cast<int>(written));
        m_peak = qMax(m_peak, pcm16PeakLevel(accepted));
        const std::function<void(const QByteArray &)> listener =
            m_pcmListener;
        if (listener) {
            listener(accepted);
        }
        return written;
    }

private:
    QFile *m_file = nullptr;
    int m_peak = 0;
    std::function<void(const QByteArray &)> m_pcmListener;
};

// 录音器：负责启动麦克风、保存 PCM/WAV 文件，并把音频交给语音识别接口。
class AudioRecorder
{
public:
    AudioRecorder() {}
    ~AudioRecorder() { stop(); }

    bool isRecording() const { return m_audioInput != nullptr; }
    QString lastWavPath() const { return m_lastWavPath; }

    bool start(const QString &modeName, const QString &recordDirectory, QString *error)
    {
        return startInternal(modeName, recordDirectory, false, error);
    }

    bool startInDirectory(const QString &modeName, const QString &targetDirectory, QString *error)
    {
        return startInternal(modeName, targetDirectory, true, error);
    }

private:
    bool startInternal(
        const QString &modeName,
        const QString &recordDirectory,
        bool useDirectDirectory,
        QString *error)
    {
        if (m_audioInput) {
            return true;
        }

        QAudioFormat format;
        format.setSampleRate(16000);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec(QStringLiteral("audio/pcm"));
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
        if (!info.isFormatSupported(format)) {
            format = info.nearestFormat(format);
            if (format.sampleRate() != 16000 || format.channelCount() != 1 || format.sampleSize() != 16) {
                if (error) {
                    *error = tr8("当前麦克风不支持 16k 单声道录音。");
                }
                return false;
            }
        }

        const QString date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        QDir dir;
        if (useDirectDirectory) {
            QDir().mkpath(recordDirectory);
            dir.setPath(recordDirectory);
        } else {
            ensureHistoryModeDateStructure(recordDirectory, modeName, date);
            dir.setPath(historyModeDateSubDirectory(
                recordDirectory,
                modeName,
                date,
                historyAudioSubFolderName()
            ));
        }
        const QString safeMode = QString(modeName).replace(QRegExp(QStringLiteral("[^A-Za-z0-9_\\u4e00-\\u9fa5]")), QStringLiteral("_"));
        const QString baseName = QDateTime::currentDateTime().toString(QStringLiteral("HHmmss_"))
            + safeMode;
        int suffix = 0;
        do {
            const QString suffixText = suffix <= 0
                ? QString()
                : QStringLiteral("_") + QString::number(suffix);
            m_rawPath = dir.filePath(baseName + suffixText + QStringLiteral(".pcm"));
            m_lastWavPath = dir.filePath(baseName + suffixText + QStringLiteral(".wav"));
            ++suffix;
        } while (QFileInfo::exists(m_rawPath) || QFileInfo::exists(m_lastWavPath));

        m_file = new QFile(m_rawPath);
        if (!m_file->open(QIODevice::WriteOnly)) {
            if (error) {
                *error = tr8("无法创建录音文件。");
            }
            delete m_file;
            m_file = nullptr;
            return false;
        }

        m_capture = new AudioCaptureDevice(m_file);
        m_capture->setPcmListener(m_pcmListener);
        m_capture->open(QIODevice::WriteOnly);

        m_audioInput = new QAudioInput(format);
        m_audioInput->start(m_capture);
        m_timer.start();
        return true;
    }

public:
    void setPcmListener(
        const std::function<void(const QByteArray &)> &listener
    )
    {
        m_pcmListener = listener;
        if (m_capture) {
            m_capture->setPcmListener(listener);
        }
    }

    int takePeakLevel()
    {
        return m_capture ? m_capture->takePeakLevel() : 0;
    }

    QByteArray stop()
    {
        if (!m_audioInput) {
            return QByteArray();
        }

        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;

        if (m_capture) {
            m_capture->close();
            delete m_capture;
            m_capture = nullptr;
        }

        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }

        QFile raw(m_rawPath);
        QByteArray pcm;
        if (raw.open(QIODevice::ReadOnly)) {
            pcm = raw.readAll();
            raw.close();
            raw.remove();
        }

        QFile wav(m_lastWavPath);
        if (wav.open(QIODevice::WriteOnly)) {
            wav.write(wavFromPcm(pcm, 16000, 1, 16));
            wav.close();
        }
        return pcm;
    }

private:
    QAudioInput *m_audioInput = nullptr;
    AudioCaptureDevice *m_capture = nullptr;
    QFile *m_file = nullptr;
    QString m_rawPath;
    QString m_lastWavPath;
    QElapsedTimer m_timer;
    std::function<void(const QByteArray &)> m_pcmListener;
};

#endif // VOCEKIT_AUDIO_RECORDER_LEGACY_H
