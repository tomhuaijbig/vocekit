#include "advanced_api_dialog.h"

#include "../config/model_advanced_settings.h"
#include "../config/secret_config.h"
#include "../providers/model_catalog.h"
#include "../storage/model_request_log.h"
#include "../tasks/model_api_diagnostics_task.h"
#include "attention_message.h"
#include "ui_style.h"

#include <QtConcurrent>
#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

QJsonDocument parsedJson(
    const QString &text,
    QJsonParseError *error)
{
    return QJsonDocument::fromJson(text.trimmed().toUtf8(), error);
}

QString prettyJson(const QJsonValue &value)
{
    if (value.isObject()) {
        return QString::fromUtf8(
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Indented)
        );
    }
    if (value.isArray()) {
        return QString::fromUtf8(
            QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented)
        );
    }
    return QString();
}

QWidget *labeledField(const QString &label, QWidget *field)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *title = new QLabel(label);
    title->setStyleSheet(QStringLiteral("color:#344054;font-weight:600;"));
    layout->addWidget(title);
    layout->addWidget(field);
    return widget;
}

struct DoubleParameterEditor
{
    QCheckBox *enabled = nullptr;
    QDoubleSpinBox *value = nullptr;
};

struct IntegerParameterEditor
{
    QCheckBox *enabled = nullptr;
    QSpinBox *value = nullptr;
};

DoubleParameterEditor addDoubleParameter(
    QGridLayout *layout,
    int row,
    const QString &title,
    double minimum,
    double maximum,
    double value,
    int decimals = 2)
{
    DoubleParameterEditor editor;
    editor.enabled = new QCheckBox(title);
    editor.value = new QDoubleSpinBox;
    editor.value->setRange(minimum, maximum);
    editor.value->setDecimals(decimals);
    editor.value->setSingleStep(0.1);
    editor.value->setValue(value);
    editor.value->setEnabled(false);
    QObject::connect(
        editor.enabled,
        &QCheckBox::toggled,
        editor.value,
        &QWidget::setEnabled
    );
    layout->addWidget(editor.enabled, row, 0);
    layout->addWidget(editor.value, row, 1);
    return editor;
}

IntegerParameterEditor addIntegerParameter(
    QGridLayout *layout,
    int row,
    const QString &title)
{
    IntegerParameterEditor editor;
    editor.enabled = new QCheckBox(title);
    editor.value = new QSpinBox;
    editor.value->setRange(1, 2000000);
    editor.value->setValue(1024);
    editor.value->setEnabled(false);
    QObject::connect(
        editor.enabled,
        &QCheckBox::toggled,
        editor.value,
        &QWidget::setEnabled
    );
    layout->addWidget(editor.enabled, row, 0);
    layout->addWidget(editor.value, row, 1);
    return editor;
}

QString diagnosticsText(const ModelApiDiagnosticsResult &result)
{
    QString text = result.success ? tr8("成功") : tr8("失败");
    text += QStringLiteral(" · ") + result.category;
    if (result.httpStatusCode > 0) {
        text += QStringLiteral(" · HTTP ") + QString::number(result.httpStatusCode);
    }
    if (result.durationMs >= 0) {
        text += QStringLiteral(" · ") + QString::number(result.durationMs) + QStringLiteral(" ms");
    }
    if (!result.message.trimmed().isEmpty()) {
        text += QStringLiteral("\n\n") + result.message;
    }
    if (!result.models.isEmpty()) {
        text += tr8("\n\n可用模型：\n") + result.models.join(QStringLiteral("\n"));
    }
    if (!result.data.isEmpty()) {
        text += tr8("\n\n服务端原始数据：\n") + prettyJson(result.data);
    } else if (!result.rawResponse.isEmpty()) {
        text += tr8("\n\n服务端原始响应：\n")
            + QString::fromUtf8(result.rawResponse);
    }
    return text;
}

} // namespace

