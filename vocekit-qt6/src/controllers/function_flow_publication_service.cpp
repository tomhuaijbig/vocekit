#include "function_flow_publication_service.h"

#include "../app/application_events.h"
#include "../domain/function_flow_compiler.h"

#include <climits>

namespace {

void clearError(OperationError *error)
{
    if (error) {
        *error = OperationError();
    }
}

void setError(
    OperationError *error,
    const QString &code,
    const QString &message = QString(),
    const QString &detail = QString())
{
    if (!error) {
        return;
    }
    error->code = code;
    error->message = message;
    error->detail = detail;
}

OperationError operationError(
    const QString &code,
    const QString &message = QString(),
    const QString &detail = QString())
{
    OperationError error;
    error.code = code;
    error.message = message;
    error.detail = detail;
    return error;
}

QString unavailableCode(
    const VersionedFunctionFlowGraph &version,
    const QString &fallback)
{
    return version.unavailableCode.trimmed().isEmpty()
        ? fallback
        : version.unavailableCode;
}

bool isFutureOrUnknownPublishedCode(const QString &code)
{
    return code == QStringLiteral("flow_schema_newer")
        || code == QStringLiteral("flow_node_type_unsupported");
}

bool isRepairablePublishedCode(const QString &code)
{
    return code == QStringLiteral("flow_json_invalid")
        || code == QStringLiteral("flow_published_hash_mismatch");
}

QString publishedUnavailableMessage(const QString &code)
{
    if (code == QStringLiteral("flow_schema_newer")) {
        return QString::fromUtf8(
            "该已发布流程由更高版本的软件创建，请升级后再切换到画布模式。"
        );
    }
    if (code == QStringLiteral("flow_node_type_unsupported")) {
        return QString::fromUtf8(
            "该已发布流程包含当前版本不支持的节点，请升级软件或重新发布流程。"
        );
    }
    return QString::fromUtf8(
        "当前已发布流程无法由此版本使用。"
    );
}

FunctionFlowValidationIssue validationIssue(
    const QString &code,
    const QString &message = QString())
{
    FunctionFlowValidationIssue issue;
    issue.code = code;
    issue.message = message;
    return issue;
}

void appendValidationIssue(
    FunctionFlowValidationResult *validation,
    const FunctionFlowValidationIssue &issue)
{
    if (!validation) {
        return;
    }
    validation->ok = false;
    if (!validation->issueCodes.contains(issue.code)) {
        validation->issueCodes.append(issue.code);
    }
    validation->issues.append(issue);
}

OperationError validationError(
    const FunctionFlowValidationResult &validation)
{
    if (validation.issueCodes.isEmpty()) {
        return operationError(
            QStringLiteral("flow_validation_failed"),
            QStringLiteral("流程校验失败。")
        );
    }
    QString message;
    for (const FunctionFlowValidationIssue &issue :
         validation.issues) {
        if (!issue.message.trimmed().isEmpty()) {
            message = issue.message;
            break;
        }
    }
    if (message.isEmpty()) {
        message = QStringLiteral("流程校验失败。");
    }
    return operationError(
        validation.issueCodes.first(),
        message,
        validation.issueCodes.join(QStringLiteral(","))
    );
}

FunctionFlowValidationContext validationContext(
    const FunctionFlowPublicationAccess &access,
    const AppSettingsData &settings,
    const QString &functionId)
{
    FunctionFlowValidationContext context =
        access.validationContextProvider
            ? access.validationContextProvider(settings, functionId)
            : FunctionFlowValidationContext();
    context.functionId = functionId;
    return context;
}

FunctionFlowCompileResult compileGraph(
    const FunctionFlowPublicationAccess &access,
    const FunctionFlowGraph &graph,
    int publishedRevision,
    const QString &publishedHash)
{
    return access.compileGraph
        ? access.compileGraph(
            graph,
            publishedRevision,
            publishedHash
        )
        : FunctionFlowCompiler::compile(
            graph,
            publishedRevision,
            publishedHash
        );
}

FunctionFlowDraftAnalysis analyzeGraph(
    const FunctionFlowPublicationAccess &access,
    const AppSettingsData &settings,
    const QString &functionId,
    const FunctionFlowGraph &input)
{
    FunctionFlowDraftAnalysis analysis;
    analysis.triggerAvailability.insert(
        FunctionFlowTrigger::MainHotkey,
        false
    );
    analysis.triggerAvailability.insert(
        FunctionFlowTrigger::ScreenshotHotkey,
        false
    );
    analysis.triggerAvailability.insert(
        FunctionFlowTrigger::ScreenshotLauncher,
        false
    );

    const FunctionFlowGraph graph =
        normalizeFunctionFlowGraph(input);
    analysis.graphHash = functionFlowGraphHash(graph);
    if (settings.functionIndex(functionId) < 0) {
        appendValidationIssue(
            &analysis.validation,
            validationIssue(
                QStringLiteral("flow_function_not_found"),
                QStringLiteral("功能不存在。")
            )
        );
        return analysis;
    }

    analysis.validation = FunctionFlowValidator::validateForPublish(
        graph,
        validationContext(access, settings, functionId)
    );
    if (!analysis.validation.ok) {
        return analysis;
    }

    const FunctionFlowCompileResult compiled =
        compileGraph(access, graph, 0, analysis.graphHash);
    if (!compiled.ok) {
        appendValidationIssue(
            &analysis.validation,
            validationIssue(
                compiled.error.code,
                compiled.error.message
            )
        );
        return analysis;
    }

    analysis.triggerAvailability[
        FunctionFlowTrigger::MainHotkey
    ] = compiled.plan.triggers.value(
        FunctionFlowTrigger::MainHotkey
    ).available;
    analysis.triggerAvailability[
        FunctionFlowTrigger::ScreenshotHotkey
    ] = compiled.plan.triggers.value(
        FunctionFlowTrigger::ScreenshotHotkey
    ).available;
    analysis.triggerAvailability[
        FunctionFlowTrigger::ScreenshotLauncher
    ] = compiled.plan.triggers.value(
        FunctionFlowTrigger::ScreenshotLauncher
    ).available;
    return analysis;
}

bool saveSnapshot(
    const FunctionFlowPublicationAccess &access,
    const AppSettingsData &settings,
    OperationError *error)
{
    if (!access.replaceAndSave) {
        setError(
            error,
            QStringLiteral("flow_save_unavailable"),
            QStringLiteral("流程设置保存服务不可用。")
        );
        return false;
    }
    if (access.replaceAndSave(settings, error)) {
        return true;
    }
    if (error && error->code.trimmed().isEmpty()) {
        setError(
            error,
            QStringLiteral("flow_save_failed"),
            QStringLiteral("无法保存流程设置。")
        );
    }
    return false;
}

void publishChange(
    const FunctionFlowPublicationAccess &access,
    const QString &key,
    const QString &functionId)
{
    if (access.publishSettingsChanged) {
        access.publishSettingsChanged(key, functionId);
    }
}

bool sameEditorState(
    const FunctionFlowEditorState &left,
    const FunctionFlowEditorState &right)
{
    return left.viewportCenter == right.viewportCenter
        && left.zoom == right.zoom;
}

QString publishedIntegrityError(
    const VersionedFunctionFlowGraph &published,
    bool enabled)
{
    if (!published.supported) {
        return unavailableCode(
            published,
            QStringLiteral("flow_published_unavailable")
        );
    }
    if (published.revision <= 0) {
        return enabled
            ? QStringLiteral("flow_published_hash_mismatch")
            : QString();
    }
    const QString actualHash =
        functionFlowGraphHash(published.graph);
    if (published.graphHash != actualHash) {
        return QStringLiteral("flow_published_hash_mismatch");
    }
    return QString();
}

} // namespace

