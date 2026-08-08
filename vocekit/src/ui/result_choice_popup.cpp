#include "result_choice_popup.h"

#include "../providers/model_catalog.h"
#include "../result_flow_config.h"
#include "app_dialogs.h"
#include "screen_position.h"
#include "ui_style.h"

#include <QtWidgets>

namespace {

QString popupTr8(const char *text)
{
    return QString::fromUtf8(text);
}

bool isAdvancedAction(const QString &id)
{
    return id == QStringLiteral("regenerate")
        || id == QStringLiteral("retryModel")
        || id == QStringLiteral("followUp")
        || id == QStringLiteral("expand")
        || id == QStringLiteral("vocabulary");
}

} // namespace

ResultChoicePopup::ResultChoicePopup(
    const ResultPopupWindowPreferences &preferences,
    const QString &title,
    const QString &result,
    ClipboardWindowHandle targetWindow,
    bool hasSelection,
    int autoCloseMsec,
    QWidget *parent
)
    : QWidget(parent),
      m_preferences(preferences),
      m_result(result),
      m_initialResult(result),
      m_targetWindow(targetWindow),
      m_hasSelection(hasSelection),
      m_autoCloseMsec(qMax(0, autoCloseMsec))
{
    setWindowTitle(popupTr8("语音助手"));
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(640, 460);
    resize(760, 520);
    setFont(appFont());
    setObjectName(QStringLiteral("resultChoicePopup"));
    setStyleSheet(QStringLiteral(
        "QWidget#resultChoicePopup { background:#f8fafc; }"
        "QTextEdit { background:#ffffff; color:#111827; border:1px solid #e4e7ec;"
        " border-radius:6px; padding:10px; selection-background-color:#2563eb; }"
        "QLabel#resultHint { color:#667085; }"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *heading = new QLabel(title.trimmed().isEmpty()
        ? popupTr8("处理结果")
        : title);
    heading->setFont(appFont(15, QFont::DemiBold));
    m_hint = new QLabel(popupTr8("已生成"));
    m_hint->setObjectName(QStringLiteral("resultHint"));
    header->addWidget(heading);
    header->addStretch();
    header->addWidget(m_hint);
    layout->addLayout(header);

    m_editor = new QTextEdit;
    m_editor->setAcceptRichText(false);
    m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
    m_editor->setPlainText(result);
    m_editor->setMinimumHeight(250);
    layout->addWidget(m_editor, 1);

    m_advancedLayout = new QHBoxLayout;
    m_advancedLayout->setSpacing(8);
    m_advancedLayout->addStretch();
    m_resultLayout = new QHBoxLayout;
    m_resultLayout->setSpacing(8);
    m_resultLayout->addStretch();

    m_regenerateButton = popupButton(popupTr8("重新生成"), false);
    m_retryModelButton = popupButton(popupTr8("换模型重试"), false);
    m_followUpButton = popupButton(popupTr8("继续追问"), false);
    m_expandButton = popupButton(popupTr8("展开全文"), false);
    m_vocabularyButton = popupButton(popupTr8("加入词库"), false);
    m_copyButton = popupButton(popupTr8("复制"), false);
    m_writeButton = popupButton(popupTr8("写入"), true);
    m_replaceButton = popupButton(popupTr8("替换选中"), false);
    m_closeButton = popupButton(popupTr8("关闭"), false);

    m_actionButtons.insert(QStringLiteral("regenerate"), m_regenerateButton);
    m_actionButtons.insert(QStringLiteral("retryModel"), m_retryModelButton);
    m_actionButtons.insert(QStringLiteral("followUp"), m_followUpButton);
    m_actionButtons.insert(QStringLiteral("expand"), m_expandButton);
    m_actionButtons.insert(QStringLiteral("vocabulary"), m_vocabularyButton);
    m_actionButtons.insert(QStringLiteral("copy"), m_copyButton);
    m_actionButtons.insert(QStringLiteral("write"), m_writeButton);
    m_actionButtons.insert(QStringLiteral("replace"), m_replaceButton);
    for (auto it = m_actionButtons.constBegin();
         it != m_actionButtons.constEnd();
         ++it) {
        it.value()->setObjectName(
            QStringLiteral("resultAction_") + it.key()
        );
    }
    m_closeButton->setObjectName(
        QStringLiteral("resultAction_close")
    );

    layout->addLayout(m_advancedLayout);
    layout->addLayout(m_resultLayout);

    connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        syncResultFromEditor();
        if (!m_programmaticTextChange && !m_busy) {
            m_userEdited = true;
            if (m_onLiveDraft) {
                m_onLiveDraft(m_result);
            }
        }
        updateActionState();
    });
    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        syncResultFromEditor();
        QApplication::clipboard()->setText(m_result);
        m_hint->setText(popupTr8("已复制"));
    });
    connect(m_writeButton, &QPushButton::clicked, this, [this]() {
        syncResultFromEditor();
        const QString text = m_result;
        if (m_onCheckedWrite) {
            const ClipboardWriteResult write =
                m_onCheckedWrite(
                    QStringLiteral("write"),
                    text,
                    m_targetWindow,
                    m_hasSelection
                );
            if (!write.ok) {
                m_hint->setText(
                    popupTr8("无法写回原目标窗口，请复制后手动粘贴")
                );
                return;
            }
            resolveResult(QStringLiteral("write"));
            close();
            return;
        }
        resolveResult(QStringLiteral("write"));
        close();
        ClipboardWriter::pasteTextToWindow(text, m_targetWindow, false, m_hasSelection);
    });
    connect(m_replaceButton, &QPushButton::clicked, this, [this]() {
        syncResultFromEditor();
        const QString text = m_result;
        if (m_onCheckedWrite) {
            const ClipboardWriteResult write =
                m_onCheckedWrite(
                    QStringLiteral("replace"),
                    text,
                    m_targetWindow,
                    true
                );
            if (!write.ok) {
                m_hint->setText(
                    popupTr8("原选区不可用，请复制后手动替换")
                );
                return;
            }
            resolveResult(QStringLiteral("replace"));
            close();
            return;
        }
        resolveResult(QStringLiteral("replace"));
        close();
        ClipboardWriter::pasteTextToWindow(text, m_targetWindow, true, true);
    });
    connect(m_regenerateButton, &QPushButton::clicked, this, [this]() {
        if (m_onRegenerate) {
            m_onRegenerate();
        }
    });
    connect(m_retryModelButton, &QPushButton::clicked, this, [this]() {
        chooseModelAndRetry();
    });
    connect(m_followUpButton, &QPushButton::clicked, this, [this]() {
        askFollowUp();
    });
    connect(m_expandButton, &QPushButton::clicked, this, [this]() {
        showExpandedResult();
    });
    connect(m_vocabularyButton, &QPushButton::clicked, this, [this]() {
        syncResultFromEditor();
        if (m_onVocabulary) {
            m_onVocabulary(m_initialResult, m_result);
        }
    });
    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        resolveResult(QStringLiteral("close"));
        close();
    });

    setOpacityPercent(m_preferences.opacityPercent);
    setActionOrder(defaultResultActionIds());
    updateActionState();
}

