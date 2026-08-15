#include <QtTest>

#include "../../src/domain/selection_context_actions.h"
#include "../../src/ui/selection_context_action_editor.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFontInfo>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRawFont>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstdlib>

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

bool hasDarkTextPixels(
    const QPixmap &pixmap,
    int leftInset,
    int topInset,
    int rightInset,
    int bottomInset
)
{
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    const int left = qBound(0, leftInset, image.width());
    const int top = qBound(0, topInset, image.height());
    const int right = qBound(left, image.width() - rightInset, image.width());
    const int bottom = qBound(top, image.height() - bottomInset, image.height());
    int dark = 0;
    for (int y = top; y < bottom; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = left; x < right; ++x) {
            const QRgb pixel = line[x];
            if (qAlpha(pixel) > 100
                && qRed(pixel) < 120
                && qGreen(pixel) < 130
                && qBlue(pixel) < 150
                && ++dark >= 16) {
                return true;
            }
        }
    }
    return false;
}

const quint32 kDeletionProbeAlive = 0x51ec7100u;

class ChangedDeleteProbe
{
public:
    ChangedDeleteProbe(SelectionContextActionEditor **editor, int *calls)
        : editor_(editor), calls_(calls) {}
    ChangedDeleteProbe(const ChangedDeleteProbe &other)
        : editor_(other.editor_), calls_(other.calls_) {}
    ~ChangedDeleteProbe() { alive_ = 0; }

    void operator()(const SelectionContextActionCustomization &)
    {
        ++(*calls_);
        SelectionContextActionEditor *victim = *editor_;
        *editor_ = nullptr;
        delete victim;
        if (alive_ != kDeletionProbeAlive) {
            std::abort();
        }
    }

private:
    SelectionContextActionEditor **editor_;
    int *calls_;
    volatile quint32 alive_ = kDeletionProbeAlive;
};

class RestoreDeleteProbe
{
public:
    RestoreDeleteProbe(SelectionContextActionEditor **editor, int *calls)
        : editor_(editor), calls_(calls) {}
    RestoreDeleteProbe(const RestoreDeleteProbe &other)
        : editor_(other.editor_), calls_(other.calls_) {}
    ~RestoreDeleteProbe() { alive_ = 0; }

    void operator()()
    {
        ++(*calls_);
        SelectionContextActionEditor *victim = *editor_;
        *editor_ = nullptr;
        delete victim;
        if (alive_ != kDeletionProbeAlive) {
            std::abort();
        }
    }

private:
    SelectionContextActionEditor **editor_;
    int *calls_;
    volatile quint32 alive_ = kDeletionProbeAlive;
};

class WarningDeleteProbe
{
public:
    WarningDeleteProbe(SelectionContextActionEditor **editor, int *calls)
        : editor_(editor), calls_(calls) {}
    WarningDeleteProbe(const WarningDeleteProbe &other)
        : editor_(other.editor_), calls_(other.calls_) {}
    ~WarningDeleteProbe() { alive_ = 0; }

    void operator()(const QString &)
    {
        ++(*calls_);
        SelectionContextActionEditor *victim = *editor_;
        *editor_ = nullptr;
        delete victim;
        if (alive_ != kDeletionProbeAlive) {
            std::abort();
        }
    }

private:
    SelectionContextActionEditor **editor_;
    int *calls_;
    volatile quint32 alive_ = kDeletionProbeAlive;
};

int runDeletionWorker(const QString &kind)
{
    int calls = 0;
    SelectionContextActionEditor *editor = nullptr;
    SelectionContextActionEditor::Callbacks callbacks;
    if (kind == QStringLiteral("changed")) {
        callbacks.changed = ChangedDeleteProbe(&editor, &calls);
    } else if (kind == QStringLiteral("restore")) {
        callbacks.restoreRequested = RestoreDeleteProbe(&editor, &calls);
    } else if (kind == QStringLiteral("warning")
               || kind == QStringLiteral("set-warning")) {
        callbacks.validationWarning = WarningDeleteProbe(&editor, &calls);
    } else {
        return 30;
    }

    editor = new SelectionContextActionEditor(
        selectionContextActionAiSearch(), fullCatalogs(), callbacks);
    QPointer<SelectionContextActionEditor> guard(editor);
    if (kind == QStringLiteral("changed")) {
        required<QLineEdit>(editor, "selectionActionDisplayName")
            ->setText(QString::fromUtf8("删除编辑器"));
    } else if (kind == QStringLiteral("restore")) {
        required<QPushButton>(editor, "selectionActionRestore")->click();
    } else if (kind == QStringLiteral("warning")) {
        required<QPlainTextEdit>(editor, "selectionActionPrompt")
            ->setPlainText(QString(8001, QLatin1Char('x')));
    } else {
        SelectionContextActionCustomization invalid;
        invalid.promptOverride = QString(8001, QLatin1Char('x'));
        editor->setCustomization(invalid);
    }

    QTest::qWait(1);
    if (editor) {
        delete editor;
        editor = nullptr;
    }
    return guard.isNull() && calls == 1 ? 0 : 31;
}