FunctionFlowPublicationService::FunctionFlowPublicationService(
    const FunctionFlowPublicationAccess &access)
    : m_access(access)
{
}

bool FunctionFlowPublicationService::readState(
    const QString &functionId,
    FunctionFlowState *state,
    OperationError *error) const
{
    clearError(error);
    if (!state) {
        setError(
            error,
            QStringLiteral("flow_state_target_missing"),
            QStringLiteral("缺少流程状态输出对象。")
        );
        return false;
    }
    const AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    const int index = settings.functionIndex(functionId);
    if (index < 0) {
        setError(
            error,
            QStringLiteral("flow_function_not_found"),
            QStringLiteral("功能不存在。")
        );
        return false;
    }
    *state = settings.functions.at(index).flow;
    return true;
}

FunctionFlowDraftAnalysis
FunctionFlowPublicationService::analyzeDraft(
    const QString &functionId,
    const FunctionFlowGraph &draft) const
{
    const AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    return analyzeGraph(m_access, settings, functionId, draft);
}

bool FunctionFlowPublicationService::addCustomFunction(
    const FunctionSettings &functionWithoutFlow,
    OperationError *error)
{
    clearError(error);
    AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    FunctionSettings function =
        normalizeFunctionSettings(functionWithoutFlow);

    if (function.builtIn) {
        setError(
            error,
            QStringLiteral("flow_custom_function_builtin"),
            QStringLiteral("不能通过自定义功能接口创建内置功能。")
        );
        return false;
    }
    if (function.id.isEmpty()) {
        setError(
            error,
            QStringLiteral("flow_function_id_required"),
            QStringLiteral("功能 ID 不能为空。")
        );
        return false;
    }
    if (function.name.isEmpty()) {
        setError(
            error,
            QStringLiteral("flow_function_name_required"),
            QStringLiteral("功能名称不能为空。")
        );
        return false;
    }
    if (settings.functionIndex(function.id) >= 0
        || settings.retainedOrphanFunctionFlows.contains(function.id)
        || settings.functionOrder.contains(function.id)) {
        setError(
            error,
            QStringLiteral("flow_function_id_conflict"),
            QStringLiteral("功能 ID 已存在。")
        );
        return false;
    }

    function.executionMode = FunctionExecutionMode::Classic;
    function.flow = FunctionFlowState();
    function.flow.draft.graphHash =
        functionFlowGraphHash(function.flow.draft.graph);
    settings.functions.append(function);
    settings.functionOrder.append(function.id);
    if (!saveSnapshot(m_access, settings, error)) {
        return false;
    }
    publishChange(
        m_access,
        functionDefinitionsSettingsKey(),
        function.id
    );
    return true;
}