void showAdvancedApiDialog(bool useSystemProxy, QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(tr8("高级 API 控制台"));
    dialog.resize(980, 760);
    dialog.setMinimumSize(820, 620);
    dialog.setFont(appFont());

    auto *root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(10);

    auto *intro = new QLabel(tr8(
        "普通聊天仍使用原来的简单流程。只有启用当前模型的高级配置后，下面的参数才会参与请求；"
        "Raw JSON 在最后应用，允许未知字段，值为 null 可删除基础字段。"
    ));
    intro->setWordWrap(true);
    intro->setStyleSheet(QStringLiteral(
        "background:#eef4ff;color:#344054;border-radius:8px;padding:10px;"
    ));
    root->addWidget(intro);

    auto *selectorRow = new QHBoxLayout;
    auto *modelSelector = new QComboBox;
    modelSelector->setEditable(false);
    const QVector<ModelOption> options = modelOptionsForSecrets(loadSecrets());
    for (const ModelOption &option : options) {
        modelSelector->addItem(
            option.hint + QStringLiteral(" · ") + option.title,
            option.id
        );
    }
    if (modelSelector->count() == 0) {
        modelSelector->addItem(tr8("默认模型"), QStringLiteral("default"));
    }
    auto *advancedEnabled = new QCheckBox(tr8("启用这个模型的高级配置"));
    selectorRow->addWidget(new QLabel(tr8("配置对象")));
    selectorRow->addWidget(modelSelector, 1);
    selectorRow->addWidget(advancedEnabled);
    root->addLayout(selectorRow);

    auto *tabs = new QTabWidget;
    root->addWidget(tabs, 1);

    auto *parametersPage = new QWidget;
    auto *parametersRoot = new QVBoxLayout(parametersPage);
    auto *parametersGrid = new QGridLayout;
    parametersGrid->setHorizontalSpacing(18);
    parametersGrid->setVerticalSpacing(9);
    auto *modelEnabled = new QCheckBox(tr8("Model（手动覆盖）"));
    auto *manualModel = new QLineEdit;
    manualModel->setPlaceholderText(tr8("可填写服务端真实模型名；不受内置列表限制"));
    manualModel->setEnabled(false);
    QObject::connect(modelEnabled, &QCheckBox::toggled, manualModel, &QWidget::setEnabled);
    parametersGrid->addWidget(modelEnabled, 0, 0);
    parametersGrid->addWidget(manualModel, 0, 1);

    const DoubleParameterEditor temperature = addDoubleParameter(
        parametersGrid, 1, tr8("Temperature"), -2.0, 2.0, 0.2, 3
    );
    const DoubleParameterEditor topP = addDoubleParameter(
        parametersGrid, 2, tr8("Top P"), 0.0, 1.0, 1.0, 3
    );
    const IntegerParameterEditor maxTokens = addIntegerParameter(
        parametersGrid, 3, tr8("Max Tokens")
    );
    const IntegerParameterEditor maxOutputTokens = addIntegerParameter(
        parametersGrid, 4, tr8("Max Output Tokens")
    );
    const DoubleParameterEditor frequencyPenalty = addDoubleParameter(
        parametersGrid, 5, tr8("Frequency Penalty"), -2.0, 2.0, 0.0, 3
    );
    const DoubleParameterEditor presencePenalty = addDoubleParameter(
        parametersGrid, 6, tr8("Presence Penalty"), -2.0, 2.0, 0.0, 3
    );
    auto *stream = new QComboBox;
    stream->addItem(tr8("沿用当前调用方式"), QVariant());
    stream->addItem(tr8("开启"), true);
    stream->addItem(tr8("关闭"), false);
    parametersGrid->addWidget(new QLabel(tr8("Stream")), 7, 0);
    parametersGrid->addWidget(stream, 7, 1);
    auto *reasoningEnabled = new QCheckBox(tr8("Reasoning Effort"));
    auto *reasoning = new QComboBox;
    reasoning->setEditable(true);
    reasoning->addItems(QStringList() << QStringLiteral("low")
        << QStringLiteral("medium") << QStringLiteral("high")
        << QStringLiteral("xhigh"));
    reasoning->setEnabled(false);
    QObject::connect(reasoningEnabled, &QCheckBox::toggled, reasoning, &QWidget::setEnabled);
    parametersGrid->addWidget(reasoningEnabled, 8, 0);
    parametersGrid->addWidget(reasoning, 8, 1);
    parametersRoot->addLayout(parametersGrid);

    auto *responseFormatEnabled = new QCheckBox(tr8("Response Format（JSON 值）"));
    auto *responseFormat = new QPlainTextEdit;
    responseFormat->setPlaceholderText(QStringLiteral("{\n  \"type\": \"json_object\"\n}"));
    responseFormat->setMaximumHeight(100);
    responseFormat->setEnabled(false);
    QObject::connect(responseFormatEnabled, &QCheckBox::toggled, responseFormat, &QWidget::setEnabled);
    parametersRoot->addWidget(responseFormatEnabled);
    parametersRoot->addWidget(responseFormat);
    auto *toolsEnabled = new QCheckBox(tr8("Tools（JSON 数组）"));
    auto *tools = new QPlainTextEdit;
    tools->setPlaceholderText(QStringLiteral("[\n  {\"type\": \"function\", \"function\": {...}}\n]"));
    tools->setMaximumHeight(120);
    tools->setEnabled(false);
    QObject::connect(toolsEnabled, &QCheckBox::toggled, tools, &QWidget::setEnabled);
    parametersRoot->addWidget(toolsEnabled);
    parametersRoot->addWidget(tools);
    parametersRoot->addStretch();
    tabs->addTab(parametersPage, tr8("API 参数"));

    auto *rawPage = new QWidget;
    auto *rawRoot = new QHBoxLayout(rawPage);
    auto *rawJson = new QPlainTextEdit;
    rawJson->setObjectName(QStringLiteral("advancedRawJsonEditor"));
    rawJson->setPlaceholderText(tr8(
        "{}\n\n这里可加入软件尚不认识的新字段。Raw JSON 最后覆盖可视化参数；null 删除字段。"
    ));
    auto *actualRequest = new QPlainTextEdit;
    actualRequest->setReadOnly(true);
    actualRequest->setPlaceholderText(tr8("完成一次请求后，这里显示日志中的最终实际 JSON。"));
    auto *rawEditorPanel = labeledField(tr8("Raw JSON（最高优先覆盖）"), rawJson);
    auto *loadActualIntoRaw = new QPushButton(tr8("将最近实际请求载入 Raw JSON"));
    auto *rawEditorColumn = new QVBoxLayout;
    rawEditorColumn->addWidget(rawEditorPanel, 1);
    rawEditorColumn->addWidget(loadActualIntoRaw, 0, Qt::AlignRight);
    rawRoot->addLayout(rawEditorColumn, 1);
    rawRoot->addWidget(labeledField(tr8("最近一次最终实际请求（只读）"), actualRequest), 1);
    tabs->addTab(rawPage, tr8("Raw JSON"));
    QObject::connect(loadActualIntoRaw, &QPushButton::clicked, &dialog, [&, actualRequest, rawJson]() {
        if (actualRequest->toPlainText().trimmed().isEmpty()) {
            showAttentionInformation(
                &dialog,
                tr8("没有实际请求"),
                tr8("请先完成一次模型调用，再把最终请求载入编辑区。")
            );
            return;
        }
        rawJson->setPlainText(actualRequest->toPlainText());
    });

    auto *promptPage = new QWidget;
    auto *promptRoot = new QVBoxLayout(promptPage);
    auto *promptOverride = new QCheckBox(tr8("使用当前预设覆盖本模型的 System Prompt"));
    auto *promptSelector = new QComboBox;
    auto *promptName = new QLineEdit;
    promptName->setPlaceholderText(tr8("预设名称"));
    auto *promptContent = new QPlainTextEdit;
    promptContent->setPlaceholderText(tr8("System Prompt 可以为空；保存并启用后会清空系统提示词。"));
    auto *promptButtons = new QHBoxLayout;
    auto *newPrompt = new QPushButton(tr8("新建"));
    auto *savePrompt = new QPushButton(tr8("保存当前预设"));
    auto *clearPrompt = new QPushButton(tr8("清空内容"));
    auto *deletePrompt = new QPushButton(tr8("删除预设"));
    promptButtons->addWidget(newPrompt);
    promptButtons->addWidget(savePrompt);
    promptButtons->addWidget(clearPrompt);
    promptButtons->addWidget(deletePrompt);
    promptButtons->addStretch();
    promptRoot->addWidget(promptOverride);
    promptRoot->addWidget(labeledField(tr8("已保存预设"), promptSelector));
    promptRoot->addWidget(promptName);
    promptRoot->addWidget(promptContent, 1);
    promptRoot->addLayout(promptButtons);
    tabs->addTab(promptPage, tr8("System Prompt"));

    auto *statusPage = new QWidget;
    auto *statusRoot = new QVBoxLayout(statusPage);
    auto *endpointGrid = new QGridLayout;
    auto *modelsEndpoint = new QLineEdit;
    modelsEndpoint->setPlaceholderText(tr8("留空使用服务商默认 Models API；也可填写完整地址"));
    auto *balanceEndpoint = new QLineEdit;
    balanceEndpoint->setPlaceholderText(tr8("服务商支持时填写完整 Balance API 地址"));
    endpointGrid->addWidget(new QLabel(tr8("Models API")), 0, 0);
    endpointGrid->addWidget(modelsEndpoint, 0, 1);
    endpointGrid->addWidget(new QLabel(tr8("Balance API")), 1, 0);
    endpointGrid->addWidget(balanceEndpoint, 1, 1);
    statusRoot->addLayout(endpointGrid);
    auto *diagnosticButtons = new QHBoxLayout;
    auto *keyTest = new QPushButton(tr8("API Key Test"));
    auto *connectionTest = new QPushButton(tr8("Connection Test"));
    auto *fetchModels = new QPushButton(tr8("Fetch Models"));
    auto *queryBalance = new QPushButton(tr8("查询余额"));
    diagnosticButtons->addWidget(keyTest);
    diagnosticButtons->addWidget(connectionTest);
    diagnosticButtons->addWidget(fetchModels);
    diagnosticButtons->addWidget(queryBalance);
    diagnosticButtons->addStretch();
    statusRoot->addLayout(diagnosticButtons);
    auto *diagnosticHint = new QLabel(tr8(
        "API Key Test 和 Connection Test 会真实发送一次短请求，可能产生少量 Token 与费用；"
        "Connection Test 使用已保存的高级配置。"
    ));
    diagnosticHint->setWordWrap(true);
    diagnosticHint->setStyleSheet(QStringLiteral("color:#667085;"));
    statusRoot->addWidget(diagnosticHint);
    auto *fetchedModelSelector = new QComboBox;
    fetchedModelSelector->setEditable(true);
    fetchedModelSelector->setPlaceholderText(tr8("获取到的模型会显示在这里；也可以直接手动输入"));
    statusRoot->addWidget(labeledField(tr8("模型列表 / 手动模型名"), fetchedModelSelector));
    auto *diagnosticResult = new QPlainTextEdit;
    diagnosticResult->setReadOnly(true);
    statusRoot->addWidget(diagnosticResult, 1);

    auto *pricingGrid = new QGridLayout;
    auto makePrice = []() {
        auto *spin = new QDoubleSpinBox;
        spin->setRange(-1.0, 1000000.0);
        spin->setDecimals(6);
        spin->setSpecialValueText(QString::fromUtf8("未设置"));
        spin->setValue(-1.0);
        return spin;
    };
    auto *inputPrice = makePrice();
    auto *outputPrice = makePrice();
    auto *reasoningPrice = makePrice();
    pricingGrid->addWidget(new QLabel(tr8("输入 / 百万 Token")), 0, 0);
    pricingGrid->addWidget(inputPrice, 0, 1);
    pricingGrid->addWidget(new QLabel(tr8("输出 / 百万 Token")), 0, 2);
    pricingGrid->addWidget(outputPrice, 0, 3);
    pricingGrid->addWidget(new QLabel(tr8("推理 / 百万 Token")), 0, 4);
    pricingGrid->addWidget(reasoningPrice, 0, 5);
    statusRoot->addLayout(pricingGrid);
    auto *estimateHint = new QLabel(tr8("价格由用户填写；费用显示始终标记为估算值，未设置价格时不估算。币种按 USD 记录。"));
    estimateHint->setWordWrap(true);
    estimateHint->setStyleSheet(QStringLiteral("color:#667085;"));
    statusRoot->addWidget(estimateHint);
    tabs->addTab(statusPage, tr8("API 状态"));

    auto *logsPage = new QWidget;
    auto *logsRoot = new QVBoxLayout(logsPage);
    auto *logsInfo = new QLabel(tr8("日志显示实际请求、HTTP 状态、Token、耗时、停止原因和错误。认证 Header 不写入日志，敏感字段自动脱敏。"));
    logsInfo->setWordWrap(true);
    auto *logsView = new QPlainTextEdit;
    logsView->setReadOnly(true);
    auto *reloadLogs = new QPushButton(tr8("刷新请求日志"));
    logsRoot->addWidget(logsInfo);
    logsRoot->addWidget(logsView, 1);
    logsRoot->addWidget(reloadLogs, 0, Qt::AlignRight);
    tabs->addTab(logsPage, tr8("Request Log"));

    QVector<ModelSystemPromptPreset> promptPresets;
    QString activePromptId;
    QStringList fetchedModelsCache;

    auto displayPrompt = [&]() {
        const int index = promptSelector->currentIndex();
        if (index < 0 || index >= promptPresets.size()) {
            promptName->clear();
            promptContent->clear();
            return;
        }
        promptName->setText(promptPresets.at(index).name);
        promptContent->setPlainText(promptPresets.at(index).content);
        activePromptId = promptPresets.at(index).id;
    };
    auto rebuildPromptSelector = [&]() {
        promptSelector->blockSignals(true);
        promptSelector->clear();
        int activeIndex = -1;
        for (int i = 0; i < promptPresets.size(); ++i) {
            promptSelector->addItem(promptPresets.at(i).name, promptPresets.at(i).id);
            if (promptPresets.at(i).id == activePromptId) {
                activeIndex = i;
            }
        }
        promptSelector->setCurrentIndex(activeIndex >= 0 ? activeIndex : 0);
        promptSelector->blockSignals(false);
        displayPrompt();
    };
    QObject::connect(promptSelector, &QComboBox::currentIndexChanged, &dialog, [=, &displayPrompt](int) {
        displayPrompt();
    });
    QObject::connect(newPrompt, &QPushButton::clicked, &dialog, [&]() {
        ModelSystemPromptPreset preset;
        preset.id = QStringLiteral("prompt-")
            + QString::number(QDateTime::currentMSecsSinceEpoch());
        preset.name = tr8("新提示词");
        promptPresets.append(preset);
        activePromptId = preset.id;
        rebuildPromptSelector();
        promptName->selectAll();
        promptName->setFocus();
    });
    QObject::connect(savePrompt, &QPushButton::clicked, &dialog, [&]() {
        const int index = promptSelector->currentIndex();
        if (index < 0 || index >= promptPresets.size()) {
            newPrompt->click();
            return;
        }
        promptPresets[index].name = promptName->text().trimmed().isEmpty()
            ? tr8("未命名提示词") : promptName->text().trimmed();
        promptPresets[index].content = promptContent->toPlainText();
        activePromptId = promptPresets.at(index).id;
        rebuildPromptSelector();
    });
    QObject::connect(clearPrompt, &QPushButton::clicked, promptContent, &QPlainTextEdit::clear);
    QObject::connect(deletePrompt, &QPushButton::clicked, &dialog, [&]() {
        const int index = promptSelector->currentIndex();
        if (index >= 0 && index < promptPresets.size()) {
            promptPresets.remove(index);
        }
        activePromptId = promptPresets.isEmpty() ? QString() : promptPresets.first().id;
        rebuildPromptSelector();
    });
    QObject::connect(fetchedModelSelector, &QComboBox::currentTextChanged, &dialog, [=](const QString &text) {
        if (!text.trimmed().isEmpty()) {
            modelEnabled->setChecked(true);
            manualModel->setText(text.trimmed());
        }
    });

    auto setDouble = [](const QJsonObject &parameters, const QString &key, const DoubleParameterEditor &editor) {
        const bool has = parameters.value(key).isDouble();
        editor.enabled->setChecked(has);
        if (has) {
            editor.value->setValue(parameters.value(key).toDouble());
        }
    };
    auto setInteger = [](const QJsonObject &parameters, const QString &key, const IntegerParameterEditor &editor) {
        const bool has = parameters.value(key).isDouble();
        editor.enabled->setChecked(has);
        if (has) {
            editor.value->setValue(parameters.value(key).toInt());
        }
    };
    auto refreshActualAndLogs = [&]() {
        const QVector<QJsonObject> entries = recentModelRequestLogs(100);
        QJsonArray array;
        for (const QJsonObject &entry : entries) {
            array.append(entry);
        }
        logsView->setPlainText(prettyJson(array));
        if (!entries.isEmpty()) {
            actualRequest->setPlainText(prettyJson(
                entries.constLast().value(QStringLiteral("actual_request"))
            ));
        } else {
            actualRequest->clear();
        }
    };

    auto loadCurrentProfile = [&]() {
        const QString key = modelSelector->currentData().toString();
        const ModelAdvancedProfile profile = loadModelAdvancedProfile(key);
        advancedEnabled->setChecked(profile.enabled);
        const QJsonObject parameters = profile.parameters;
        modelEnabled->setChecked(parameters.value(QStringLiteral("model")).isString());
        manualModel->setText(parameters.value(QStringLiteral("model")).toString());
        setDouble(parameters, QStringLiteral("temperature"), temperature);
        setDouble(parameters, QStringLiteral("top_p"), topP);
        setInteger(parameters, QStringLiteral("max_tokens"), maxTokens);
        setInteger(parameters, QStringLiteral("max_output_tokens"), maxOutputTokens);
        setDouble(parameters, QStringLiteral("frequency_penalty"), frequencyPenalty);
        setDouble(parameters, QStringLiteral("presence_penalty"), presencePenalty);
        const QJsonValue streamValue = parameters.value(QStringLiteral("stream"));
        stream->setCurrentIndex(streamValue.isBool() ? (streamValue.toBool() ? 1 : 2) : 0);
        reasoningEnabled->setChecked(parameters.value(QStringLiteral("reasoning_effort")).isString());
        reasoning->setCurrentText(parameters.value(QStringLiteral("reasoning_effort")).toString(QStringLiteral("medium")));
        responseFormatEnabled->setChecked(parameters.contains(QStringLiteral("response_format")));
        responseFormat->setPlainText(prettyJson(parameters.value(QStringLiteral("response_format"))));
        toolsEnabled->setChecked(parameters.value(QStringLiteral("tools")).isArray());
        tools->setPlainText(prettyJson(parameters.value(QStringLiteral("tools"))));
        rawJson->setPlainText(prettyJson(profile.rawJson));
        promptPresets = profile.systemPrompts;
        activePromptId = profile.activeSystemPromptId;
        promptOverride->setChecked(profile.systemPromptOverrideEnabled);
        rebuildPromptSelector();
        modelsEndpoint->setText(profile.modelsEndpoint);
        balanceEndpoint->setText(profile.balanceEndpoint);
        fetchedModelsCache = profile.fetchedModels;
        fetchedModelSelector->clear();
        fetchedModelSelector->addItems(fetchedModelsCache);
        inputPrice->setValue(profile.inputPricePerMillion);
        outputPrice->setValue(profile.outputPricePerMillion);
        reasoningPrice->setValue(profile.reasoningPricePerMillion);
        refreshActualAndLogs();
    };
    QObject::connect(modelSelector, &QComboBox::currentIndexChanged, &dialog, [=, &loadCurrentProfile](int) {
        loadCurrentProfile();
    });
    QObject::connect(reloadLogs, &QPushButton::clicked, &dialog, [&]() {
        refreshActualAndLogs();
    });

    auto runDiagnostic = [&](int kind) {
        ModelApiDiagnosticsRequest request;
        request.modelId = modelSelector->currentData().toString();
        request.modelNameOverride = modelEnabled->isChecked()
            ? manualModel->text().trimmed()
            : QString();
        request.useSystemProxy = useSystemProxy;
        request.endpointOverride = kind == 2
            ? modelsEndpoint->text().trimmed()
            : kind == 3 ? balanceEndpoint->text().trimmed() : QString();
        const QList<QPushButton *> buttons = {
            keyTest, connectionTest, fetchModels, queryBalance
        };
        for (QPushButton *button : buttons) {
            button->setEnabled(false);
        }
        diagnosticResult->setPlainText(tr8("正在请求，请稍候……"));
        auto *watcher = new QFutureWatcher<ModelApiDiagnosticsResult>(&dialog);
        QObject::connect(watcher, &QFutureWatcher<ModelApiDiagnosticsResult>::finished, &dialog, [&, watcher, kind, buttons]() {
            const ModelApiDiagnosticsResult result = watcher->result();
            watcher->deleteLater();
            for (QPushButton *button : buttons) {
                button->setEnabled(true);
            }
            diagnosticResult->setPlainText(diagnosticsText(result));
            if (kind == 2 && result.success && !result.models.isEmpty()) {
                fetchedModelsCache = result.models;
                fetchedModelSelector->clear();
                fetchedModelSelector->addItems(fetchedModelsCache);
            }
        });
        watcher->setFuture(QtConcurrent::run([request, kind]() {
            if (kind == 0) {
                return testModelApiKey(request);
            }
            if (kind == 1) {
                return testModelConnection(request);
            }
            if (kind == 2) {
                return fetchModelApiModels(request);
            }
            return queryModelApiBalance(request);
        }));
    };
    QObject::connect(keyTest, &QPushButton::clicked, &dialog, [&]() { runDiagnostic(0); });
    QObject::connect(connectionTest, &QPushButton::clicked, &dialog, [&]() { runDiagnostic(1); });
    QObject::connect(fetchModels, &QPushButton::clicked, &dialog, [&]() { runDiagnostic(2); });
    QObject::connect(queryBalance, &QPushButton::clicked, &dialog, [&]() { runDiagnostic(3); });

    auto *bottom = new QHBoxLayout;
    auto *pathHint = new QLabel(tr8("配置保存到本机 config/model_advanced.json"));
    pathHint->setStyleSheet(QStringLiteral("color:#667085;"));
    auto *cancel = new QPushButton(tr8("关闭"));
    auto *save = new QPushButton(tr8("保存高级配置"));
    save->setDefault(true);
    bottom->addWidget(pathHint);
    bottom->addStretch();
    bottom->addWidget(cancel);
    bottom->addWidget(save);
    root->addLayout(bottom);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dialog, [&]() {
        QJsonObject parameters;
        if (modelEnabled->isChecked()) {
            parameters.insert(QStringLiteral("model"), manualModel->text().trimmed());
        }
        if (temperature.enabled->isChecked()) {
            parameters.insert(QStringLiteral("temperature"), temperature.value->value());
        }
        if (topP.enabled->isChecked()) {
            parameters.insert(QStringLiteral("top_p"), topP.value->value());
        }
        if (maxTokens.enabled->isChecked()) {
            parameters.insert(QStringLiteral("max_tokens"), maxTokens.value->value());
        }
        if (maxOutputTokens.enabled->isChecked()) {
            parameters.insert(QStringLiteral("max_output_tokens"), maxOutputTokens.value->value());
        }
        if (frequencyPenalty.enabled->isChecked()) {
            parameters.insert(QStringLiteral("frequency_penalty"), frequencyPenalty.value->value());
        }
        if (presencePenalty.enabled->isChecked()) {
            parameters.insert(QStringLiteral("presence_penalty"), presencePenalty.value->value());
        }
        if (stream->currentIndex() > 0) {
            parameters.insert(QStringLiteral("stream"), stream->currentData().toBool());
        }
        if (reasoningEnabled->isChecked()) {
            parameters.insert(QStringLiteral("reasoning_effort"), reasoning->currentText().trimmed());
        }

        auto parseOptional = [&](QCheckBox *enabled, QPlainTextEdit *editor, bool requireArray, const QString &field) -> bool {
            if (!enabled->isChecked()) {
                return true;
            }
            QJsonParseError error;
            const QJsonDocument document = parsedJson(editor->toPlainText(), &error);
            const bool valid = error.error == QJsonParseError::NoError
                && (requireArray ? document.isArray() : (document.isObject() || document.isArray()));
            if (!valid) {
                showAttentionWarning(
                    &dialog,
                    tr8("JSON 格式错误"),
                    field + tr8(" 必须是有效的 ")
                        + (requireArray ? tr8("JSON 数组。") : tr8("JSON 对象或数组。"))
                        + QStringLiteral("\n") + error.errorString()
                );
                return false;
            }
            parameters.insert(
                field,
                document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array())
            );
            return true;
        };
        if (!parseOptional(responseFormatEnabled, responseFormat, false, QStringLiteral("response_format"))
            || !parseOptional(toolsEnabled, tools, true, QStringLiteral("tools"))) {
            return;
        }

        QJsonObject raw;
        if (!rawJson->toPlainText().trimmed().isEmpty()) {
            QJsonParseError error;
            const QJsonDocument document = parsedJson(rawJson->toPlainText(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                showAttentionWarning(
                    &dialog,
                    tr8("Raw JSON 格式错误"),
                    tr8("Raw JSON 必须是一个有效 JSON 对象。")
                        + QStringLiteral("\n") + error.errorString()
                );
                tabs->setCurrentWidget(rawPage);
                return;
            }
            raw = document.object();
        }
        const int promptIndex = promptSelector->currentIndex();
        if (promptIndex >= 0 && promptIndex < promptPresets.size()) {
            promptPresets[promptIndex].name = promptName->text().trimmed().isEmpty()
                ? tr8("未命名提示词") : promptName->text().trimmed();
            promptPresets[promptIndex].content = promptContent->toPlainText();
            activePromptId = promptPresets.at(promptIndex).id;
        }

        ModelAdvancedProfile profile;
        profile.key = modelSelector->currentData().toString();
        profile.enabled = advancedEnabled->isChecked();
        profile.parameters = parameters;
        profile.rawJson = raw;
        profile.systemPrompts = promptPresets;
        profile.activeSystemPromptId = activePromptId;
        profile.systemPromptOverrideEnabled = promptOverride->isChecked();
        profile.modelsEndpoint = modelsEndpoint->text().trimmed();
        profile.balanceEndpoint = balanceEndpoint->text().trimmed();
        profile.fetchedModels = fetchedModelsCache;
        profile.inputPricePerMillion = inputPrice->value();
        profile.outputPricePerMillion = outputPrice->value();
        profile.reasoningPricePerMillion = reasoningPrice->value();
        if (!saveModelAdvancedProfile(profile)) {
            showAttentionWarning(
                &dialog,
                tr8("保存失败"),
                tr8("无法写入 config/model_advanced.json。")
            );
            return;
        }
        showAttentionInformation(
            &dialog,
            tr8("已保存"),
            tr8("高级配置已保存。下一次模型调用会使用新的最终请求。")
        );
    });

    loadCurrentProfile();
    dialog.exec();
}