void ResultChoicePopup::setActionOrder(const QStringList &actionIds)
{
    const QStringList normalized = normalizeResultActionIds(actionIds);
    for (QPushButton *button : m_actionButtons) {
        m_advancedLayout->removeWidget(button);
        m_resultLayout->removeWidget(button);
        button->hide();
    }
    m_resultLayout->removeWidget(m_closeButton);
    m_closeButton->hide();

    for (const QString &id : normalized) {
        QPushButton *button = m_actionButtons.value(id, nullptr);
        if (!button) {
            continue;
        }
        (isAdvancedAction(id) ? m_advancedLayout : m_resultLayout)
            ->addWidget(button);
        button->show();
    }
    m_resultLayout->addWidget(m_closeButton);
    m_closeButton->show();
    updateActionState();
}

void ResultChoicePopup::setOpacityPercent(int percent)
{
    setWindowOpacity(qBound(60, percent, 100) / 100.0);
}

void ResultChoicePopup::setResolvedCallback(
    const std::function<void(const QString &)> &onResolved)
{
    m_onResolved = onResolved;
}

void ResultChoicePopup::setCheckedWriteCallback(
    const std::function<ClipboardWriteResult(
        const QString &,
        const QString &,
        ClipboardWindowHandle,
        bool
    )> &onWrite)
{
    m_onCheckedWrite = onWrite;
}

void ResultChoicePopup::setActionCallbacks(
    const std::function<void()> &onRegenerate,
    const std::function<void(const QString &)> &onRetryModel,
    const std::function<void(const QString &)> &onFollowUp)
{
    m_onRegenerate = onRegenerate;
    m_onRetryModel = onRetryModel;
    m_onFollowUp = onFollowUp;
    updateActionState();
}

void ResultChoicePopup::setCancellationCallback(
    const std::function<void()> &onCancellation)
{
    m_onCancellation = onCancellation;
}

void ResultChoicePopup::setDraftCallback(
    const std::function<void(const QString &)> &onDraft)
{
    m_onDraft = onDraft;
}

void ResultChoicePopup::setLiveDraftCallback(
    const std::function<void(const QString &)> &onLiveDraft)
{
    m_onLiveDraft = onLiveDraft;
}

void ResultChoicePopup::setVocabularyCallback(
    const std::function<void(const QString &, const QString &)> &onVocabulary)
{
    m_onVocabulary = onVocabulary;
    updateActionState();
}