bool FunctionFlowPublicationService::updateDraft(
    const QString &functionId,
    int expectedRevision,
    const FunctionFlowGraph &draft,
    int *savedRevision,
    OperationError *error)
{
    clearError(error);
    if (savedRevision) {
        *savedRevision = 0;
    }
    AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    const int index = settings.functionIndex(functionId);
    if (index < 0) {
        setError(
            error,
            QStringLiteral("flow_function_not_found"),
            QStringLiteral("功能不存在。")
        );
        return false;
    }

    FunctionFlowState &flow = settings.functions[index].flow;
    if (!flow.draft.supported) {
        setError(
            error,
            unavailableCode(
                flow.draft,
                QStringLiteral("flow_draft_unavailable")
            ),
            QStringLiteral("当前草稿无法由此版本编辑。")
        );
        return false;
    }
    if (flow.draft.revision != expectedRevision) {
        setError(
            error,
            QStringLiteral("flow_draft_stale"),
            QStringLiteral("草稿已在其他位置更新。")
        );
        return false;
    }
    if (flow.draft.revision == INT_MAX) {
        setError(
            error,
            QStringLiteral("flow_draft_revision_invalid"),
            QStringLiteral("草稿版本号已超出范围。")
        );
        return false;
    }

    flow.draft.graph = normalizeFunctionFlowGraph(draft);
    ++flow.draft.revision;
    flow.draft.sourceDraftRevision = 0;
    flow.draft.graphHash =
        functionFlowGraphHash(flow.draft.graph);
    flow.draft.unavailableCode.clear();
    if (!saveSnapshot(m_access, settings, error)) {
        return false;
    }
    if (savedRevision) {
        *savedRevision = flow.draft.revision;
    }
    publishChange(
        m_access,
        functionFlowDraftSettingsKey(),
        functionId
    );
    return true;
}

