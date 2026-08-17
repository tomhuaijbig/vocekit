#include "ocr_page_controller.h"

#include "app_dialogs.h"
#include "attention_message.h"
#include "ocr_page.h"

#include "../config/app_settings_defaults.h"
#include "../config/secret_config.h"
#include "../ocr/ocr_batch_text.h"
#include "../ocr/ocr_manager.h"
#include "../ocr/ocr_types.h"
#include "../ocr/screenshot_ocr_config.h"
#include "../runtime_log.h"
#include "../storage/history_record_service.h"
#include "../tasks/screenshot_text_action_task.h"

#include <QtConcurrent>
#include <QtWidgets>

namespace {

QString ocrPageTr8(const char *text)
{
    return QString::fromUtf8(text);
}

QString ocrModelForFunction(
    const AppSettingsData &settings,
    const QString &id
)
{
    const QString model = settings.function(id).modelId.trimmed();
    return model.isEmpty() ? defaultModelForFunction(id) : model;
}

} // namespace

OcrPageController::OcrPageController(
    const OcrPageAccess &access,
    QWidget *dialogParent,
    QObject *parent
)
    : QObject(parent),
      m_access(access),
      m_dialogParent(dialogParent)
{
    m_ocrManager = new OcrManager(this);
    refreshConfiguration();
    m_ocrManager->statusCallback = [this](const QString &status) {
        if (m_statusLabel) {
            m_statusLabel->setText(status);
        }
    };
    m_ocrManager->finishedCallback = [this](const OcrResult &result) {
        finishRecognition(result);
    };
}

OcrPageController::~OcrPageController()
{
    m_aiCancellation.cancel();
    if (m_ocrManager && m_ocrManager->isBusy()) {
        m_ocrManager->cancel();
    }
}

QWidget *OcrPageController::page()
{
    if (m_page) {
        return m_page;
    }

    OcrPageCallbacks callbacks;
    callbacks.selectImages = [this]() { selectImage(); };
    callbacks.startRecognition = [this]() { startRecognition(); };
    callbacks.cancelRecognition = [this]() {
        if (m_aiWatcher) {
            m_aiCancellation.cancel();
            if (m_statusLabel) {
                m_statusLabel->setText(
                    ocrPageTr8("正在取消模型处理")
                );
            }
            return;
        }
        if (m_ocrManager) {
            m_batchRunning = false;
            m_ocrManager->cancel();
            if (m_statusLabel) {
                m_statusLabel->setText(ocrPageTr8("正在取消"));
            }
        }
    };
    callbacks.previousImage = [this]() { switchImage(-1); };
    callbacks.nextImage = [this]() { switchImage(1); };
    callbacks.aiAction = [this](const QString &action) { runAiAction(action); };
    callbacks.resultTextChanged = [this](const QString &text) {
        if (!m_updatingResultEdit) {
            m_batchQueue.setEditedText(
                m_batchQueue.currentIndex(),
                text
            );
        }
    };

    m_page = new OcrPage(
        int(screenshotOcrEngineFromSettings(
            settingsSnapshot()
        )),
        callbacks
    );
    m_imagePathEdit = m_page->imagePathEdit();
    m_previewLabel = m_page->previewLabel();
    m_statusLabel = m_page->statusLabel();
    m_engineBox = m_page->engineBox();
    m_resultEdit = m_page->resultEdit();
    m_selectButton = m_page->selectButton();
    m_startButton = m_page->startButton();
    m_cancelButton = m_page->cancelButton();
    m_previousButton = m_page->previousButton();
    m_nextButton = m_page->nextButton();
    m_positionLabel = m_page->positionLabel();
    m_aiButtons = m_page->aiButtons();

    refreshPage();
    return m_page;
}

bool OcrPageController::pageCreated() const
{
    return m_page != nullptr;
}

void OcrPageController::refreshConfiguration()
{
    if (!m_ocrManager || !m_access.settingsSnapshotProvider) {
        return;
    }

    const AppSettingsData settings = settingsSnapshot();
    const SecretConfig secrets = loadSecrets();
    m_ocrManager->setConfig(
        buildScreenshotOcrManagerConfig(settings, secrets)
    );

    if (m_engineBox && !m_ocrManager->isBusy()) {
        const int index = m_engineBox->findData(int(
            screenshotOcrEngineFromSettings(settings)
        ));
        if (index >= 0) {
            m_engineBox->setCurrentIndex(index);
        }
    }
}

