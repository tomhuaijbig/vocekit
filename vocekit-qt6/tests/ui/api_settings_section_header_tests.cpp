#include <QtTest>

#include "../../src/ui/api_settings_section.h"
#include "../../src/ui/attention_message.h"

#include <QFile>
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QDir>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <type_traits>

namespace {

QString sourceText(const char *relativePath)
{
    const QString path = QFINDTESTDATA(relativePath);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QString withoutWhitespace(QString text)
{
    text.remove(QRegularExpression(QStringLiteral("\\s+")));
    return text;
}

bool hasScopedCustomModelButtonWiring(
    const QString &source,
    const QString &buttonDeclaration,
    const QString &nextSourceMarker,
    const QString &expectedSizingCall)
{
    const int begin = source.indexOf(buttonDeclaration);
    if (begin < 0) {
        return false;
    }
    const int end = source.indexOf(nextSourceMarker, begin + buttonDeclaration.size());
    if (end < 0) {
        return false;
    }

    const QString block = withoutWhitespace(source.mid(begin, end - begin));
    return block.startsWith(withoutWhitespace(buttonDeclaration))
        && block.contains(withoutWhitespace(expectedSizingCall))
        && !block.contains(QStringLiteral("setFixedHeight("));
}

} // namespace

class ApiSettingsSectionHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromCallbacksOnly();
    void declaresOfficialProviderBaseUrlEditors();
    void wiresOfficialProviderBaseUrlLoadSaveAndRefresh();
    void wiringMatcherRejectsMissingSizingAndFixedHeight();
    void wiresCustomModelEndpointPreview();
    void wiresAddModelButtonSizing();
    void wiresTestButtonSizing();
    void wiresDeleteButtonSizing();
    void wiresCancelButtonSizing();
    void wiresSaveButtonSizing();
    void carriesAndPersistsWindowsSpeechLanguage();
    void savesThreeRuntimeValuesAndRollsBackOnFailure();
    void customModelDialogOffersOptionalSamplingParameters();
};

void ApiSettingsSectionHeaderTests::constructsFromCallbacksOnly()
{
    QVERIFY((std::is_constructible<
        ApiSettingsSection,
        const ApiSettingsSection::Callbacks &,
        QWidget *
    >::value));
}

void ApiSettingsSectionHeaderTests::declaresOfficialProviderBaseUrlEditors()
{
    const QString header = sourceText("../../src/ui/api_settings_section.h");
    QVERIFY(!header.isEmpty());
    QVERIFY(header.contains(QStringLiteral("m_openaiBaseUrlEdit")));
    QVERIFY(header.contains(QStringLiteral("m_anthropicBaseUrlEdit")));
}

void ApiSettingsSectionHeaderTests::wiresOfficialProviderBaseUrlLoadSaveAndRefresh()
{
    const QString source = sourceText("../../src/ui/api_settings_section.cpp");
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral("secrets.openaiBaseUrl")));
    QVERIFY(source.contains(QStringLiteral("secrets.anthropicBaseUrl")));
    QVERIFY(source.contains(QString::fromUtf8("OpenAI Base URL（可选）")));
    QVERIFY(source.contains(QString::fromUtf8("Anthropic Base URL（可选）")));
    QVERIFY(source.contains(QString::fromUtf8("官方地址")));
    QVERIFY(source.contains(QStringLiteral("m_openaiBaseUrlEdit->setText")));
    QVERIFY(source.contains(QStringLiteral("m_anthropicBaseUrlEdit->setText")));
}