void ResultChoicePopup::setWindowPreferenceCallback(
    const std::function<void(const QRect &)> &onChanged)
{
    m_onWindowPreferenceChanged = onChanged;
}

void ResultChoicePopup::setCurrentModel(const QString &model)
{
    m_currentModel = model;
}

QString ResultChoicePopup::currentModel() const
{
    return m_currentModel;
}

void ResultChoicePopup::setHasSelection(bool hasSelection)
{
    m_hasSelection = hasSelection;
    updateActionState();
}

void ResultChoicePopup::setResultText(
    const QString &result,
    bool resetDraftState)
{
    m_result = result;
    m_programmaticTextChange = true;
    m_editor->setPlainText(result);
    m_editor->moveCursor(QTextCursor::End);
    m_programmaticTextChange = false;
    if (resetDraftState) {
        m_initialResult = result;
        m_userEdited = false;
        m_draftSaved = false;
    }
    updateActionState();
}

void ResultChoicePopup::appendResultText(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    m_programmaticTextChange = true;
    m_editor->moveCursor(QTextCursor::End);
    m_editor->insertPlainText(text);
    m_editor->moveCursor(QTextCursor::End);
    m_programmaticTextChange = false;
    syncResultFromEditor();
    m_initialResult = m_result;
    updateActionState();
}

QString ResultChoicePopup::resultText() const
{
    return m_editor ? m_editor->toPlainText() : m_result;
}

void ResultChoicePopup::setBusy(bool busy, const QString &hint)
{
    m_busy = busy;
    m_editor->setReadOnly(busy);
    m_hint->setText(hint.trimmed().isEmpty()
        ? (busy ? popupTr8("正在生成") : popupTr8("请选择下一步操作"))
        : hint);
    updateActionState();
    if (!m_busy) {
        scheduleAutoClose();
    }
}

void ResultChoicePopup::setAutoCloseMsec(int autoCloseMsec)
{
    m_autoCloseMsec = qMax(0, autoCloseMsec);
    ++m_autoCloseGeneration;
    scheduleAutoClose();
}

void ResultChoicePopup::showNearBottom()
{
    const QRect screen = QApplication::desktop()->availableGeometry(
        QApplication::desktop()->screenNumber(QCursor::pos())
    );
    if (m_preferences.hasGeometry && m_preferences.geometry.isValid()) {
        const QRect saved = m_preferences.geometry;
        resize(saved.size().expandedTo(minimumSize()));
        move(clampedTopLeftToScreen(saved.topLeft(), size()));
    } else {
        resize(
            qMin(760, qMax(minimumWidth(), screen.width() - 80)),
            qMin(520, qMax(minimumHeight(), screen.height() - 100))
        );
        move(
            screen.center().x() - width() / 2,
            screen.bottom() - height() - 40
        );
    }
    show();
    raise();
    activateWindow();
    scheduleAutoClose();
}

void ResultChoicePopup::closeEvent(QCloseEvent *event)
{
    if (m_busy && m_onCancellation) {
        m_onCancellation();
    }
    saveDraftIfNeeded();
    saveGeometryPreference();
    resolveResult(QStringLiteral("close"));
    QWidget::closeEvent(event);
}

