#include <QtTest>

#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/selection_context_action_editor.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QFontInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

template <typename T>
T *required(QWidget *root, const char *name)
{
    T *widget = root->findChild<T *>(QString::fromLatin1(name));
    if (!widget) {
        qFatal("missing widget: %s", name);
    }
    return widget;
}

SelectionContextActionEditor::Catalogs fullCatalogs()
{
    SelectionContextActionEditor::Catalogs catalogs;
    catalogs.models
        << qMakePair(QString::fromUtf8("推荐模型"), QStringLiteral("model:recommended"))
        << qMakePair(QString::fromUtf8("备用模型"), QStringLiteral("model:backup"));
    catalogs.vocabularyScopes
        << qMakePair(QString::fromUtf8("全局词库"), QStringLiteral("__global"))
        << qMakePair(QString::fromUtf8("问答功能"), QStringLiteral("ask"));
    catalogs.targetLanguages
        << qMakePair(QString::fromUtf8("跟随全局目标语言"), QString())
        << qMakePair(QString::fromUtf8("简体中文"), QString::fromUtf8("简体中文"))
        << qMakePair(QStringLiteral("English"), QStringLiteral("English"));
    return catalogs;
}

void verifyOnly(QWidget *root, const char *present,
                const QList<const char *> &absent)
{
    QVERIFY2(root->findChild<QWidget *>(QString::fromLatin1(present)), present);
    for (const char *name : absent) {
        QVERIFY2(!root->findChild<QWidget *>(QString::fromLatin1(name)), name);
    }
}

} // namespace

class SelectionContextActionEditorTests : public QObject
{
    Q_OBJECT

private slots:
    void commonFieldsRoundTripAndCallbacksAreStable();
    void actionSpecificControlsAreCreatedOnlyWhenApplicable();
    void promptRejectsMoreThanEightThousandWithoutRecursiveWarnings();
    void unavailableAndUnicodeCatalogValuesRoundTripWithoutSilentReplacement();
    void expandedStateOnlyAffectsSpecificFields();
    void controlsDoNotClipAt100_125_150Percent();
};

void SelectionContextActionEditorTests::
commonFieldsRoundTripAndCallbacksAreStable()
{
    QVector<SelectionContextActionCustomization> changes;
    int restores = 0;
    SelectionContextActionEditor::Callbacks callbacks;
    callbacks.changed = [&](const SelectionContextActionCustomization &value) {
        changes.append(value);
    };
    callbacks.restoreRequested = [&]() { ++restores; };

    SelectionContextActionEditor editor(
        selectionContextActionAiSearch(), fullCatalogs(), callbacks);
    SelectionContextActionCustomization initial;
    initial.displayName = QString::fromUtf8("智能检索");
    initial.visible = false;
    initial.modelId = QStringLiteral("model:recommended");
    initial.promptOverride = QString::fromUtf8("只回答事实");
    editor.setCustomization(initial);
    QVERIFY(changes.isEmpty());

    QLineEdit *name = required<QLineEdit>(&editor, "selectionActionDisplayName");
    QCheckBox *visible = required<QCheckBox>(&editor, "selectionActionVisible");
    QPushButton *restore = required<QPushButton>(&editor, "selectionActionRestore");
    QToolButton *expand = required<QToolButton>(&editor, "selectionActionExpand");
    QCOMPARE(name->text(), initial.displayName);
    QCOMPARE(visible->isChecked(), false);

    name->setText(QString::fromUtf8("资料查找"));
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.last().displayName, QString::fromUtf8("资料查找"));
    QCOMPARE(changes.last().visible, false);
    visible->click();
    QCOMPARE(changes.size(), 2);
    QCOMPARE(changes.last().visible, true);
    QCOMPARE(editor.customization().displayName, QString::fromUtf8("资料查找"));

    restore->click();
    QCOMPARE(restores, 1);
    QCOMPARE(editor.customization().displayName, QString::fromUtf8("资料查找"));
    QVERIFY(!editor.isExpanded());
    expand->click();
    QVERIFY(editor.isExpanded());
}