struct WorkerResult
{
    bool started = false;
    bool finished = false;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    int exitCode = -1;
    QByteArray output;
};

WorkerResult runDeletionWorkerProcess(const QString &kind)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("VOCEKIT_ACTION_EDITOR_DELETE_WORKER"), kind);
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QCoreApplication::applicationFilePath(), QStringList());

    WorkerResult result;
    result.started = process.waitForStarted(5000);
    if (result.started) {
        result.finished = process.waitForFinished(10000);
    }
    if (!result.finished && result.started) {
        process.kill();
        process.waitForFinished(5000);
    }
    result.exitStatus = process.exitStatus();
    result.exitCode = process.exitCode();
    result.output = process.readAll();
    return result;
}

void flushQueuedCallbacks()
{
    QTest::qWait(1);
}

void verifyDeletionWorkerResult(const QString &kind)
{
    const WorkerResult result = runDeletionWorkerProcess(kind);
    const QByteArray diagnostic = QByteArray("worker=")
        + kind.toUtf8() + QByteArray(" output=") + result.output;
    QVERIFY2(result.started, diagnostic.constData());
    QVERIFY2(result.finished, diagnostic.constData());
    QVERIFY2(result.exitStatus == QProcess::NormalExit,
             diagnostic.constData());
    QVERIFY2(result.exitCode == 0, diagnostic.constData());
}

} // namespace

class SelectionContextActionEditorTests : public QObject
{
    Q_OBJECT

private slots:
    void commonFieldsRoundTripAndCallbacksAreStable();
    void actionSpecificControlsAreCreatedOnlyWhenApplicable();
    void promptRejectsMoreThanEightThousandWithoutRecursiveWarnings();
    void setCustomizationRejectsOverlongPromptWithoutTruncation();
    void changedCallbackMayDeleteEditorSynchronously();
    void restoreCallbackMayDeleteEditorSynchronously();
    void warningCallbackMayDeleteEditorAfterRollback();
    void setCustomizationWarningMayDeleteEditorSynchronously();
    void rejectedPastePreservesPriorUndoAndCursor();
    void commonControlsExposeAccessibleRelationshipsAndExpandedState();
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
    flushQueuedCallbacks();
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.last().displayName, QString::fromUtf8("资料查找"));
    QCOMPARE(changes.last().visible, false);
    visible->click();
    flushQueuedCallbacks();
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
    QVERIFY(search.findChild<QPlainTextEdit *>(
        QStringLiteral("selectionActionPrompt")));

    SelectionContextActionEditor explain(
        selectionContextActionExplain(), fullCatalogs());
    verifyOnly(&explain, "selectionActionModel", noTarget);
    QVERIFY(explain.findChild<QPlainTextEdit *>(
        QStringLiteral("selectionActionPrompt")));

    SelectionContextActionEditor translate(
        selectionContextActionTranslate(), fullCatalogs());
    verifyOnly(&translate, "selectionActionTargetLanguage",
               QList<const char *>() << "selectionActionVocabularyScope"
                                     << "selectionActionCopyMode");
    QVERIFY(translate.findChild<QComboBox *>(QStringLiteral("selectionActionModel")));
    QVERIFY(translate.findChild<QPlainTextEdit *>(
        QStringLiteral("selectionActionPrompt")));

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
    QPlainTextEdit *prompt = required<QPlainTextEdit>(
        &editor, "selectionActionPrompt");
    QLabel *count = required<QLabel>(&editor, "selectionActionPromptCount");

    const QString valid(8000, QLatin1Char('a'));
    prompt->setPlainText(valid);
    flushQueuedCallbacks();
    QCOMPARE(editor.customization().promptOverride, valid);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(warnings.size(), 0);
    QVERIFY(count->text().contains(QStringLiteral("8000")));

    prompt->setPlainText(valid + QLatin1Char('b'));
    flushQueuedCallbacks();
    QCOMPARE(prompt->toPlainText(), valid);
    QCOMPARE(editor.customization().promptOverride, valid);
    QCOMPARE(changes.size(), 1);
    QCOMPARE(warnings.size(), 1);
    QVERIFY(count->text().contains(QStringLiteral("8000")));

    prompt->setPlainText(QString::fromUtf8("新的有效提示词"));
    flushQueuedCallbacks();
    QCOMPARE(editor.customization().promptOverride,
             QString::fromUtf8("新的有效提示词"));
    QCOMPARE(changes.size(), 2);
}