QPushButton *ResultChoicePopup::popupButton(
    const QString &text,
    bool primary)
{
    auto *button = new QPushButton(text);
    button->setMinimumSize(64, 40);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(primary
        ? buttonStyle(QStringLiteral("#111827"))
        : buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    return button;
}

void ResultChoicePopup::updateActionState()
{
    syncResultFromEditor();
    const bool hasResult = !m_result.trimmed().isEmpty();
    m_copyButton->setEnabled(!m_busy && hasResult);
    m_writeButton->setEnabled(!m_busy && hasResult);
    m_replaceButton->setEnabled(!m_busy && hasResult && m_hasSelection);
    m_regenerateButton->setEnabled(!m_busy && bool(m_onRegenerate));
    m_retryModelButton->setEnabled(!m_busy && bool(m_onRetryModel));
    m_followUpButton->setEnabled(!m_busy && bool(m_onFollowUp));
    m_expandButton->setEnabled(!m_busy && hasResult);
    m_vocabularyButton->setEnabled(!m_busy && hasResult && bool(m_onVocabulary));
}

void ResultChoicePopup::chooseModelAndRetry()
{
    if (!m_onRetryModel) {
        return;
    }

    HelpDialog dialog(
        popupTr8("换模型重试帮助"),
        popupTr8("选择另一个已配置的大模型，并用相同输入重新生成。"),
        this
    );
    dialog.setWindowTitle(popupTr8("换模型重试"));
    dialog.resize(430, 180);
    auto *layout = new QVBoxLayout(&dialog);
    auto *label = new QLabel(popupTr8("选择模型"));
    auto *models = new QComboBox;
    const QVector<ModelOption> options = modelOptions();
    for (const ModelOption &option : options) {
        models->addItem(option.title, option.id);
    }
    const QString displayedModelId = normalizeModelId(
        m_currentModel,
        m_currentModel
    );
    const int currentIndex = models->findData(displayedModelId);
    if (currentIndex >= 0) {
        models->setCurrentIndex(currentIndex);
    }
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = popupButton(popupTr8("取消"), false);
    auto *retry = popupButton(popupTr8("重新生成"), true);
    buttons->addWidget(cancel);
    buttons->addWidget(retry);
    layout->addWidget(label);
    layout->addWidget(models);
    layout->addStretch();
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(retry, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() == QDialog::Accepted && models->currentIndex() >= 0) {
        m_onRetryModel(models->currentData().toString());
    }
}

void ResultChoicePopup::askFollowUp()
{
    if (!m_onFollowUp) {
        return;
    }

    HelpDialog dialog(
        popupTr8("继续追问帮助"),
        popupTr8("输入新的问题或补充要求，会基于上一次输入和当前结果继续处理。"),
        this
    );
    dialog.setWindowTitle(popupTr8("继续追问"));
    dialog.resize(520, 390);
    auto *layout = new QVBoxLayout(&dialog);
    auto *instruction = new QLabel(
        popupTr8("输入新的问题或补充要求，会基于上一次输入和当前结果继续处理。")
    );
    instruction->setWordWrap(true);
    auto *editor = new QTextEdit;
    editor->setPlaceholderText(popupTr8("例如：再简短一点，或者继续解释第二点。"));
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *cancel = popupButton(popupTr8("取消"), false);
    auto *send = popupButton(popupTr8("发送"), true);
    buttons->addWidget(cancel);
    buttons->addWidget(send);
    layout->addWidget(instruction);
    layout->addWidget(editor, 1);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(send, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() == QDialog::Accepted) {
        const QString followUp = editor->toPlainText().trimmed();
        if (!followUp.isEmpty()) {
            m_onFollowUp(followUp);
        }
    }
}

void ResultChoicePopup::showExpandedResult()
{
    syncResultFromEditor();
    HelpDialog dialog(
        popupTr8("完整内容帮助"),
        popupTr8("这里可以查看和修改完整结果，关闭后修改会同步回结果小框。"),
        this
    );
    dialog.setWindowTitle(popupTr8("完整内容"));
    dialog.resize(760, 620);
    auto *layout = new QVBoxLayout(&dialog);
    auto *editor = new QTextEdit;
    editor->setAcceptRichText(false);
    editor->setPlainText(m_result);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *copy = popupButton(popupTr8("复制"), false);
    auto *close = popupButton(popupTr8("关闭"), true);
    buttons->addWidget(copy);
    buttons->addWidget(close);
    layout->addWidget(editor, 1);
    layout->addLayout(buttons);
    connect(copy, &QPushButton::clicked, &dialog, [editor]() {
        QApplication::clipboard()->setText(editor->toPlainText());
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();

    const QString edited = editor->toPlainText();
    if (edited != m_result) {
        m_userEdited = true;
        setResultText(edited, false);
        if (m_onLiveDraft) {
            m_onLiveDraft(edited);
        }
    }
}

void ResultChoicePopup::syncResultFromEditor()
{
    if (m_editor) {
        m_result = m_editor->toPlainText();
    }
}

bool ResultChoicePopup::shouldSaveDraft() const
{
    const QString current = resultText();
    return m_userEdited
        && !m_draftSaved
        && !m_busy
        && !current.trimmed().isEmpty()
        && current != m_initialResult
        && bool(m_onDraft);
}

void ResultChoicePopup::saveDraftIfNeeded()
{
    if (!shouldSaveDraft()) {
        return;
    }
    m_draftSaved = true;
    m_onDraft(resultText());
}

void ResultChoicePopup::saveGeometryPreference()
{
    if (!m_onWindowPreferenceChanged || width() <= 0 || height() <= 0) {
        return;
    }
    m_onWindowPreferenceChanged(QRect(pos(), size()));
}

void ResultChoicePopup::resolveResult(const QString &action)
{
    if (m_resolved) {
        return;
    }
    m_resolved = true;
    if (m_onResolved) {
        m_onResolved(action);
    }
}

void ResultChoicePopup::scheduleAutoClose()
{
    if (!isVisible() || m_busy || m_autoCloseMsec <= 0) {
        return;
    }
    const quint64 generation = ++m_autoCloseGeneration;
    m_hint->setText(popupTr8("将在设定时间后自动关闭"));
    QTimer::singleShot(
        m_autoCloseMsec,
        this,
        [this, generation]() {
            if (generation == m_autoCloseGeneration
                && !m_busy) {
                close();
            }
        }
    );
}
