#include <QtTest>

#define private public
#include "../../src/ui/result_choice_popup.h"
#undef private

#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTimer>

#include <type_traits>

namespace {

QPushButton *buttonWithText(ResultChoicePopup *popup, const QString &text)
{
    const QList<QPushButton *> buttons = popup->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

} // namespace

class ResultChoicePopupHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void exposesWidgetType();
    void acceptsWindowPreferenceSnapshot();
    void appliesConfiguredActionVisibility();
    void clampsConfiguredOpacity();
    void reportsTheResolvedActionOnce();
    void closingBusyPopupRequestsCancellation();
    void retryDialogPreservesCurrentModelOutsideBootstrapCatalog();
    void rendersMarkdownAndKeepsEditableSource();
    void acceptsRawResponseAndTelemetryDetails();
    void rendersDenseRichResponseAtLargeFont();
};

void ResultChoicePopupHeaderTests::exposesWidgetType()
{
    QVERIFY((std::is_base_of<QWidget, ResultChoicePopup>::value));
}

void ResultChoicePopupHeaderTests::acceptsWindowPreferenceSnapshot()
{
    QVERIFY((std::is_constructible<
        ResultChoicePopup,
        const ResultPopupWindowPreferences &,
        const QString &,
        const QString &,
        ClipboardWindowHandle,
        bool,
        int,
        QWidget *
    >::value));
}

void ResultChoicePopupHeaderTests::appliesConfiguredActionVisibility()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        true,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    popup.setActionOrder(
        QStringList()
            << QStringLiteral("write")
            << QStringLiteral("copy")
            << QStringLiteral("write")
            << QStringLiteral("unsupported")
    );

    QPushButton *write = buttonWithText(&popup, QString::fromUtf8("写入"));
    QPushButton *copy = buttonWithText(&popup, QString::fromUtf8("复制"));
    QPushButton *replace = buttonWithText(&popup, QString::fromUtf8("替换选中"));
    QPushButton *close = buttonWithText(&popup, QString::fromUtf8("关闭"));
    QVERIFY(write);
    QVERIFY(copy);
    QVERIFY(replace);
    QVERIFY(close);
    QVERIFY(!write->isHidden());
    QVERIFY(!copy->isHidden());
    QVERIFY(replace->isHidden());
    QVERIFY(!close->isHidden());
}

void ResultChoicePopupHeaderTests::clampsConfiguredOpacity()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    popup.setOpacityPercent(20);
    QVERIFY(qAbs(popup.windowOpacity() - 0.60) < 0.02);
    popup.setOpacityPercent(95);
    QVERIFY(qAbs(popup.windowOpacity() - 0.95) < 0.02);
    popup.setOpacityPercent(120);
    QVERIFY(qAbs(popup.windowOpacity() - 1.00) < 0.02);
}

void ResultChoicePopupHeaderTests::reportsTheResolvedActionOnce()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    QStringList resolvedActions;
    popup.setResolvedCallback([&resolvedActions](const QString &action) {
        resolvedActions.append(action);
    });

    QPushButton *close = buttonWithText(&popup, QString::fromUtf8("关闭"));
    QVERIFY(close);
    close->click();
    popup.close();

    QCOMPARE(resolvedActions, QStringList() << QStringLiteral("close"));
}

void ResultChoicePopupHeaderTests::closingBusyPopupRequestsCancellation()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    int cancellationCount = 0;
    popup.setCancellationCallback([&cancellationCount]() {
        ++cancellationCount;
    });
    popup.setBusy(true);

    QVERIFY(popup.close());
    QCOMPARE(cancellationCount, 1);
}

void ResultChoicePopupHeaderTests::
retryDialogPreservesCurrentModelOutsideBootstrapCatalog()
{
    ResultPopupWindowPreferences preferences;
    ResultChoicePopup popup(
        preferences,
        QStringLiteral("test"),
        QStringLiteral("result"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);
    popup.setActionCallbacks(
        std::function<void()>(),
        [](const QString &) {},
        std::function<void(const QString &)>()
    );
    popup.setCurrentModel(QStringLiteral("openai:gpt-5.4"));

    QString selectedModel;
    int oldModelIndex = -2;
    bool dialogFound = false;
    QTimer::singleShot(0, [&]() {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            QDialog *dialog = qobject_cast<QDialog *>(widget);
            if (!dialog || !dialog->isVisible()) {
                continue;
            }
            dialogFound = true;
            QComboBox *models = dialog->findChild<QComboBox *>();
            if (models) {
                selectedModel = models->currentData().toString();
                oldModelIndex = models->findData(
                    QStringLiteral("openai:gpt-5.4")
                );
            }
            dialog->reject();
            break;
        }
    });

    popup.chooseModelAndRetry();
    QVERIFY(dialogFound);
    QCOMPARE(selectedModel, QStringLiteral("openai:gpt-5.4"));
    QVERIFY(selectedModel != QStringLiteral("deepseek-v4-flash"));
    QVERIFY(oldModelIndex >= 0);
}

