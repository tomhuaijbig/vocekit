#include "result_choice_popup.h"

#include "../providers/model_catalog.h"
#include "../result_flow_config.h"
#include "app_dialogs.h"
#include "screen_position.h"
#include "ui_style.h"

#include <QDesktopServices>
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

void configureSafeExternalLinks(QTextBrowser *browser)
{
    if (!browser) {
        return;
    }
    browser->setOpenExternalLinks(false);
    browser->setOpenLinks(false);
    QObject::connect(
        browser,
        &QTextBrowser::anchorClicked,
        browser,
        [](const QUrl &url) {
            const QString scheme = url.scheme().toLower();
            if (scheme == QStringLiteral("http")
                || scheme == QStringLiteral("https")) {
                QDesktopServices::openUrl(url);
            }
        }
    );
}

bool isMarkdownCodeBlock(const QTextBlock &block)
{
    const QTextBlockFormat format = block.blockFormat();
    return format.hasProperty(QTextFormat::BlockCodeFence)
        || format.hasProperty(QTextFormat::BlockCodeLanguage);
}

class MarkdownCodeHighlighter final : public QSyntaxHighlighter
{
public:
    explicit MarkdownCodeHighlighter(QTextDocument *document)
        : QSyntaxHighlighter(document)
    {
    }

protected:
    void highlightBlock(const QString &text) override
    {
        const QTextBlock block = currentBlock();
        if (!isMarkdownCodeBlock(block)) {
            return;
        }

        QTextCharFormat base;
        base.setFontFamilies(QStringList() << QStringLiteral("Consolas"));
        base.setForeground(QColor(QStringLiteral("#e5e7eb")));
        base.setBackground(QColor(QStringLiteral("#111827")));
        setFormat(0, text.size(), base);

        QString language = block.blockFormat()
            .property(QTextFormat::BlockCodeLanguage)
            .toString()
            .trimmed()
            .toLower();
        QStringList keywords;
        if (language.contains(QStringLiteral("py"))) {
            keywords = QStringList() << QStringLiteral("and")
                << QStringLiteral("as") << QStringLiteral("class")
                << QStringLiteral("def") << QStringLiteral("elif")
                << QStringLiteral("else") << QStringLiteral("False")
                << QStringLiteral("for") << QStringLiteral("from")
                << QStringLiteral("if") << QStringLiteral("import")
                << QStringLiteral("in") << QStringLiteral("None")
                << QStringLiteral("not") << QStringLiteral("or")
                << QStringLiteral("return") << QStringLiteral("True")
                << QStringLiteral("while") << QStringLiteral("with");
        } else {
            keywords = QStringList() << QStringLiteral("auto")
                << QStringLiteral("bool") << QStringLiteral("break")
                << QStringLiteral("case") << QStringLiteral("class")
                << QStringLiteral("const") << QStringLiteral("continue")
                << QStringLiteral("double") << QStringLiteral("else")
                << QStringLiteral("false") << QStringLiteral("float")
                << QStringLiteral("for") << QStringLiteral("if")
                << QStringLiteral("int") << QStringLiteral("namespace")
                << QStringLiteral("nullptr") << QStringLiteral("private")
                << QStringLiteral("protected") << QStringLiteral("public")
                << QStringLiteral("return") << QStringLiteral("static")
                << QStringLiteral("struct") << QStringLiteral("true")
                << QStringLiteral("void") << QStringLiteral("while");
        }

        QTextCharFormat keywordFormat;
        keywordFormat.setForeground(QColor(QStringLiteral("#93c5fd")));
        keywordFormat.setBackground(QColor(QStringLiteral("#111827")));
        keywordFormat.setFontWeight(QFont::DemiBold);
        const QRegularExpression keywordExpression(
            QStringLiteral("\\b(?:")
                + keywords.join(QLatin1Char('|'))
                + QStringLiteral(")\\b")
        );
        auto matches = keywordExpression.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), keywordFormat);
        }

        QTextCharFormat numberFormat;
        numberFormat.setForeground(QColor(QStringLiteral("#fbbf24")));
        numberFormat.setBackground(QColor(QStringLiteral("#111827")));
        matches = QRegularExpression(
            QStringLiteral("\\b(?:0x[0-9a-fA-F]+|\\d+(?:\\.\\d+)?)\\b")
        ).globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), numberFormat);
        }

        QTextCharFormat stringFormat;
        stringFormat.setForeground(QColor(QStringLiteral("#86efac")));
        stringFormat.setBackground(QColor(QStringLiteral("#111827")));
        matches = QRegularExpression(
            QStringLiteral("(?:\\\"(?:\\\\.|[^\\\"])*\\\"|'(?:\\\\.|[^'])*')")
        ).globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), stringFormat);
        }

        QTextCharFormat commentFormat;
        commentFormat.setForeground(QColor(QStringLiteral("#94a3b8")));
        commentFormat.setBackground(QColor(QStringLiteral("#111827")));
        commentFormat.setFontItalic(true);
        const QRegularExpressionMatch comment = QRegularExpression(
            language.contains(QStringLiteral("py"))
                ? QStringLiteral("#.*$")
                : QStringLiteral("//.*$")
        ).match(text);
        if (comment.hasMatch()) {
            setFormat(comment.capturedStart(), comment.capturedLength(), commentFormat);
        }
    }
};