void ApiSettingsSectionHeaderTests::wiringMatcherRejectsMissingSizingAndFixedHeight()
{
    const QString declaration = QStringLiteral(
        "auto *add = new QPushButton(apiTr8(\"新增模型\"));"
    );
    const QString nextMarker = QStringLiteral("top->addWidget(title, 1);");
    const QString sizingCall = QStringLiteral(
        "applyCustomModelDialogButtonSizing(add);"
    );
    const QString validFixture = declaration
        + sizingCall
        + nextMarker;
    const QString missingSizingFixture = declaration
        + nextMarker;
    const QString fixedHeightFixture = declaration
        + QStringLiteral("add->setFixedHeight(36);")
        + sizingCall
        + nextMarker;

    QVERIFY(hasScopedCustomModelButtonWiring(
        validFixture,
        declaration,
        nextMarker,
        sizingCall
    ));
    QVERIFY(!hasScopedCustomModelButtonWiring(
        missingSizingFixture,
        declaration,
        nextMarker,
        sizingCall
    ));
    QVERIFY(!hasScopedCustomModelButtonWiring(
        fixedHeightFixture,
        declaration,
        nextMarker,
        sizingCall
    ));
}

void ApiSettingsSectionHeaderTests::wiresCustomModelEndpointPreview()
{
    const QString source = sourceText("../../src/ui/api_settings_section.cpp");
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QString::fromUtf8("API URL（接口地址）")));
    QVERIFY(source.contains(QString::fromUtf8("最终请求地址")));
    QVERIFY(source.contains(QStringLiteral("customModelFinalEndpointPreview")));
}

void ApiSettingsSectionHeaderTests::wiresAddModelButtonSizing()
{
    const QString source = sourceText("../../src/ui/api_settings_section.cpp");
    QVERIFY(hasScopedCustomModelButtonWiring(
        source,
        QStringLiteral("auto *add = new QPushButton(apiTr8(\"新增模型\"));"),
        QStringLiteral("top->addWidget(title, 1);"),
        QStringLiteral("applyCustomModelDialogButtonSizing(add);")
    ));
}

void ApiSettingsSectionHeaderTests::wiresTestButtonSizing()
{
    const QString source = sourceText("../../src/ui/api_settings_section.cpp");
    QVERIFY(hasScopedCustomModelButtonWiring(
        source,
        QStringLiteral("auto *testButton = new QPushButton(apiTr8(\"测试\"));"),
        QStringLiteral("*deleteButton = new QPushButton(apiTr8(\"删除\"));"),
        QStringLiteral("applyCustomModelDialogButtonSizing(testButton,")
    ));
}

void ApiSettingsSectionHeaderTests::wiresDeleteButtonSizing()
{
    const QString source = sourceText("../../src/ui/api_settings_section.cpp");
    QVERIFY(hasScopedCustomModelButtonWiring(
        source,
        QStringLiteral("*deleteButton = new QPushButton(apiTr8(\"删除\"));"),
        QStringLiteral("top->addWidget(title, 1);"),
        QStringLiteral("applyCustomModelDialogButtonSizing(*deleteButton,")
    ));
}

void ApiSettingsSectionHeaderTests::wiresCancelButtonSizing()
{
    const QString source = sourceText("../../src/ui/api_settings_section.cpp");
    QVERIFY(hasScopedCustomModelButtonWiring(
        source,
        QStringLiteral("auto *cancel = new QPushButton(apiTr8(\"取消\"));"),
        QStringLiteral("auto *save = new QPushButton(apiTr8(\"保存\"));"),
        QStringLiteral("applyCustomModelDialogButtonSizing(cancel,")
    ));
}

void ApiSettingsSectionHeaderTests::wiresSaveButtonSizing()
{
    const QString source = sourceText("../../src/ui/api_settings_section.cpp");
    QVERIFY(hasScopedCustomModelButtonWiring(
        source,
        QStringLiteral("auto *save = new QPushButton(apiTr8(\"保存\"));"),
        QStringLiteral("buttons->addWidget(cancel);"),
        QStringLiteral("applyCustomModelDialogButtonSizing(save);")
    ));
}

