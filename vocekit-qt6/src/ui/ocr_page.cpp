#include "ocr_page.h"

#include "ui_style.h"

#include "../ocr/ocr_types.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

OcrPage::OcrPage(
    int configuredEngine,
    const OcrPageCallbacks &callbacks,
    QWidget *parent
)
    : QWidget(parent), m_callbacks(callbacks)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto *title = new QLabel(tr8("图片识别"));
    title->setFont(appFont(24, QFont::DemiBold));
    layout->addWidget(title);

    auto *sourceRow = new QHBoxLayout;
    sourceRow->setSpacing(10);

    m_imagePathEdit = new QLineEdit;
    m_imagePathEdit->setReadOnly(true);
    m_imagePathEdit->setMinimumHeight(40);
    m_imagePathEdit->setPlaceholderText(tr8("可以一次选择多张图片"));
    m_imagePathEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #ffffff; border: 1px solid #d0d5dd;"
        " border-radius: 7px; padding: 0 12px; color: #344054; }"
    ));

    m_engineBox = new QComboBox;
    m_engineBox->setMinimumSize(180, 40);
    m_engineBox->addItem(tr8("自动选择"), int(OcrEngine::Automatic));
    m_engineBox->addItem(QStringLiteral("RapidOCR"), int(OcrEngine::RapidOcr));
    m_engineBox->addItem(QStringLiteral("Windows OCR"), int(OcrEngine::WindowsOcr));
    m_engineBox->addItem(tr8("自定义云 OCR"), int(OcrEngine::CustomCloud));
    const int configuredEngineIndex = m_engineBox->findData(configuredEngine);
    m_engineBox->setCurrentIndex(configuredEngineIndex >= 0 ? configuredEngineIndex : 0);

    m_selectButton = new QPushButton(tr8("选择图片"));
    m_selectButton->setMinimumSize(106, 40);
    m_selectButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));

    m_startButton = new QPushButton(tr8("开始识别"));
    m_startButton->setMinimumSize(106, 40);
    m_startButton->setStyleSheet(buttonStyle(QStringLiteral("#111827")));

    m_cancelButton = new QPushButton(tr8("取消"));
    m_cancelButton->setMinimumSize(80, 40);
    m_cancelButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#b42318")));
    m_cancelButton->setEnabled(false);

    sourceRow->addWidget(m_imagePathEdit, 1);
    sourceRow->addWidget(m_engineBox);
    sourceRow->addWidget(m_selectButton);
    sourceRow->addWidget(m_startButton);
    sourceRow->addWidget(m_cancelButton);
    layout->addLayout(sourceRow);

    auto *body = new QHBoxLayout;
    body->setSpacing(14);

    auto *previewFrame = new QFrame;
    previewFrame->setObjectName(QStringLiteral("card"));
    previewFrame->setStyleSheet(cardStyle());
    previewFrame->setMinimumWidth(310);
    previewFrame->setMaximumWidth(390);
    auto *previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(14, 14, 14, 14);
    previewLayout->setSpacing(10);

    auto *previewTitle = new QLabel(tr8("图片预览"));
    previewTitle->setFont(appFont(13, QFont::DemiBold));
    previewLayout->addWidget(previewTitle);

    m_previewLabel = new QLabel(tr8("尚未选择图片"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(280, 260);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #f2f4f7; color: #667085; border: 1px solid #e4e7ec;"
        " border-radius: 7px; padding: 12px; }"
    ));
    previewLayout->addWidget(m_previewLabel, 1);

    auto *navigation = new QHBoxLayout;
    navigation->setSpacing(8);
    m_previousButton = new QPushButton(tr8("上一张"));
    m_previousButton->setMinimumHeight(34);
    m_previousButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    m_positionLabel = new QLabel(tr8("0 / 0"));
    m_positionLabel->setAlignment(Qt::AlignCenter);
    m_positionLabel->setMinimumWidth(86);
    m_positionLabel->setStyleSheet(QStringLiteral("color: #344054; font-weight: 600;"));
    m_nextButton = new QPushButton(tr8("下一张"));
    m_nextButton->setMinimumHeight(34);
    m_nextButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    navigation->addWidget(m_previousButton);
    navigation->addWidget(m_positionLabel, 1);
    navigation->addWidget(m_nextButton);
    previewLayout->addLayout(navigation);

    m_statusLabel = new QLabel(tr8("等待识别"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setMinimumHeight(38);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { background: #eef2ff; color: #1d4ed8; border-radius: 7px;"
        " font-weight: 600; padding: 8px; }"
    ));
    previewLayout->addWidget(m_statusLabel);

    auto *resultFrame = new QFrame;
    resultFrame->setObjectName(QStringLiteral("card"));
    resultFrame->setStyleSheet(cardStyle());
    auto *resultLayout = new QVBoxLayout(resultFrame);
    resultLayout->setContentsMargins(16, 14, 16, 14);
    resultLayout->setSpacing(10);

    auto *resultTop = new QHBoxLayout;
    auto *resultTitle = new QLabel(tr8("识别结果"));
    resultTitle->setFont(appFont(13, QFont::DemiBold));
    auto *copyButton = new QPushButton(tr8("复制"));
    copyButton->setMinimumSize(78, 36);
    copyButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    resultTop->addWidget(resultTitle, 1);
    resultTop->addWidget(copyButton);
    resultLayout->addLayout(resultTop);

    m_resultEdit = new QTextEdit;
    m_resultEdit->setAcceptRichText(false);
    m_resultEdit->setPlaceholderText(tr8("识别完成后会在这里显示文字，可以直接修改。"));
    m_resultEdit->setStyleSheet(QStringLiteral(
        "QTextEdit { background: #ffffff; border: 1px solid #d0d5dd;"
        " border-radius: 7px; padding: 12px; }"
    ));
    resultLayout->addWidget(m_resultEdit, 1);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    const QVector<QPair<QString, QString>> actionDefs = {
        { QStringLiteral("organize"), tr8("智能整理") },
        { QStringLiteral("translate"), tr8("翻译") },
        { QStringLiteral("polish"), tr8("润色") },
        { QStringLiteral("summarize"), tr8("总结") },
        { QStringLiteral("ask"), tr8("问答") }
    };
    for (const auto &definition : actionDefs) {
        auto *button = new QPushButton(definition.second);
        button->setMinimumHeight(38);
        button->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(button, &QPushButton::clicked, this, [this, definition]() {
            if (m_callbacks.aiAction) {
                m_callbacks.aiAction(definition.first);
            }
        });
        actions->addWidget(button);
        m_aiButtons.append(button);
    }
    actions->addStretch();
    resultLayout->addLayout(actions);

    body->addWidget(previewFrame);
    body->addWidget(resultFrame, 1);
    layout->addLayout(body, 1);

    connect(m_selectButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.selectImages) {
            m_callbacks.selectImages();
        }
    });
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.startRecognition) {
            m_callbacks.startRecognition();
        }
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.cancelRecognition) {
            m_callbacks.cancelRecognition();
        }
    });
    connect(m_previousButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.previousImage) {
            m_callbacks.previousImage();
        }
    });
    connect(m_nextButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.nextImage) {
            m_callbacks.nextImage();
        }
    });
    connect(m_resultEdit, &QTextEdit::textChanged, this, [this]() {
        if (m_callbacks.resultTextChanged && m_resultEdit) {
            m_callbacks.resultTextChanged(m_resultEdit->toPlainText());
        }
    });
    connect(copyButton, &QPushButton::clicked, this, [this]() {
        const QString text = m_resultEdit ? m_resultEdit->toPlainText() : QString();
        if (!text.trimmed().isEmpty()) {
            QApplication::clipboard()->setText(text);
        }
    });
}