QString readableMathExpression(QString expression)
{
    if (expression.startsWith(QStringLiteral("$$"))
        && expression.endsWith(QStringLiteral("$$"))) {
        expression = expression.mid(2, expression.size() - 4);
    } else if (expression.startsWith(QLatin1Char('$'))
               && expression.endsWith(QLatin1Char('$'))) {
        expression = expression.mid(1, expression.size() - 2);
    } else if ((expression.startsWith(QStringLiteral("\\("))
                && expression.endsWith(QStringLiteral("\\)")))
               || (expression.startsWith(QStringLiteral("\\["))
                   && expression.endsWith(QStringLiteral("\\]")))) {
        expression = expression.mid(2, expression.size() - 4);
    }
    expression.replace(
        QRegularExpression(QStringLiteral("\\\\frac\\{([^{}]+)\\}\\{([^{}]+)\\}")),
        QStringLiteral("(\\1)/(\\2)")
    );
    expression.replace(
        QRegularExpression(QStringLiteral("\\\\sqrt\\{([^{}]+)\\}")),
        QStringLiteral("√(\\1)")
    );
    const QVector<QPair<QString, QString>> symbols = {
        {QStringLiteral("\\int"), QStringLiteral("∫")},
        {QStringLiteral("\\sum"), QStringLiteral("∑")},
        {QStringLiteral("\\prod"), QStringLiteral("∏")},
        {QStringLiteral("\\infty"), QStringLiteral("∞")},
        {QStringLiteral("\\times"), QStringLiteral("×")},
        {QStringLiteral("\\cdot"), QStringLiteral("·")},
        {QStringLiteral("\\pm"), QStringLiteral("±")},
        {QStringLiteral("\\le"), QStringLiteral("≤")},
        {QStringLiteral("\\ge"), QStringLiteral("≥")},
        {QStringLiteral("\\neq"), QStringLiteral("≠")},
        {QStringLiteral("\\approx"), QStringLiteral("≈")},
        {QStringLiteral("\\alpha"), QStringLiteral("α")},
        {QStringLiteral("\\beta"), QStringLiteral("β")},
        {QStringLiteral("\\gamma"), QStringLiteral("γ")},
        {QStringLiteral("\\delta"), QStringLiteral("δ")},
        {QStringLiteral("\\theta"), QStringLiteral("θ")},
        {QStringLiteral("\\lambda"), QStringLiteral("λ")},
        {QStringLiteral("\\mu"), QStringLiteral("μ")},
        {QStringLiteral("\\pi"), QStringLiteral("π")},
        {QStringLiteral("\\sigma"), QStringLiteral("σ")},
        {QStringLiteral("\\omega"), QStringLiteral("ω")}
    };
    for (const auto &symbol : symbols) {
        expression.replace(symbol.first, symbol.second);
    }
    const QVector<QPair<QString, QString>> scripts = {
        {QStringLiteral("^0"), QStringLiteral("⁰")},
        {QStringLiteral("^1"), QStringLiteral("¹")},
        {QStringLiteral("^2"), QStringLiteral("²")},
        {QStringLiteral("^3"), QStringLiteral("³")},
        {QStringLiteral("^4"), QStringLiteral("⁴")},
        {QStringLiteral("^5"), QStringLiteral("⁵")},
        {QStringLiteral("^6"), QStringLiteral("⁶")},
        {QStringLiteral("^7"), QStringLiteral("⁷")},
        {QStringLiteral("^8"), QStringLiteral("⁸")},
        {QStringLiteral("^9"), QStringLiteral("⁹")},
        {QStringLiteral("_0"), QStringLiteral("₀")},
        {QStringLiteral("_1"), QStringLiteral("₁")},
        {QStringLiteral("_2"), QStringLiteral("₂")},
        {QStringLiteral("_3"), QStringLiteral("₃")},
        {QStringLiteral("_4"), QStringLiteral("₄")},
        {QStringLiteral("_5"), QStringLiteral("₅")},
        {QStringLiteral("_6"), QStringLiteral("₆")},
        {QStringLiteral("_7"), QStringLiteral("₇")},
        {QStringLiteral("_8"), QStringLiteral("₈")},
        {QStringLiteral("_9"), QStringLiteral("₉")}
    };
    for (const auto &script : scripts) {
        expression.replace(script.first, script.second);
    }
    return expression.trimmed();
}