void SelectionContextActionEditorTests::
actionSpecificControlsAreCreatedOnlyWhenApplicable()
{
    const QList<const char *> none;
    const QList<const char *> noTarget = QList<const char *>()
        << "selectionActionTargetLanguage"
        << "selectionActionVocabularyScope"
        << "selectionActionCopyMode";

    SelectionContextActionEditor search(
        selectionContextActionAiSearch(), fullCatalogs());
    verifyOnly(&search, "selectionActionModel", noTarget);
    QVERIFY(search.findChild<QTextEdit *>(QStringLiteral("selectionActionPrompt")));

    SelectionContextActionEditor explain(
        selectionContextActionExplain(), fullCatalogs());
    verifyOnly(&explain, "selectionActionModel", noTarget);
    QVERIFY(explain.findChild<QTextEdit *>(QStringLiteral("selectionActionPrompt")));

    SelectionContextActionEditor translate(
        selectionContextActionTranslate(), fullCatalogs());
    verifyOnly(&translate, "selectionActionTargetLanguage",
               QList<const char *>() << "selectionActionVocabularyScope"
                                     << "selectionActionCopyMode");
    QVERIFY(translate.findChild<QComboBox *>(QStringLiteral("selectionActionModel")));
    QVERIFY(translate.findChild<QTextEdit *>(QStringLiteral("selectionActionPrompt")));

    SelectionContextActionEditor save(
        selectionContextActionSave(), fullCatalogs());
    verifyOnly(&save, "selectionActionVocabularyScope",
               QList<const char *>() << "selectionActionModel"
                                     << "selectionActionPrompt"
                                     << "selectionActionCopyMode"
                                     << "selectionActionTargetLanguage");

    SelectionContextActionEditor copy(
        selectionContextActionCopy(), fullCatalogs());
    verifyOnly(&copy, "selectionActionCopyMode",
               QList<const char *>() << "selectionActionModel"
                                     << "selectionActionPrompt"
                                     << "selectionActionVocabularyScope"
                                     << "selectionActionTargetLanguage");

    Q_UNUSED(none);
}

void SelectionContextActionEditorTests::
promptRejectsMoreThanEightThousandWithoutRecursiveWarnings()
{
    QVector<SelectionContextActionCustomization> changes;
    QStringList warnings;
    SelectionContextActionEditor::Callbacks callbacks;
    callbacks.changed = [&](const SelectionContextActionCustomization &value) {
        changes.append(value);
    };
    callbacks.validationWarning = [&](const QString &warning) {
        warnings.append(warning);
    };
    SelectionContextActionEditor editor(
        selectionContextActionExplain(), fullCatalogs(), callbacks);
    QTextEdit *prompt = required<QTextEdit>(&editor, "selectionActionPrompt");
    QLabel *count = required<QLabel>(&editor, "selectionActionPromptCount");

    const QString valid(8000, QLatin1Char('a'));
    prompt->setPlainText(valid);
    QCOMPARE(editor.customization().promptOverride, valid);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(warnings.size(), 0);
    QVERIFY(count->text().contains(QStringLiteral("8000")));

    prompt->setPlainText(valid + QLatin1Char('b'));
    QCOMPARE(prompt->toPlainText(), valid);
    QCOMPARE(editor.customization().promptOverride, valid);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(warnings.size(), 1);
    QVERIFY(count->text().contains(QStringLiteral("8000")));

    prompt->setPlainText(QString::fromUtf8("新的有效提示词"));
    QCOMPARE(editor.customization().promptOverride,
             QString::fromUtf8("新的有效提示词"));
    QCOMPARE(changes.size(), 2);
}

void SelectionContextActionEditorTests::
unavailableAndUnicodeCatalogValuesRoundTripWithoutSilentReplacement()
{
    SelectionContextActionEditor search(
        selectionContextActionAiSearch(), fullCatalogs());
    SelectionContextActionCustomization searchValue;
    searchValue.modelId = QStringLiteral("provider:retired-model");
    search.setCustomization(searchValue);
    QComboBox *model = required<QComboBox>(&search, "selectionActionModel");
    QCOMPARE(model->currentData().toString(), searchValue.modelId);
    QVERIFY(model->currentText().contains(QString::fromUtf8("不可用")));
    QCOMPARE(search.customization().modelId, searchValue.modelId);

    searchValue.modelId.clear();
    search.setCustomization(searchValue);
    QCOMPARE(model->currentData().toString(), QString());
    QVERIFY(!model->currentText().contains(QString::fromUtf8("不可用")));

    SelectionContextActionEditor save(
        selectionContextActionSave(), fullCatalogs());
    SelectionContextActionCustomization saveValue;
    saveValue.vocabularyScopeId = QString::fromUtf8("function:已删除功能");
    save.setCustomization(saveValue);
    QComboBox *scope = required<QComboBox>(&save, "selectionActionVocabularyScope");
    QCOMPARE(scope->currentData().toString(), saveValue.vocabularyScopeId);
    QVERIFY(scope->currentText().contains(QString::fromUtf8("不可用")));
    QCOMPARE(save.customization().vocabularyScopeId,
             saveValue.vocabularyScopeId);

    SelectionContextActionEditor translate(
        selectionContextActionTranslate(), fullCatalogs());
    SelectionContextActionCustomization translateValue;
    translateValue.modelId = QStringLiteral("model:backup");
    translateValue.promptOverride = QString::fromUtf8("用自然中文翻译");
    translateValue.targetLanguage = QString::fromUtf8("克林贡语");
    translate.setCustomization(translateValue);
    QComboBox *language = required<QComboBox>(
        &translate, "selectionActionTargetLanguage");
    QVERIFY(language->isEditable());
    QCOMPARE(language->currentText(), translateValue.targetLanguage);
    QCOMPARE(translate.customization().targetLanguage,
             translateValue.targetLanguage);

    language->setCurrentIndex(language->findData(QStringLiteral("English")));
    QCOMPARE(translate.customization().targetLanguage, QStringLiteral("English"));
    language->setEditText(QString::fromUtf8("繁體中文"));
    QCOMPARE(translate.customization().targetLanguage,
             QString::fromUtf8("繁體中文"));
}