void ResultChoicePopupHeaderTests::rendersMarkdownAndKeepsEditableSource()
{
    ResultChoicePopup popup(
        ResultPopupWindowPreferences(),
        QStringLiteral("test"),
        QStringLiteral("# 标题\n\n**粗体** [OpenAI](https://openai.com)\n\n```cpp\nint x = 1;\n```"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);

    QVERIFY(popup.m_rendered);
    QVERIFY(popup.m_resultStack);
    QCOMPARE(popup.m_resultStack->currentWidget(), popup.m_rendered);
    QVERIFY(popup.m_rendered->toPlainText().contains(QStringLiteral("标题")));
    QVERIFY(popup.m_rendered->toPlainText().contains(QStringLiteral("int x = 1;")));
    QVERIFY(!popup.m_rendered->openExternalLinks());
    QVERIFY(!popup.m_rendered->openLinks());
    QVERIFY(popup.resultText().contains(QStringLiteral("**粗体**")));
}

void ResultChoicePopupHeaderTests::acceptsRawResponseAndTelemetryDetails()
{
    ResultChoicePopup popup(
        ResultPopupWindowPreferences(),
        QStringLiteral("test"),
        QStringLiteral("answer"),
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);
    ModelRequestTelemetry telemetry;
    telemetry.requestedAtUtc = QDateTime::currentDateTimeUtc();
    telemetry.providerId = QStringLiteral("openai");
    telemetry.modelId = QStringLiteral("vendor-model");
    telemetry.actualRequest.insert(QStringLiteral("temperature"), 0.7);
    telemetry.httpStatusCode = 200;
    telemetry.usage.inputTokens = 10;
    telemetry.usage.outputTokens = 5;
    telemetry.usage.totalTokens = 15;
    telemetry.finishReason = QStringLiteral("stop");
    telemetry.totalDurationMs = 321;

    popup.setModelResponseDetails(
        QByteArrayLiteral("{\"ok\":true}"),
        telemetry
    );

    QCOMPARE(popup.m_rawResponse, QByteArrayLiteral("{\"ok\":true}"));
    QCOMPARE(popup.m_telemetry.httpStatusCode, 200);
    QCOMPARE(popup.m_conversationTotalTokens, qint64(15));
    QVERIFY(popup.m_detailsButton->isEnabled());
}

void ResultChoicePopupHeaderTests::rendersDenseRichResponseAtLargeFont()
{
    const QString markdown = QString::fromUtf8(
        "# 富文本回答\n\n"
        "这里有 **粗体**、*斜体*、`inline code` 和 [OpenAI](https://openai.com)。\n\n"
        "> 这是一段引用，用于确认长中文在放大字体下仍然完整。\n\n"
        "1. 有序项目\n2. 第二项\n\n- 无序项目\n- 另一项\n\n"
        "| 字段 | 数值 |\n|---|---:|\n| Input Tokens | 120 |\n| Output Tokens | 80 |\n\n"
        "公式：$E = mc^2$\n\n$$\\int_0^1 x^2 dx = \\frac{1}{3}$$\n\n"
        "```cpp\nconst int answer = 42; // highlighted\n```"
    );
    ResultChoicePopup popup(
        ResultPopupWindowPreferences(),
        QString::fromUtf8("模型回答"),
        markdown,
        nullptr,
        false,
        0
    );
    popup.setAttribute(Qt::WA_DeleteOnClose, false);
    QFont largeFont = popup.font();
    largeFont.setPointSize(qMax(12, largeFont.pointSize() + 2));
    popup.setFont(largeFont);
    popup.resize(820, 680);
    popup.show();
    QApplication::processEvents();

    const QString html = popup.m_rendered->document()->toHtml();
    QVERIFY(html.contains(QStringLiteral("https://openai.com")));
    QVERIFY(html.contains(QStringLiteral("<table")));
    QVERIFY(popup.m_rendered->toPlainText().contains(QStringLiteral("E = mc²")));
    QVERIFY(popup.m_rendered->document()
        ->property("vocekitMarkdownHighlighter").toBool());

    const QString visualOutputDir = QString::fromLocal8Bit(
        qgetenv("VOCEKIT_VISUAL_OUTPUT_DIR")
    ).trimmed();
    if (!visualOutputDir.isEmpty()) {
        QVERIFY(QDir().mkpath(visualOutputDir));
        QVERIFY(popup.grab().save(
            QDir(visualOutputDir).filePath(
                QStringLiteral("rich-model-response-large-font.png")
            )
        ));
    }
    popup.close();
}

QTEST_MAIN(ResultChoicePopupHeaderTests)

#include "result_choice_popup_header_tests.moc"