void decorateMarkdownDocument(QTextDocument *document)
{
    if (!document) {
        return;
    }
    if (!document->property("vocekitMarkdownHighlighter").toBool()) {
        new MarkdownCodeHighlighter(document);
        document->setProperty("vocekitMarkdownHighlighter", true);
    }

    const QString plain = document->toPlainText();
    const QRegularExpression mathExpression(QStringLiteral(
        "(?s)(\\$\\$.+?\\$\\$|\\\\\\[.+?\\\\\\]|(?<!\\\\)\\$[^$\\n]+?\\$|\\\\\\(.+?\\\\\\))"
    ));
    QVector<QRegularExpressionMatch> mathMatches;
    auto matches = mathExpression.globalMatch(plain);
    while (matches.hasNext()) {
        mathMatches.append(matches.next());
    }
    for (int i = mathMatches.size() - 1; i >= 0; --i) {
        const QRegularExpressionMatch match = mathMatches.at(i);
        const QTextBlock block = document->findBlock(match.capturedStart());
        if (isMarkdownCodeBlock(block)) {
            continue;
        }
        const bool blockFormula = match.captured().startsWith(QStringLiteral("$$"))
            || match.captured().startsWith(QStringLiteral("\\["));
        QTextCursor cursor(document);
        cursor.setPosition(match.capturedStart());
        cursor.setPosition(
            match.capturedStart() + match.capturedLength(),
            QTextCursor::KeepAnchor
        );
        QTextCharFormat formula;
        formula.setFontFamilies(QStringList() << QStringLiteral("Cambria Math"));
        formula.setForeground(QColor(QStringLiteral("#1d4ed8")));
        formula.setFontWeight(QFont::DemiBold);
        if (blockFormula) {
            formula.setBackground(QColor(QStringLiteral("#eff6ff")));
        }
        cursor.insertText(readableMathExpression(match.captured()), formula);
        if (blockFormula) {
            QTextBlockFormat blockFormat = cursor.blockFormat();
            blockFormat.setAlignment(Qt::AlignHCenter);
            cursor.mergeBlockFormat(blockFormat);
        }
    }

    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        if (isMarkdownCodeBlock(block)) {
            QTextCursor blockCursor(block);
            QTextBlockFormat format = block.blockFormat();
            format.setBackground(QColor(QStringLiteral("#111827")));
            format.setLeftMargin(10.0);
            format.setRightMargin(10.0);
            blockCursor.setBlockFormat(format);
        }
        for (QTextBlock::Iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || !fragment.charFormat().isAnchor()) {
                continue;
            }
            QTextCursor linkCursor(document);
            linkCursor.setPosition(fragment.position());
            linkCursor.setPosition(
                fragment.position() + fragment.length(),
                QTextCursor::KeepAnchor
            );
            QTextCharFormat linkFormat = fragment.charFormat();
            linkFormat.setForeground(QColor(QStringLiteral("#2563eb")));
            linkFormat.setFontUnderline(true);
            linkCursor.mergeCharFormat(linkFormat);
        }
    }
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
        "QTextEdit, QTextBrowser { background:#ffffff; color:#111827; border:1px solid #e4e7ec;"
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

    auto *viewRow = new QHBoxLayout;
    viewRow->setSpacing(6);
    m_renderedViewButton = popupButton(popupTr8("渲染后的回答"), false);
    m_editViewButton = popupButton(popupTr8("编辑原文"), false);
    m_detailsButton = popupButton(popupTr8("原始响应 / 调试详情"), false);
    m_detailsButton->setEnabled(false);
    m_renderedViewButton->setCheckable(true);
    m_editViewButton->setCheckable(true);
    m_renderedViewButton->setChecked(true);
    viewRow->addWidget(m_renderedViewButton);
    viewRow->addWidget(m_editViewButton);
    viewRow->addStretch();
    viewRow->addWidget(m_detailsButton);
    layout->addLayout(viewRow);

    m_editor = new QTextEdit;
    m_editor->setAcceptRichText(false);
    m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
    m_editor->setPlainText(result);
    m_editor->setMinimumHeight(250);
    m_rendered = new QTextBrowser;
    configureSafeExternalLinks(m_rendered);
    m_rendered->setMinimumHeight(250);
    m_rendered->document()->setDefaultStyleSheet(QStringLiteral(
        "body { color:#111827; line-height:1.45; }"
        "pre { background:#111827; color:#f8fafc; padding:10px; border-radius:6px; }"
        "code { font-family:Consolas,monospace; background:#f2f4f7; }"
        "a { color:#2563eb; text-decoration:underline; }"
        "blockquote { color:#475467; border-left:3px solid #98a2b3; margin-left:4px; padding-left:10px; }"
        "table { border-collapse:collapse; } td, th { border:1px solid #d0d5dd; padding:5px; }"
    ));
    m_resultStack = new QStackedWidget;
    m_resultStack->addWidget(m_rendered);
    m_resultStack->addWidget(m_editor);
    m_resultStack->setCurrentWidget(m_rendered);
    layout->addWidget(m_resultStack, 1);
    updateRenderedResult();

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
        updateRenderedResult();
        if (!m_programmaticTextChange && !m_busy) {
            m_userEdited = true;
            if (m_onLiveDraft) {
                m_onLiveDraft(m_result);
            }
        }
        updateActionState();
    });
    connect(m_renderedViewButton, &QPushButton::clicked, this, [this]() {
        syncResultFromEditor();
        updateRenderedResult();
        m_resultStack->setCurrentWidget(m_rendered);
        m_renderedViewButton->setChecked(true);
        m_editViewButton->setChecked(false);
    });
    connect(m_editViewButton, &QPushButton::clicked, this, [this]() {
        m_resultStack->setCurrentWidget(m_editor);
        m_renderedViewButton->setChecked(false);
        m_editViewButton->setChecked(true);
        m_editor->setFocus();
    });
    connect(m_detailsButton, &QPushButton::clicked, this, [this]() {
        showModelDetails();
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
    updateRenderedResult();
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
    updateRenderedResult();
    m_initialResult = m_result;
    updateActionState();
}

void ResultChoicePopup::setModelResponseDetails(
    const QByteArray &rawResponse,
    const ModelRequestTelemetry &telemetry)
{
    m_rawResponse = rawResponse;
    m_telemetry = telemetry;
    const QString telemetryKey = QString::number(
        telemetry.requestedAtUtc.toMSecsSinceEpoch()
    ) + QStringLiteral(":") + telemetry.providerId
        + QStringLiteral(":") + telemetry.modelId
        + QStringLiteral(":") + QString::number(telemetry.totalDurationMs);
    if (!telemetryKey.isEmpty()
        && telemetryKey != m_lastTelemetryKey
        && telemetry.requestedAtUtc.isValid()) {
        m_lastTelemetryKey = telemetryKey;
        if (telemetry.usage.inputTokens >= 0) {
            m_conversationInputTokens += telemetry.usage.inputTokens;
        }
        if (telemetry.usage.outputTokens >= 0) {
            m_conversationOutputTokens += telemetry.usage.outputTokens;
        }
        if (telemetry.usage.totalTokens >= 0) {
            m_conversationTotalTokens += telemetry.usage.totalTokens;
        }
        if (telemetry.estimatedCost >= 0.0) {
            m_conversationEstimatedCost += telemetry.estimatedCost;
            m_conversationHasEstimatedCost = true;
        }
    }
    if (m_detailsButton) {
        m_detailsButton->setEnabled(
            !m_rawResponse.isEmpty()
            || !m_telemetry.actualRequest.isEmpty()
            || m_telemetry.usage.hasAny()
            || !m_telemetry.finishReason.trimmed().isEmpty()
        );
    }
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
    const QRect screen = availableScreenGeometryAt(QCursor::pos());
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

void ResultChoicePopup::updateRenderedResult()
{
    if (!m_rendered) {
        return;
    }
    m_rendered->setMarkdown(m_result);
    decorateMarkdownDocument(m_rendered->document());
}

void ResultChoicePopup::showModelDetails()
{
    syncResultFromEditor();
    QDialog dialog(this);
    dialog.setWindowTitle(popupTr8("模型回答与 API 调试详情"));
    dialog.resize(880, 680);
    auto *layout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget;
    layout->addWidget(tabs, 1);

    auto *rendered = new QTextBrowser;
    configureSafeExternalLinks(rendered);
    rendered->setMarkdown(m_result);
    decorateMarkdownDocument(rendered->document());
    tabs->addTab(rendered, popupTr8("Rendered Response"));

    auto *raw = new QPlainTextEdit;
    raw->setReadOnly(true);
    QJsonParseError rawError;
    const QJsonDocument rawDocument = QJsonDocument::fromJson(
        m_rawResponse,
        &rawError
    );
    raw->setPlainText(
        rawError.error == QJsonParseError::NoError
            ? QString::fromUtf8(rawDocument.toJson(QJsonDocument::Indented))
            : QString::fromUtf8(m_rawResponse)
    );
    tabs->addTab(raw, popupTr8("Raw Response"));

    auto *request = new QPlainTextEdit;
    request->setReadOnly(true);
    request->setPlainText(QString::fromUtf8(
        QJsonDocument(m_telemetry.actualRequest)
            .toJson(QJsonDocument::Indented)
    ));
    tabs->addTab(request, popupTr8("Actual Request JSON"));

    auto *metrics = new QPlainTextEdit;
    metrics->setReadOnly(true);
    QStringList metricLines;
    metricLines << popupTr8("Provider：") + m_telemetry.providerId;
    metricLines << popupTr8("Model：") + m_telemetry.modelId;
    if (m_telemetry.httpStatusCode > 0) {
        metricLines << QStringLiteral("HTTP Status Code：")
            + QString::number(m_telemetry.httpStatusCode);
    }
    if (m_telemetry.usage.inputTokens >= 0) {
        metricLines << QStringLiteral("Input Tokens：")
            + QString::number(m_telemetry.usage.inputTokens);
    }
    if (m_telemetry.usage.outputTokens >= 0) {
        metricLines << QStringLiteral("Output Tokens：")
            + QString::number(m_telemetry.usage.outputTokens);
    }
    if (m_telemetry.usage.totalTokens >= 0) {
        metricLines << QStringLiteral("Total Tokens：")
            + QString::number(m_telemetry.usage.totalTokens);
    }
    if (m_telemetry.usage.reasoningTokens >= 0) {
        metricLines << QStringLiteral("Reasoning Tokens：")
            + QString::number(m_telemetry.usage.reasoningTokens);
    }
    if (m_telemetry.usage.cacheHitTokens >= 0) {
        metricLines << QStringLiteral("Cache Hit Tokens：")
            + QString::number(m_telemetry.usage.cacheHitTokens);
    }
    if (!m_telemetry.finishReason.trimmed().isEmpty()) {
        metricLines << QStringLiteral("Finish Reason：")
            + m_telemetry.finishReason;
    }
    if (m_telemetry.firstResponseMs >= 0) {
        metricLines << popupTr8("首次响应：")
            + QString::number(m_telemetry.firstResponseMs)
            + QStringLiteral(" ms");
    }
    if (m_telemetry.firstTokenMs >= 0) {
        metricLines << popupTr8("首 Token：")
            + QString::number(m_telemetry.firstTokenMs)
            + QStringLiteral(" ms");
    }
    if (m_telemetry.totalDurationMs >= 0) {
        metricLines << popupTr8("总耗时：")
            + QString::number(m_telemetry.totalDurationMs)
            + QStringLiteral(" ms");
    }
    if (m_telemetry.estimatedCost >= 0.0) {
        metricLines << popupTr8("本次费用估算：")
            + m_telemetry.estimatedCostCurrency
            + QStringLiteral(" ")
            + QString::number(m_telemetry.estimatedCost, 'f', 8)
            + popupTr8("（估算值）");
    }
    if (m_conversationInputTokens > 0
        || m_conversationOutputTokens > 0
        || m_conversationTotalTokens > 0) {
        metricLines << QString();
        metricLines << popupTr8("当前结果窗口累计：");
        metricLines << QStringLiteral("Input Tokens：")
            + QString::number(m_conversationInputTokens);
        metricLines << QStringLiteral("Output Tokens：")
            + QString::number(m_conversationOutputTokens);
        metricLines << QStringLiteral("Total Tokens：")
            + QString::number(m_conversationTotalTokens);
    }
    if (m_conversationHasEstimatedCost) {
        metricLines << popupTr8("累计费用估算：")
            + m_telemetry.estimatedCostCurrency
            + QStringLiteral(" ")
            + QString::number(m_conversationEstimatedCost, 'f', 8)
            + popupTr8("（估算值）");
    }
    metrics->setPlainText(metricLines.join(QStringLiteral("\n")));
    tabs->addTab(metrics, popupTr8("Token / Latency"));

    auto *sources = new QTextBrowser;
    configureSafeExternalLinks(sources);
    QString sourcesHtml;
    for (int i = 0; i < m_telemetry.citations.size(); ++i) {
        const ModelCitation &source = m_telemetry.citations.at(i);
        const QString title = source.title.trimmed().isEmpty()
            ? source.url : source.title;
        sourcesHtml += QStringLiteral(
            "<div style='margin:8px 0;padding:10px;border:1px solid #d0d5dd;border-radius:7px'>"
            "<b>%1. <a href='%2'>%3</a></b><br><span style='color:#667085'>%4</span><br>%5</div>"
        ).arg(
            QString::number(i + 1),
            source.url.toHtmlEscaped(),
            title.toHtmlEscaped(),
            source.siteName.toHtmlEscaped(),
            source.snippet.toHtmlEscaped()
        );
    }
    sources->setHtml(sourcesHtml.isEmpty()
        ? popupTr8("当前 API 响应没有返回结构化来源信息。")
        : sourcesHtml);
    tabs->addTab(sources, popupTr8("Citations / Sources"));

    auto *buttons = new QHBoxLayout;
    auto *copyCode = popupButton(popupTr8("复制代码块"), false);
    auto *copyRaw = popupButton(popupTr8("复制原始响应"), false);
    auto *close = popupButton(popupTr8("关闭"), true);
    buttons->addWidget(copyCode);
    buttons->addWidget(copyRaw);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(copyCode, &QPushButton::clicked, &dialog, [this]() {
        QStringList blocks;
        QRegularExpression expression(
            QStringLiteral("```(?:[^\\n]*)\\n([\\s\\S]*?)```")
        );
        QRegularExpressionMatchIterator matches = expression.globalMatch(m_result);
        while (matches.hasNext()) {
            blocks.append(matches.next().captured(1).trimmed());
        }
        QApplication::clipboard()->setText(
            blocks.isEmpty() ? m_result : blocks.join(QStringLiteral("\n\n"))
        );
    });
    connect(copyRaw, &QPushButton::clicked, &dialog, [this]() {
        QApplication::clipboard()->setText(QString::fromUtf8(m_rawResponse));
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
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