void OcrPageController::refreshPage()
{
    const bool busy = (m_ocrManager && m_ocrManager->isBusy())
        || m_aiWatcher;
    if (m_selectButton) {
        m_selectButton->setEnabled(!busy);
    }
    if (m_startButton) {
        m_startButton->setEnabled(!busy && !m_batchQueue.isEmpty());
    }
    if (m_cancelButton) {
        m_cancelButton->setEnabled(busy);
    }
    if (m_engineBox) {
        m_engineBox->setEnabled(!busy);
    }
    if (m_previousButton) {
        m_previousButton->setEnabled(m_batchQueue.currentIndex() > 0);
    }
    if (m_nextButton) {
        m_nextButton->setEnabled(
            m_batchQueue.currentIndex() >= 0
            && m_batchQueue.currentIndex() + 1 < m_batchQueue.count()
        );
    }
}

QPixmap OcrPageController::loadPreview(const QString &path) const
{
    QImageReader reader(path);
    const QSize sourceSize = reader.size();
    QSize targetSize = m_previewLabel
        ? m_previewLabel->size() - QSize(24, 24)
        : QSize(320, 320);
    if (targetSize.width() < 80 || targetSize.height() < 80) {
        targetSize = QSize(320, 320);
    }
    if (sourceSize.isValid()) {
        reader.setScaledSize(sourceSize.scaled(targetSize, Qt::KeepAspectRatio));
    }
    const QImage preview = reader.read();
    return preview.isNull() ? QPixmap() : QPixmap::fromImage(preview);
}

void OcrPageController::saveCurrentEditorText()
{
    if (m_resultEdit && !m_updatingResultEdit) {
        m_batchQueue.setEditedText(
            m_batchQueue.currentIndex(),
            m_resultEdit->toPlainText()
        );
    }
}

void OcrPageController::displayCurrentItem()
{
    const OcrBatchItem *item = m_batchQueue.currentItem();
    if (!item) {
        if (m_imagePathEdit) {
            m_imagePathEdit->clear();
        }
        if (m_previewLabel) {
            m_previewLabel->setPixmap(QPixmap());
            m_previewLabel->setText(ocrPageTr8("尚未选择图片"));
        }
        if (m_positionLabel) {
            m_positionLabel->setText(ocrPageTr8("0 / 0"));
        }
        if (m_statusLabel) {
            m_statusLabel->setText(ocrPageTr8("等待识别"));
        }
        if (m_resultEdit) {
            m_updatingResultEdit = true;
            m_resultEdit->clear();
            m_updatingResultEdit = false;
        }
        refreshPage();
        return;
    }

    if (m_imagePathEdit) {
        m_imagePathEdit->setText(QDir::toNativeSeparators(item->path));
    }
    if (m_positionLabel) {
        m_positionLabel->setText(
            QStringLiteral("%1 / %2")
                .arg(m_batchQueue.currentIndex() + 1)
                .arg(m_batchQueue.count())
        );
    }
    if (m_previewLabel) {
        const QPixmap preview = loadPreview(item->path);
        if (preview.isNull()) {
            m_previewLabel->setPixmap(QPixmap());
            m_previewLabel->setText(ocrPageTr8("图片预览不可用"));
        } else {
            m_previewLabel->setText(QString());
            m_previewLabel->setPixmap(preview);
        }
    }
    if (m_statusLabel) {
        m_statusLabel->setText(ocrBatchStatusText(*item));
    }
    if (m_resultEdit) {
        m_updatingResultEdit = true;
        m_resultEdit->setPlainText(item->text);
        m_updatingResultEdit = false;
    }
    refreshPage();
}

void OcrPageController::switchImage(int direction)
{
    saveCurrentEditorText();
    const bool changed = direction < 0
        ? m_batchQueue.movePrevious()
        : m_batchQueue.moveNext();
    if (changed) {
        displayCurrentItem();
    }
}