bool FunctionFlowPublicationService::updateEditorState(
    const QString &functionId,
    const FunctionFlowEditorState &editor,
    OperationError *error)
{
    clearError(error);
    AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    const int index = settings.functionIndex(functionId);
    if (index < 0) {
        setError(
            error,
            QStringLiteral("flow_function_not_found"),
            QStringLiteral("功能不存在。")
        );
        return false;
    }

    FunctionFlowState &flow = settings.functions[index].flow;
    const FunctionFlowEditorState normalized =
        normalizeFunctionFlowEditorState(editor);
    if (sameEditorState(flow.editor, normalized)) {
        return true;
    }
    flow.editor = normalized;
    if (!saveSnapshot(m_access, settings, error)) {
        return false;
    }
    publishChange(
        m_access,
        functionFlowEditorStateSettingsKey(),
        functionId
    );
    return true;
}

FunctionFlowPublishResult FunctionFlowPublicationService::publish(
    const QString &functionId,
    int expectedDraftRevision,
    bool replaceCorruptPublished)
{
    FunctionFlowPublishResult result;
    AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    const int index = settings.functionIndex(functionId);
    if (index < 0) {
        result.error = operationError(
            QStringLiteral("flow_function_not_found"),
            QStringLiteral("功能不存在。")
        );
        return result;
    }

    FunctionSettings &function = settings.functions[index];
    FunctionFlowState &flow = function.flow;
    result.publishedRevision = flow.published.revision;
    if (flow.draft.revision != expectedDraftRevision) {
        result.error = operationError(
            QStringLiteral("flow_draft_stale"),
            QStringLiteral("草稿已在其他位置更新。")
        );
        return result;
    }
    if (!flow.draft.supported) {
        result.error = operationError(
            unavailableCode(
                flow.draft,
                QStringLiteral("flow_draft_unavailable")
            ),
            QStringLiteral("当前草稿无法发布。")
        );
        return result;
    }

    QString publishedProblem = publishedIntegrityError(
        flow.published,
        function.executionMode == FunctionExecutionMode::Canvas
    );
    if (!flow.published.supported
        && isFutureOrUnknownPublishedCode(publishedProblem)) {
        result.error = operationError(
            publishedProblem,
            QStringLiteral("当前发布版本不能由此版本覆盖。")
        );
        return result;
    }
    if (!publishedProblem.isEmpty()) {
        if (!isRepairablePublishedCode(publishedProblem)
            || !replaceCorruptPublished) {
            result.error = operationError(
                isRepairablePublishedCode(publishedProblem)
                    ? QStringLiteral(
                        "flow_published_repair_confirmation_required"
                    )
                    : publishedProblem,
                QStringLiteral("当前发布版本需要确认后才能修复。"),
                publishedProblem
            );
            return result;
        }
    }

    const FunctionFlowGraph graph =
        normalizeFunctionFlowGraph(flow.draft.graph);
    const FunctionFlowDraftAnalysis analysis =
        analyzeGraph(m_access, settings, functionId, graph);
    result.validation = analysis.validation;
    if (!analysis.validation.ok) {
        result.error = validationError(analysis.validation);
        return result;
    }

    const bool hasPublished =
        flow.published.supported
        && flow.published.revision > 0
        && publishedProblem.isEmpty();
    const bool sameHash =
        hasPublished
        && flow.published.graphHash == analysis.graphHash;
    if (!sameHash && flow.published.revision == INT_MAX) {
        result.error = operationError(
            QStringLiteral("flow_published_revision_invalid"),
            QStringLiteral("发布流程版本号已达到上限，无法继续发布。")
        );
        return result;
    }
    const int nextRevision = sameHash
        ? flow.published.revision
        : qMax(0, flow.published.revision) + 1;
    const FunctionFlowCompileResult compiled =
        compileGraph(
            m_access,
            graph,
            nextRevision,
            analysis.graphHash
        );
    if (!compiled.ok) {
        result.error = compiled.error;
        return result;
    }

    if (sameHash) {
        result.ok = true;
        result.publishedRevision = flow.published.revision;
        return result;
    }

    VersionedFunctionFlowGraph published;
    published.revision = nextRevision;
    published.sourceDraftRevision = flow.draft.revision;
    published.graphHash = analysis.graphHash;
    published.graph = graph;
    flow.published = published;
    if (!saveSnapshot(m_access, settings, &result.error)) {
        return result;
    }

    result.ok = true;
    result.publishedRevision = nextRevision;
    publishChange(
        m_access,
        functionFlowPublishedSettingsKey(),
        functionId
    );
    return result;
}