void SelectionContextActionEditorTests::
setCustomizationRejectsOverlongPromptWithoutTruncation()
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
    SelectionContextActionCustomization accepted;
    accepted.displayName = QString::fromUtf8("解释");
    accepted.promptOverride = QString::fromUtf8("已接受提示词");
    editor.setCustomization(accepted);

    SelectionContextActionCustomization rejected = accepted;
    rejected.displayName = QString::fromUtf8("新的解释名称");
    rejected.promptOverride = QString(8001, QLatin1Char('x'));
    editor.setCustomization(rejected);
    QPlainTextEdit *prompt = required<QPlainTextEdit>(
        &editor, "selectionActionPrompt");
    QLabel *count = required<QLabel>(&editor, "selectionActionPromptCount");
    QCOMPARE(prompt->toPlainText(), accepted.promptOverride);
    QCOMPARE(editor.customization().promptOverride, accepted.promptOverride);
    QCOMPARE(count->text(), QStringLiteral("%1 / 8000")
        .arg(accepted.promptOverride.size()));
    QCOMPARE(changes.size(), 0);
    QCOMPARE(warnings.size(), 1);

    required<QLineEdit>(&editor, "selectionActionDisplayName")
        ->setText(QString::fromUtf8("随后修改名称"));
    flushQueuedCallbacks();
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.last().promptOverride, accepted.promptOverride);

    QStringList firstWarnings;
    SelectionContextActionEditor::Callbacks firstCallbacks;
    firstCallbacks.validationWarning = [&](const QString &warning) {
        firstWarnings.append(warning);
    };
    SelectionContextActionEditor first(
        selectionContextActionAiSearch(), fullCatalogs(), firstCallbacks);
    SelectionContextActionCustomization firstRejected;
    firstRejected.promptOverride = QString(8001, QLatin1Char('y'));
    first.setCustomization(firstRejected);
    QCOMPARE(first.customization().promptOverride, QString());
    QCOMPARE(firstWarnings.size(), 1);
}

void SelectionContextActionEditorTests::
changedCallbackMayDeleteEditorSynchronously()
{
    verifyDeletionWorkerResult(QStringLiteral("changed"));
}

void SelectionContextActionEditorTests::
restoreCallbackMayDeleteEditorSynchronously()
{
    verifyDeletionWorkerResult(QStringLiteral("restore"));
}

void SelectionContextActionEditorTests::
warningCallbackMayDeleteEditorAfterRollback()
{
    verifyDeletionWorkerResult(QStringLiteral("warning"));
}

void SelectionContextActionEditorTests::
setCustomizationWarningMayDeleteEditorSynchronously()
{
    verifyDeletionWorkerResult(QStringLiteral("set-warning"));
}

void SelectionContextActionEditorTests::
rejectedPastePreservesPriorUndoAndCursor()
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
    SelectionContextActionCustomization initial;
    initial.promptOverride = QString(7999, QLatin1Char('a'));
    editor.setCustomization(initial);
    QPlainTextEdit *prompt = required<QPlainTextEdit>(
        &editor, "selectionActionPrompt");
    QTextCursor cursor = prompt->textCursor();
    cursor.movePosition(QTextCursor::End);
    prompt->setTextCursor(cursor);

    QTest::keyClick(prompt, Qt::Key_B);
    flushQueuedCallbacks();
    QCOMPARE(prompt->toPlainText().size(), 8000);
    QCOMPARE(changes.size(), 1);
    cursor = prompt->textCursor();
    cursor.movePosition(QTextCursor::Start);
    prompt->setTextCursor(cursor);
    cursor.movePosition(QTextCursor::End);
    prompt->setTextCursor(cursor);

    QApplication::clipboard()->setText(QStringLiteral("c"));
    prompt->paste();
    flushQueuedCallbacks();
    QCOMPARE(prompt->toPlainText().size(), 8000);
    QCOMPARE(prompt->textCursor().position(), 8000);
    QCOMPARE(warnings.size(), 1);
    QCOMPARE(changes.size(), 1);
    QVERIFY(prompt->document()->isUndoAvailable());

    prompt->undo();
    flushQueuedCallbacks();
    QCOMPARE(prompt->toPlainText().size(), 7999);
    QCOMPARE(changes.size(), 2);
}