void OcrPageController::selectImage()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        m_dialogParent,
        ocrPageTr8("选择一张或多张图片"),
        QString(),
        ocrPageTr8("所有可读取的图片 (*.*)")
    );
    if (paths.isEmpty()) {
        return;
    }

    QStringList acceptedPaths;
    QStringList rejectedMessages;
    for (const QString &path : paths) {
        QString error;
        if (validateOcrImage(path, &error)) {
            acceptedPaths.append(path);
        } else if (rejectedMessages.size() < 8) {
            rejectedMessages.append(QFileInfo(path).fileName() + QStringLiteral(": ") + error);
        }
    }
    if (acceptedPaths.isEmpty()) {
        showAttentionWarning(
            m_dialogParent,
            ocrPageTr8("没有可识别的图片"),
            rejectedMessages.join(QStringLiteral("\n"))
        );
        return;
    }

    m_batchRunning = false;
    m_activeIndex = -1;
    m_batchQueue.replacePaths(acceptedPaths);
    displayCurrentItem();
    if (!rejectedMessages.isEmpty()) {
        showAttentionWarning(
            m_dialogParent,
            ocrPageTr8("部分图片已跳过"),
            rejectedMessages.join(QStringLiteral("\n"))
        );
    }
}

void OcrPageController::startRecognition()
{
    if (!m_ocrManager || m_batchQueue.isEmpty() || m_ocrManager->isBusy() || !m_engineBox) {
        return;
    }

    saveCurrentEditorText();
    const OcrEngine selectedEngine = OcrEngine(m_engineBox->currentData().toInt());
    if (selectedEngine == OcrEngine::CustomCloud) {
        const QMessageBox::StandardButton choice = QMessageBox::question(
            m_dialogParent,
            ocrPageTr8("发送图片到云端"),
            ocrPageTr8("当前选择了自定义云 OCR。继续后，所选图片会依次发送到你填写的接口地址。是否继续？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (choice != QMessageBox::Yes) {
            return;
        }
    }

    m_batchRunning = true;
    int index = m_batchQueue.nextPendingIndex(m_batchQueue.currentIndex());
    if (index < 0) {
        index = m_batchQueue.currentIndex();
    }
    startQueueItem(index);
}

void OcrPageController::startQueueItem(int index)
{
    if (!m_ocrManager || !m_engineBox || index < 0 || index >= m_batchQueue.count()) {
        m_batchRunning = false;
        return;
    }

    m_batchQueue.setCurrentIndex(index);
    m_batchQueue.markProcessing(index);
    m_activeIndex = index;
    displayCurrentItem();

    OcrRequest request;
    request.requestId = QUuid::createUuid().toString();
    request.requestId.remove(QLatin1Char('{'));
    request.requestId.remove(QLatin1Char('}'));
    request.imagePath = QDir::toNativeSeparators(m_batchQueue.item(index).path);
    request.languages = QStringList()
        << QStringLiteral("zh-Hans")
        << QStringLiteral("en");
    request.engine = OcrEngine(m_engineBox->currentData().toInt());

    m_ocrManager->recognize(request);
    refreshPage();
    logRuntimeEvent(
        ocrPageTr8("图片识别"),
        ocrPageTr8("开始"),
        QStringLiteral("引擎=") + m_engineBox->currentText()
            + QStringLiteral("，图片=") + QFileInfo(request.imagePath).fileName()
            + QStringLiteral("，序号=") + QString::number(index + 1)
            + QStringLiteral("/") + QString::number(m_batchQueue.count())
    );
}

void OcrPageController::finishRecognition(const OcrResult &result)
{
    const int completedIndex = m_activeIndex;
    if (completedIndex < 0 || completedIndex >= m_batchQueue.count()) {
        m_batchRunning = false;
        refreshPage();
        return;
    }
    const QString completedImagePath = m_batchQueue.item(completedIndex).path;
    m_batchQueue.applyResult(completedIndex, result);
    m_activeIndex = -1;
    refreshPage();
    if (result.errorCode == QStringLiteral("CANCELLED")) {
        m_batchRunning = false;
        if (m_batchQueue.currentIndex() == completedIndex) {
            displayCurrentItem();
        }
        logRuntimeEvent(
            ocrPageTr8("图片识别"),
            ocrPageTr8("取消"),
            QStringLiteral("用户取消了当前 OCR 任务")
        );
        return;
    }

    if (result.ok) {
        logRuntimeEvent(
            ocrPageTr8("图片识别"),
            ocrPageTr8("完成"),
            QStringLiteral("引擎=") + historyEngineName(result.engine)
                + QStringLiteral("，耗时=") + QString::number(result.elapsedMs) + QStringLiteral("ms")
        );
        saveHistory(result, completedImagePath);
    } else {
        logRuntimeEvent(
            ocrPageTr8("图片识别"),
            ocrPageTr8("失败"),
            QStringLiteral("错误码=") + result.errorCode
                + QStringLiteral("，原因=") + result.errorMessage
        );
        saveHistory(result, completedImagePath);
    }

    if (m_batchQueue.currentIndex() == completedIndex) {
        displayCurrentItem();
    }

    if (m_batchRunning) {
        const int nextIndex = m_batchQueue.nextPendingIndex(completedIndex + 1);
        if (nextIndex >= 0) {
            QTimer::singleShot(0, this, [this, nextIndex]() {
                startQueueItem(nextIndex);
            });
            return;
        }
    }

    m_batchRunning = false;
    int failedCount = 0;
    for (int i = 0; i < m_batchQueue.count(); ++i) {
        if (m_batchQueue.item(i).state == OcrBatchItemState::Failed) {
            ++failedCount;
        }
    }
    if (m_statusLabel) {
        m_statusLabel->setText(ocrBatchCompletionText(failedCount));
    }
    refreshPage();
}

QString OcrPageController::historyEngineName(OcrEngine engine) const
{
    if (engine == OcrEngine::WindowsOcr) {
        return QStringLiteral("Windows OCR");
    }
    if (engine == OcrEngine::CustomCloud) {
        return ocrPageTr8("自定义云 OCR");
    }
    if (engine == OcrEngine::Automatic) {
        return ocrPageTr8("自动选择");
    }
    return QStringLiteral("RapidOCR");
}

void OcrPageController::saveHistory(const OcrResult &result, const QString &imagePath)
{
    if (!m_access.settingsSnapshotProvider) {
        return;
    }

    const QString recordRoot = settingsSnapshot().recordDirectory;
    const QString modeTitle = ocrPageTr8("图片识别");

    OcrPageHistoryMetadataRequest metadataRequest;
    metadataRequest.result = result;
    metadataRequest.imagePath = imagePath;
    metadataRequest.languages = QStringList()
        << QStringLiteral("zh-Hans")
        << QStringLiteral("en");

    const HistoryAppendResult saved =
        HistoryRecordService(recordRoot).saveOcr(modeTitle, metadataRequest);
    if (!saved.ok) {
        logRuntimeEvent(
            ocrPageTr8("图片识别"),
            ocrPageTr8("历史保存失败"),
            QStringLiteral("路径=") + saved.modeDetailPath
        );
        return;
    }

    if (m_access.historyRecordSaved) {
        m_access.historyRecordSaved(saved.modeDetailPath);
    }
}

void OcrPageController::setAiButtonsEnabled(bool enabled)
{
    for (QPushButton *button : m_aiButtons) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}

void OcrPageController::runAiAction(const QString &action)
{
    if (m_aiWatcher) {
        showAttentionInformation(
            m_dialogParent,
            ocrPageTr8("正在处理"),
            ocrPageTr8("请等待当前模型任务完成。")
        );
        return;
    }

    const QString sourceText = m_resultEdit ? m_resultEdit->toPlainText().trimmed() : QString();
    if (sourceText.isEmpty()) {
        showAttentionInformation(
            m_dialogParent,
            ocrPageTr8("没有文字"),
            ocrPageTr8("请先识别图片或在结果框中填写文字。")
        );
        return;
    }

    QString instruction;
    if (action == QStringLiteral("ask")) {
        bool accepted = false;
        instruction = QInputDialog::getMultiLineText(
            m_dialogParent,
            ocrPageTr8("基于识别结果提问"),
            ocrPageTr8("问题"),
            QString(),
            &accepted
        ).trimmed();
        if (!accepted || instruction.isEmpty()) {
            return;
        }
    }

    const AppSettingsData settings = settingsSnapshot();
    QString model = ocrModelForFunction(settings, QStringLiteral("dictate"));
    QString systemPrompt;
    QString userPrompt;
    if (action == QStringLiteral("translate")) {
        model = ocrModelForFunction(settings, QStringLiteral("translate"));
        systemPrompt = ocrPageTr8("你是翻译助手。把输入文字翻译成目标语言，默认目标语言为简体中文。只输出译文；如果原文主要是中文，则翻译成英文。");
        userPrompt = sourceText;
    } else if (action == QStringLiteral("polish")) {
        systemPrompt = ocrPageTr8("你是文字润色助手。修正 OCR 错字和标点，改善表达，但不要添加原文没有的信息，只输出结果。");
        userPrompt = sourceText;
    } else if (action == QStringLiteral("summarize")) {
        model = ocrModelForFunction(settings, QStringLiteral("ask"));
        systemPrompt = ocrPageTr8("你是内容总结助手。准确提炼输入文字的核心信息，只输出总结结果。");
        userPrompt = sourceText;
    } else if (action == QStringLiteral("ask")) {
        model = ocrModelForFunction(settings, QStringLiteral("ask"));
        systemPrompt = ocrPageTr8("你是文本问答助手。只能依据用户提供的 OCR 文字回答；信息不足时明确说明。");
        userPrompt = ocrPageTr8("OCR 文字：\n") + sourceText + ocrPageTr8("\n\n问题：\n") + instruction;
    } else {
        systemPrompt = ocrPageTr8("你是 OCR 文本整理助手。修正明显识别错误，恢复合理段落和标点，不改变原意，只输出整理后的文字。");
        userPrompt = sourceText;
    }

    const bool useSystemProxy = settings.useSystemProxy;
    setAiButtonsEnabled(false);
    if (m_statusLabel) {
        m_statusLabel->setText(ocrPageTr8("模型处理中"));
    }
    m_aiWatcher = new QFutureWatcher<OcrAiTaskResult>(this);
    connect(m_aiWatcher, &QFutureWatcher<OcrAiTaskResult>::finished, this, [this]() {
        const OcrAiTaskResult result = m_aiWatcher->result();
        const bool cancelled =
            m_aiCancellation.isCancellationRequested();
        m_aiWatcher->deleteLater();
        m_aiWatcher = nullptr;
        setAiButtonsEnabled(true);
        refreshPage();

        if (cancelled) {
            if (m_statusLabel) {
                m_statusLabel->setText(
                    ocrPageTr8("模型处理已取消")
                );
            }
            return;
        }

        if (!result.error.isEmpty()) {
            if (m_statusLabel) {
                m_statusLabel->setText(ocrPageTr8("模型处理失败"));
            }
            showAttentionWarning(m_dialogParent, ocrPageTr8("模型处理失败"), result.error);
            return;
        }
        if (m_resultEdit) {
            m_resultEdit->setPlainText(result.text);
        }
        if (m_statusLabel) {
            m_statusLabel->setText(ocrPageTr8("模型处理完成"));
        }
    });

    ScreenshotTextActionTaskRequest taskRequest;
    taskRequest.model = model;
    taskRequest.systemPrompt = systemPrompt;
    taskRequest.sourceText = userPrompt;
    taskRequest.useSystemProxy = useSystemProxy;
    m_aiCancellation = CancellationSource();
    taskRequest.cancellation = m_aiCancellation.token();

    const QFuture<OcrAiTaskResult> future = QtConcurrent::run(
        runScreenshotTextActionTask,
        taskRequest
    );
    m_aiWatcher->setFuture(future);
    refreshPage();
}

AppSettingsData OcrPageController::settingsSnapshot() const
{
    return m_access.settingsSnapshotProvider
        ? m_access.settingsSnapshotProvider()
        : AppSettingsData();
}