bool FunctionFlowPublicationService::setExecutionMode(
    const QString &functionId,
    FunctionExecutionMode mode,
    OperationError *error)
{
    clearError(error);
    AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    const int index = settings.functionIndex(functionId);
    if (index < 0) {
        setError(
            error,
            QStringLiteral("flow_function_not_found"),
            QStringLiteral("功能不存在。")
        );
        return false;
    }

    FunctionSettings &function = settings.functions[index];
    FunctionFlowState &flow = function.flow;
    if (mode == FunctionExecutionMode::Classic) {
        if (function.executionMode == mode
            && !flow.retainedValues.contains(
                QStringLiteral("executionMode")
            )) {
            return true;
        }
        function.executionMode = mode;
        flow.retainedValues.remove(QStringLiteral("executionMode"));
        function = normalizeFunctionSettings(function);
        if (!saveSnapshot(m_access, settings, error)) {
            return false;
        }
        publishChange(
            m_access,
            functionExecutionModeSettingsKey(),
            functionId
        );
        return true;
    }

    if (!flow.published.supported) {
        const QString code = unavailableCode(
            flow.published,
            QStringLiteral("flow_published_unavailable")
        );
        setError(
            error,
            code,
            publishedUnavailableMessage(code)
        );
        return false;
    }
    if (flow.published.revision <= 0) {
        setError(
            error,
            QStringLiteral("flow_published_unavailable"),
            QStringLiteral("没有可启用的发布流程。"),
            flow.published.unavailableCode
        );
        return false;
    }
    const QString integrityProblem =
        publishedIntegrityError(flow.published, true);
    if (!integrityProblem.isEmpty()) {
        setError(
            error,
            integrityProblem,
            QStringLiteral("发布流程的完整性校验失败。")
        );
        return false;
    }

    const FunctionFlowValidationResult validation =
        FunctionFlowValidator::validateForPublish(
            flow.published.graph,
            validationContext(m_access, settings, functionId)
        );
    if (!validation.ok) {
        const OperationError failure = validationError(validation);
        if (error) {
            *error = failure;
        }
        return false;
    }
    const FunctionFlowCompileResult compiled =
        compileGraph(
            m_access,
            flow.published.graph,
            flow.published.revision,
            flow.published.graphHash
        );
    if (!compiled.ok) {
        if (error) {
            *error = compiled.error;
        }
        return false;
    }
    if (function.executionMode == FunctionExecutionMode::Canvas
        && !flow.retainedValues.contains(
            QStringLiteral("executionMode")
        )) {
        return true;
    }

    function.executionMode = FunctionExecutionMode::Canvas;
    flow.retainedValues.remove(QStringLiteral("executionMode"));
    function = normalizeFunctionSettings(function);
    if (!saveSnapshot(m_access, settings, error)) {
        return false;
    }
    publishChange(
        m_access,
        functionExecutionModeSettingsKey(),
        functionId
    );
    return true;
}

bool FunctionFlowPublicationService::removeCustomFunction(
    const QString &functionId,
    OperationError *error)
{
    clearError(error);
    AppSettingsData settings =
        m_access.settingsSnapshotProvider
            ? m_access.settingsSnapshotProvider()
            : AppSettingsData();
    const int index = settings.functionIndex(functionId);
    if (index < 0) {
        setError(
            error,
            QStringLiteral("flow_function_not_found"),
            QStringLiteral("功能不存在。")
        );
        return false;
    }
    if (settings.functions.at(index).builtIn) {
        setError(
            error,
            QStringLiteral("flow_builtin_function_immutable"),
            QStringLiteral("不能删除内置功能。")
        );
        return false;
    }

    settings.functions.remove(index);
    settings.functionOrder.removeAll(functionId);
    settings.retainedOrphanFunctionFlows.remove(functionId);
    if (!saveSnapshot(m_access, settings, error)) {
        return false;
    }
    publishChange(
        m_access,
        functionDefinitionsSettingsKey(),
        functionId
    );
    return true;
}