void SelectionContextActionEditorTests::
expandedStateOnlyAffectsSpecificFields()
{
    SelectionContextActionEditor editor(
        selectionContextActionTranslate(), fullCatalogs());
    QLineEdit *name = required<QLineEdit>(&editor, "selectionActionDisplayName");
    QWidget *specific = required<QWidget>(&editor, "selectionActionSpecificFields");
    QVERIFY(name->isVisibleTo(&editor));
    QVERIFY(!specific->isVisibleTo(&editor));

    editor.setExpanded(true);
    QVERIFY(editor.isExpanded());
    QVERIFY(name->isVisibleTo(&editor));
    QVERIFY(specific->isVisibleTo(&editor));
    editor.setExpanded(false);
    QVERIFY(!editor.isExpanded());
    QVERIFY(name->isVisibleTo(&editor));
    QVERIFY(!specific->isVisibleTo(&editor));
}

void SelectionContextActionEditorTests::
controlsDoNotClipAt100_125_150Percent()
{
#ifdef Q_OS_WIN
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("windows"));
#endif
    const QFont originalFont = QApplication::font();
    const QVector<int> scales = QVector<int>() << 100 << 125 << 150;
    for (int scale : scales) {
        QFont font(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(qMax(12, (14 * scale) / 100));
        QApplication::setFont(font);

        SelectionContextActionEditor *editor =
            new SelectionContextActionEditor(
                selectionContextActionTranslate(), fullCatalogs());
        SelectionContextActionCustomization value;
        value.displayName = QString::fromUtf8("这是二十四字符以内的超长中文自定义动作名称");
        value.modelId = QStringLiteral("provider:unavailable-extra-long-model-name");
        value.promptOverride = QString::fromUtf8("请保留术语并给出准确自然的中文翻译。");
        value.targetLanguage = QString::fromUtf8("超长的自定义目标语言名称");
        editor->setCustomization(value);
        editor->setExpanded(true);

        QWidget host;
        QVBoxLayout layout(&host);
        layout.addWidget(editor);
        host.resize(qMax(760, (760 * scale) / 100),
                    qMax(520, (520 * scale) / 100));
        host.show();
        QTest::qWait(20);
        host.adjustSize();
        QCoreApplication::processEvents();

#ifdef Q_OS_WIN
        const QFontMetrics metrics(editor->font());
        QVERIFY2(metrics.width(QString::fromUtf8("中文控件文字")) > 40,
                 "Windows native visual gate requires renderable CJK glyphs");
        QVERIFY2(QFontInfo(editor->font()).family().contains(
                     QString::fromUtf8("雅黑"), Qt::CaseInsensitive)
                     || QFontInfo(editor->font()).family().contains(
                         QStringLiteral("YaHei"), Qt::CaseInsensitive),
                 "Windows native visual gate requires Microsoft YaHei UI");
#endif
        const QList<QLabel *> labels = editor->findChildren<QLabel *>();
        for (QLabel *label : labels) {
            if (!label->isVisible() || label->text().trimmed().isEmpty()) {
                continue;
            }
            QVERIFY2(label->height() >= label->sizeHint().height(),
                     qPrintable(QStringLiteral("scale=%1 label=%2 height=%3 hint=%4")
                         .arg(scale).arg(label->objectName())
                         .arg(label->height()).arg(label->sizeHint().height())));
        }
        const QList<QAbstractButton *> buttons =
            editor->findChildren<QAbstractButton *>();
        for (QAbstractButton *button : buttons) {
            if (!button->isVisible()) {
                continue;
            }
            QVERIFY(button->height() >= button->sizeHint().height());
            QCOMPARE(button->maximumHeight(), QWIDGETSIZE_MAX);
            QVERIFY2(button->styleSheet().contains(QStringLiteral("padding: 0")),
                     qPrintable(button->objectName()));
        }
        const QList<QLineEdit *> lines = editor->findChildren<QLineEdit *>();
        for (QLineEdit *line : lines) {
            if (line->isVisible()
                && line->objectName().startsWith(QStringLiteral("selectionAction"))) {
                QVERIFY(line->height() >= line->sizeHint().height());
            }
        }
        const QList<QComboBox *> combos = editor->findChildren<QComboBox *>();
        for (QComboBox *combo : combos) {
            if (combo->isVisible()) {
                QVERIFY(combo->height() >= combo->sizeHint().height());
            }
        }
        QTextEdit *prompt = required<QTextEdit>(editor, "selectionActionPrompt");
        QVERIFY(prompt->height() >= prompt->sizeHint().height());
    }
    QApplication::setFont(originalFont);
}

QTEST_MAIN(SelectionContextActionEditorTests)

#include "selection_context_action_editor_tests.moc"