void SelectionContextActionEditorTests::
commonControlsExposeAccessibleRelationshipsAndExpandedState()
{
    SelectionContextActionEditor editor(
        selectionContextActionTranslate(), fullCatalogs());
    QLabel *nameLabel = required<QLabel>(
        &editor, "selectionActionDisplayNameLabel");
    QLineEdit *name = required<QLineEdit>(
        &editor, "selectionActionDisplayName");
    QToolButton *expand = required<QToolButton>(
        &editor, "selectionActionExpand");
    QCOMPARE(nameLabel->buddy(), static_cast<QWidget *>(name));
    QVERIFY(expand->isCheckable());
    QVERIFY(!expand->accessibleName().trimmed().isEmpty());
    QVERIFY(!expand->isChecked());
    QVERIFY(expand->accessibleDescription().contains(
        QString::fromUtf8("收起")));

    expand->click();
    QVERIFY(editor.isExpanded());
    QVERIFY(expand->isChecked());
    QVERIFY(expand->accessibleDescription().contains(
        QString::fromUtf8("展开")));
    editor.setExpanded(false);
    QVERIFY(!editor.isExpanded());
    QVERIFY(!expand->isChecked());
    QVERIFY(expand->accessibleDescription().contains(
        QString::fromUtf8("收起")));
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
#else
    QSKIP("The strengthened CJK rendering gate runs on Windows native only");
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
        value.displayName = QString::fromUtf8(
            "中文动作名称中文动作名称中文动作名称中文动作名称");
        value.modelId = QStringLiteral("provider:")
            + QString(80, QLatin1Char('x'));
        value.promptOverride = QString::fromUtf8("请保留术语并给出准确自然的中文翻译。");
        value.targetLanguage = QString::fromUtf8("超长的自定义目标语言名称");
        editor->setCustomization(value);
        editor->setExpanded(true);

        QWidget host;
        QVBoxLayout layout(&host);
        layout.addWidget(editor);
        host.setFixedWidth(760);
        host.resize(760, 900);
        host.show();
        QTest::qWait(20);
        QCoreApplication::processEvents();
        QCOMPARE(host.width(), 760);

#ifdef Q_OS_WIN
        const QRawFont rawFont = QRawFont::fromFont(editor->font());
        QVERIFY2(rawFont.isValid(),
                 "Windows native visual gate requires a valid raw font");
        const QString requiredCjk = QString::fromUtf8("中文不可用显示名称");
        for (const QChar character : requiredCjk) {
            QVERIFY2(rawFont.supportsCharacter(character),
                     qPrintable(QStringLiteral("missing CJK glyph U+%1")
                         .arg(character.unicode(), 4, 16, QLatin1Char('0'))));
        }
        QVERIFY2(QFontInfo(editor->font()).family().contains(
                     QString::fromUtf8("雅黑"), Qt::CaseInsensitive)
                     || QFontInfo(editor->font()).family().contains(
                         QStringLiteral("YaHei"), Qt::CaseInsensitive),
                 "Windows native visual gate requires Microsoft YaHei UI");
#endif
        QLabel *displayTitle = required<QLabel>(
            editor, "selectionActionDisplayNameLabel");
        QLineEdit *displayName = required<QLineEdit>(
            editor, "selectionActionDisplayName");
        QComboBox *model = required<QComboBox>(
            editor, "selectionActionModel");
        displayName->clearFocus();
        model->clearFocus();
        QCoreApplication::processEvents();
        const QPixmap screenshot = host.grab();
        QVERIFY(!screenshot.isNull());
        QVERIFY(hasDarkTextPixels(screenshot, 8, 8, 8, 8));
        QVERIFY2(hasDarkTextPixels(displayTitle->grab(), 0, 0, 0, 0),
                 "display-name title did not render visible dark pixels");
        QVERIFY2(hasDarkTextPixels(displayName->grab(), 6, 4, 12, 4),
                 "long Chinese display name did not render visible dark pixels");
        QVERIFY(model->currentText().contains(QString::fromUtf8("不可用")));
        QVERIFY2(hasDarkTextPixels(model->grab(), 8, 4,
                                   qMax(28, model->width() / 8), 4),
                 "unavailable-model text did not render visible dark pixels");
        QCOMPARE(model->toolTip(), model->currentText());
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
        QPlainTextEdit *prompt = required<QPlainTextEdit>(
            editor, "selectionActionPrompt");
        QVERIFY(prompt->height() >= prompt->sizeHint().height());
    }
    QApplication::setFont(originalFont);
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    const QString worker = QString::fromLocal8Bit(
        qgetenv("VOCEKIT_ACTION_EDITOR_DELETE_WORKER"));
    if (!worker.isEmpty()) {
        return runDeletionWorker(worker);
    }
    SelectionContextActionEditorTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "selection_context_action_editor_tests.moc"
#include <QApplication>
#include <QClipboard>