void ApiSettingsSectionHeaderTests::carriesAndPersistsWindowsSpeechLanguage()
{
    ApiSettingsSnapshot snapshot;
    snapshot.windowsSpeechLanguage = QStringLiteral("zh-CN");
    QCOMPARE(snapshot.windowsSpeechLanguage, QStringLiteral("zh-CN"));

    const QString header = withoutWhitespace(
        sourceText("../../src/ui/api_settings_section.h")
    );
    const QString source = withoutWhitespace(
        sourceText("../../src/ui/api_settings_section.cpp")
    );
    QVERIFY(header.contains(QStringLiteral("QStringwindowsSpeechLanguage;")));
    QVERIFY(header.contains(QStringLiteral(
        "std::function<bool(constQString&,constQString&,constQString&)>saveRuntimeSettings;"
    )));
    QVERIFY(source.contains(QStringLiteral("supportedSpeechProviderIds()")));
    QVERIFY(source.contains(QStringLiteral("speechProviderWindowsLocal()")));
    QVERIFY(source.contains(QStringLiteral(
        "saveRuntimeSettings(speechProvider,ocrEngine,windowsSpeechLanguage)"
    )));
}

void ApiSettingsSectionHeaderTests::
savesThreeRuntimeValuesAndRollsBackOnFailure()
{
    ApiSettingsSnapshot persisted;
    persisted.speechProvider = QStringLiteral("baidu");
    persisted.ocrEngine = QStringLiteral("automatic");
    persisted.windowsSpeechLanguage = QStringLiteral("follow-windows");
    QString savedSpeech;
    QString savedOcr;
    QString savedLanguage;
    int saveCalls = 0;

    ApiSettingsSection::Callbacks callbacks;
    callbacks.snapshotProvider = [&persisted]() { return persisted; };
    callbacks.saveRuntimeSettings = [
        &savedSpeech,
        &savedOcr,
        &savedLanguage,
        &saveCalls
    ](
        const QString &speech,
        const QString &ocr,
        const QString &language
    ) {
        ++saveCalls;
        savedSpeech = speech;
        savedOcr = ocr;
        savedLanguage = language;
        return false;
    };
    ApiSettingsSection section(callbacks);
    setAttentionMessageBoxClickCallbackForTests([](QWidget *widget) {
        QMessageBox *box = qobject_cast<QMessageBox *>(widget);
        if (box) {
            box->done(QMessageBox::Ok);
        }
    });
    QComboBox *speech = section.findChild<QComboBox *>(
        QStringLiteral("speechProviderBox")
    );
    QComboBox *ocr = section.findChild<QComboBox *>(
        QStringLiteral("ocrProviderBox")
    );
    QComboBox *language = section.findChild<QComboBox *>(
        QStringLiteral("windowsSpeechLanguageBox")
    );
    QVERIFY(speech && ocr && language);
    speech->setCurrentIndex(speech->findData(QStringLiteral("windows-local")));
    ocr->setCurrentIndex(ocr->findData(QStringLiteral("windows")));
    language->setCurrentIndex(language->findData(QStringLiteral("en-US")));

    QVERIFY(!section.saveSecretsFromUi(false));
    QCOMPARE(saveCalls, 1);
    QCOMPARE(savedSpeech, QStringLiteral("windows-local"));
    QCOMPARE(savedOcr, QStringLiteral("windows"));
    QCOMPARE(savedLanguage, QStringLiteral("en-US"));
    QCOMPARE(persisted.speechProvider, QStringLiteral("baidu"));
    QCOMPARE(persisted.ocrEngine, QStringLiteral("automatic"));
    QCOMPARE(
        persisted.windowsSpeechLanguage,
        QStringLiteral("follow-windows")
    );
    QCOMPARE(speech->currentData().toString(), persisted.speechProvider);
    QCOMPARE(ocr->currentData().toString(), persisted.ocrEngine);
    QCOMPARE(language->currentData().toString(), persisted.windowsSpeechLanguage);
    setAttentionMessageBoxClickCallbackForTests(
        std::function<void(QWidget *)>()
    );
}

