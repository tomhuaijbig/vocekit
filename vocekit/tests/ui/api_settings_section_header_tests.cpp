#include <QtTest>

#include "../../src/ui/api_settings_section.h"

#include <QFile>
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
    text.remove(QRegExp(QStringLiteral("\\s+")));
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

QTEST_MAIN(ApiSettingsSectionHeaderTests)

#include "api_settings_section_header_tests.moc"