void ApiSettingsSectionHeaderTests::
customModelDialogOffersOptionalSamplingParameters()
{
    ApiSettingsSection::Callbacks callbacks;
    ApiSettingsSection section(callbacks);
    QPushButton *configure = nullptr;
    const QList<QPushButton *> buttons = section.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button && button->text() == QString::fromUtf8("配置")) {
            configure = button;
            break;
        }
    }
    QVERIFY(configure);

    bool inspected = false;
    QTimer::singleShot(0, [&inspected]() {
        QDialog *dialog = nullptr;
        const QWidgetList topLevels = QApplication::topLevelWidgets();
        for (QWidget *widget : topLevels) {
            QDialog *candidate = qobject_cast<QDialog *>(widget);
            if (candidate && candidate->windowTitle()
                    == QString::fromUtf8("配置自定义大模型")) {
                dialog = candidate;
                break;
            }
        }
        QVERIFY(dialog);
        QTimer::singleShot(1000, dialog, &QDialog::reject);

        QPushButton *add = nullptr;
        const QList<QPushButton *> dialogButtons =
            dialog->findChildren<QPushButton *>();
        for (QPushButton *button : dialogButtons) {
            if (button && button->text() == QString::fromUtf8("新增模型")) {
                add = button;
                break;
            }
        }
        QVERIFY(add);
        add->click();

        const QList<QCheckBox *> temperatureSwitches =
            dialog->findChildren<QCheckBox *>(
                QStringLiteral("customModelTemperatureEnabled")
            );
        const QList<QDoubleSpinBox *> temperatureSpins =
            dialog->findChildren<QDoubleSpinBox *>(
                QStringLiteral("customModelTemperatureSpin")
            );
        const QList<QCheckBox *> topPSwitches =
            dialog->findChildren<QCheckBox *>(
                QStringLiteral("customModelTopPEnabled")
            );
        const QList<QDoubleSpinBox *> topPSpins =
            dialog->findChildren<QDoubleSpinBox *>(
                QStringLiteral("customModelTopPSpin")
            );
        QVERIFY(!temperatureSwitches.isEmpty());
        QCOMPARE(temperatureSwitches.size(), temperatureSpins.size());
        QCOMPARE(topPSwitches.size(), topPSpins.size());
        QCOMPARE(topPSwitches.size(), temperatureSwitches.size());

        QCheckBox *temperatureEnabled = temperatureSwitches.constLast();
        QDoubleSpinBox *temperature = temperatureSpins.constLast();
        QCheckBox *topPEnabled = topPSwitches.constLast();
        QDoubleSpinBox *topP = topPSpins.constLast();
        QVERIFY(!temperatureEnabled->isChecked());
        QVERIFY(!temperature->isEnabled());
        QCOMPARE(temperature->minimum(), 0.0);
        QCOMPARE(temperature->maximum(), 2.0);
        QVERIFY(!topPEnabled->isChecked());
        QVERIFY(!topP->isEnabled());
        QCOMPARE(topP->minimum(), 0.0);
        QCOMPARE(topP->maximum(), 1.0);

        const QString visualOutputDir = QString::fromLocal8Bit(
            qgetenv("VOCEKIT_VISUAL_OUTPUT_DIR")
        ).trimmed();
        if (!visualOutputDir.isEmpty()) {
            QVERIFY(QDir().mkpath(visualOutputDir));
            QApplication::processEvents();
            const QString imagePath = QDir(visualOutputDir).filePath(
                QStringLiteral("custom-model-sampling-dialog.png")
            );
            QVERIFY(dialog->grab().save(imagePath));
        }

        temperatureEnabled->setChecked(true);
        topPEnabled->setChecked(true);
        QVERIFY(temperature->isEnabled());
        QVERIFY(topP->isEnabled());
        QVERIFY(temperature->minimumHeight() >= temperature->sizeHint().height());
        QVERIFY(topP->minimumHeight() >= topP->sizeHint().height());
        inspected = true;
        dialog->reject();
    });

    configure->click();
    QVERIFY(inspected);
}

QTEST_MAIN(ApiSettingsSectionHeaderTests)

#include "api_settings_section_header_tests.moc"
