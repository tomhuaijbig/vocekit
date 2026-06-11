#include "voiceassistant.h"

#include <QtWidgets>
#include <QtNetwork>
#include <QtMultimedia>
#include <QtConcurrent>
#include <QtWebSockets/QWebSocket>
#include <QDesktopServices>
#include <algorithm>
#include <cmath>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>
using NativeWindowHandle = HWND;

// Windows UI Automation 接口声明：用于读取鼠标拖选的文字，避免一上来就模拟 Ctrl+C。
struct IUIAutomationCondition;
struct IUIAutomationCacheRequest;
struct IUIAutomationElement;
struct IUIAutomationElementArray;
struct IUIAutomationTreeWalker;
struct IUIAutomationTextRange;
struct IUIAutomationTextRangeArray;
struct IUIAutomationTextPattern;

typedef int PROPERTYID;
typedef int PATTERNID;
typedef int TEXTATTRIBUTEID;

static const PATTERNID kUiaTextPatternId = 10014;
static const CLSID kClsidCuiAutomation = {0xff48dba4, 0x60ef, 0x4201, {0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e}};
static const IID kIidIUiAutomation = {0x30cbe57d, 0xd9d0, 0x452a, {0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee}};
static const IID kIidIUiAutomationTextPattern = {0x32eba289, 0x3583, 0x42c9, {0x9c, 0x59, 0x3b, 0x6d, 0x9a, 0x1e, 0x9b, 0x6a}};

struct IUIAutomationElement : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SetFocus(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRuntimeId(SAFEARRAY **runtimeId) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindFirst(int scope, IUIAutomationCondition *condition, IUIAutomationElement **found) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindAll(int scope, IUIAutomationCondition *condition, IUIAutomationElementArray **found) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindFirstBuildCache(int scope, IUIAutomationCondition *condition, IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **found) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindAllBuildCache(int scope, IUIAutomationCondition *condition, IUIAutomationCacheRequest *cacheRequest, IUIAutomationElementArray **found) = 0;
    virtual HRESULT STDMETHODCALLTYPE BuildUpdatedCache(IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **updatedElement) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPropertyValue(PROPERTYID propertyId, VARIANT *retVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPropertyValueEx(PROPERTYID propertyId, BOOL ignoreDefaultValue, VARIANT *retVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCachedPropertyValue(PROPERTYID propertyId, VARIANT *retVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCachedPropertyValueEx(PROPERTYID propertyId, BOOL ignoreDefaultValue, VARIANT *retVal) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPatternAs(PATTERNID patternId, REFIID riid, void **patternObject) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCachedPatternAs(PATTERNID patternId, REFIID riid, void **patternObject) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPattern(PATTERNID patternId, IUnknown **patternObject) = 0;
};

struct IUIAutomationTreeWalker : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetParentElement(IUIAutomationElement *element, IUIAutomationElement **parent) = 0;
};

struct IUIAutomationTextRange : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Clone(IUIAutomationTextRange **clonedRange) = 0;
    virtual HRESULT STDMETHODCALLTYPE Compare(IUIAutomationTextRange *range, BOOL *areSame) = 0;
    virtual HRESULT STDMETHODCALLTYPE CompareEndpoints(int srcEndPoint, IUIAutomationTextRange *range, int targetEndPoint, int *compValue) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(int textUnit) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindAttribute(TEXTATTRIBUTEID attr, VARIANT val, BOOL backward, IUIAutomationTextRange **found) = 0;
    virtual HRESULT STDMETHODCALLTYPE FindText(BSTR text, BOOL backward, BOOL ignoreCase, IUIAutomationTextRange **found) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAttributeValue(TEXTATTRIBUTEID attr, VARIANT *value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBoundingRectangles(SAFEARRAY **boundingRects) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEnclosingElement(IUIAutomationElement **enclosingElement) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetText(int maxLength, BSTR *text) = 0;
};

struct IUIAutomationTextRangeArray : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE get_Length(int *length) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetElement(int index, IUIAutomationTextRange **element) = 0;
};

struct IUIAutomationTextPattern : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE RangeFromPoint(POINT pt, IUIAutomationTextRange **range) = 0;
    virtual HRESULT STDMETHODCALLTYPE RangeFromChild(IUIAutomationElement *child, IUIAutomationTextRange **range) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSelection(IUIAutomationTextRangeArray **ranges) = 0;
};

struct IUIAutomation : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE CompareElements(IUIAutomationElement *el1, IUIAutomationElement *el2, BOOL *areSame) = 0;
    virtual HRESULT STDMETHODCALLTYPE CompareRuntimeIds(SAFEARRAY *runtimeId1, SAFEARRAY *runtimeId2, BOOL *areSame) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRootElement(IUIAutomationElement **root) = 0;
    virtual HRESULT STDMETHODCALLTYPE ElementFromHandle(HWND hwnd, IUIAutomationElement **element) = 0;
    virtual HRESULT STDMETHODCALLTYPE ElementFromPoint(POINT pt, IUIAutomationElement **element) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFocusedElement(IUIAutomationElement **element) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRootElementBuildCache(IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **root) = 0;
    virtual HRESULT STDMETHODCALLTYPE ElementFromHandleBuildCache(HWND hwnd, IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **element) = 0;
    virtual HRESULT STDMETHODCALLTYPE ElementFromPointBuildCache(POINT pt, IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **element) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFocusedElementBuildCache(IUIAutomationCacheRequest *cacheRequest, IUIAutomationElement **element) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateTreeWalker(IUIAutomationCondition *condition, IUIAutomationTreeWalker **walker) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ControlViewWalker(IUIAutomationTreeWalker **walker) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ContentViewWalker(IUIAutomationTreeWalker **walker) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_RawViewWalker(IUIAutomationTreeWalker **walker) = 0;
};
#else
using NativeWindowHandle = void *;
#endif

// 基础配置数据：描述内置快捷键、自定义功能和模型下拉项。
struct HotkeyDef
{
    QString id;
    QString title;
    QString defaultValue;
    QString hint;
};

struct CustomFunctionDef
{
    QString id;
    QString name;
    QString shortcut;
    QString model;
    QString outputMode;
    bool useSelection;
    bool useVoice;
    int floatingBarSeconds;
    int resultPopupSeconds;
    int countdownSeconds;
    bool recordingBeepEnabled;
    QString recordingBeepPath;
    QString prompt;
};

struct ModelOption
{
    QString id;
    QString title;
    QString hint;
};

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

// 统一提示弹窗：让错误和提示短暂置前，避免弹在最底层用户看不到。
static void showAttentionMessageBox(
    QMessageBox::Icon icon,
    QWidget *parent,
    const QString &title,
    const QString &text
)
{
    QWidget *visibleParent = parent && parent->isVisible() ? parent : nullptr;
    QMessageBox box(icon, title, text, QMessageBox::Ok, visibleParent);
    box.setWindowModality(Qt::ApplicationModal);
    if (QAbstractButton *okButton = box.button(QMessageBox::Ok)) {
        okButton->setText(tr8("确定"));
    }

    QTimer::singleShot(0, &box, [&box]() {
        const int screenNumber = QApplication::desktop()->screenNumber(QCursor::pos());
        const QRect screen = QApplication::desktop()->availableGeometry(screenNumber);
        box.adjustSize();
        box.move(screen.center() - box.rect().center());
        box.show();
        box.raise();
        box.activateWindow();
        QApplication::alert(&box, 3000);
#ifdef Q_OS_WIN
        const HWND handle = reinterpret_cast<HWND>(box.winId());
        SetWindowPos(handle, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        ShowWindow(handle, SW_RESTORE);
        SetForegroundWindow(handle);
#endif
    });
    box.exec();
}

static void showAttentionWarning(QWidget *parent, const QString &title, const QString &text)
{
    showAttentionMessageBox(QMessageBox::Warning, parent, title, text);
}

static void showAttentionInformation(QWidget *parent, const QString &title, const QString &text)
{
    showAttentionMessageBox(QMessageBox::Information, parent, title, text);
}

// 网络错误整理：把底层错误转换成用户能理解的原因和处理方向。
static bool looksLikeRemoteClosedError(const QString &errorText)
{
    return errorText.contains(QStringLiteral("remote host closed"), Qt::CaseInsensitive)
        || errorText.contains(QStringLiteral("host closed"), Qt::CaseInsensitive)
        || errorText.contains(QStringLiteral("connection closed"), Qt::CaseInsensitive)
        || errorText.contains(tr8("远端主机关闭"))
        || errorText.contains(tr8("关闭了这个连接"));
}

static QString xfyunNetworkErrorMessage(const QString &rawError)
{
    const QString original = rawError.trimmed().isEmpty() ? tr8("连接被关闭") : rawError.trimmed();
    if (looksLikeRemoteClosedError(original)) {
        return tr8(
            "讯飞语音听写连接被远端主机关闭。\n\n"
            "专业原因：当前网络可能启用了 TUN 模式、透明代理或虚拟网卡接管。"
            "这类模式会在网卡层重定向流量，软件即使设置为直连，WebSocket 握手也可能被代理链路转发、改写或更换出口 IP。"
            "讯飞 iat-api.xfyun.cn 可能因为出口 IP、DNS 解析、TLS/WebSocket 代理兼容性、控制台 IP 白名单或接口权限不匹配而主动断开连接。\n\n"
            "处理建议：在 v2rayN、Clash 或类似工具的 TUN 规则中，将 iat-api.xfyun.cn 和 *.xfyun.cn 设置为 DIRECT/直连；"
            "然后重新测试讯飞语音听写。\n\n"
            "原始错误："
        ) + original;
    }

    return tr8("讯飞语音听写网络请求失败：") + original
        + tr8("\n\n如果正在使用 TUN 模式、透明代理或虚拟网卡，请优先将 iat-api.xfyun.cn 和 *.xfyun.cn 设置为直连。");
}

// 内置功能和模型选项：集中定义默认快捷键、默认模型、输出方式和语音服务名称。
static QVector<HotkeyDef> hotkeyDefs()
{
    QVector<HotkeyDef> defs;
    defs.append({QStringLiteral("dictate"), tr8("听写（Dictate）"), QStringLiteral("Ctrl+Alt+Space"), tr8("开始或停止语音输入")});
    defs.append({QStringLiteral("translate"), tr8("翻译（Translate）"), QStringLiteral("Ctrl+Alt+T"), tr8("翻译鼠标拖选的文字")});
    defs.append({QStringLiteral("ask"), tr8("问答（Ask）"), QStringLiteral("Ctrl+Alt+Q"), tr8("基于选中文本回答语音问题")});
    defs.append({QStringLiteral("hub"), tr8("打开主界面"), QStringLiteral("Ctrl+Alt+S"), tr8("打开主界面和设置入口")});
    return defs;
}

static QVector<HotkeyDef> coreFunctionDefs()
{
    QVector<HotkeyDef> defs = hotkeyDefs();
    for (int i = defs.size() - 1; i >= 0; --i) {
        if (defs[i].id == QStringLiteral("hub")) {
            defs.remove(i);
        }
    }
    return defs;
}

static QVector<ModelOption> modelOptions()
{
    QVector<ModelOption> options;
    options.append({QStringLiteral("deepseek-v4-flash"), QStringLiteral("deepseek-v4-flash"), tr8("DeepSeek")});
    options.append({QStringLiteral("deepseek-v4-pro"), QStringLiteral("deepseek-v4-pro"), tr8("DeepSeek")});
    options.append({QStringLiteral("openai:gpt-5.5"), QStringLiteral("gpt-5.5"), tr8("OpenAI")});
    options.append({QStringLiteral("openai:gpt-5.4"), QStringLiteral("gpt-5.4"), tr8("OpenAI")});
    options.append({QStringLiteral("openai:gpt-5.4-mini"), QStringLiteral("gpt-5.4-mini"), tr8("OpenAI")});
    options.append({QStringLiteral("claude:claude-opus-4-8"), QStringLiteral("claude-opus-4-8"), tr8("Anthropic")});
    options.append({QStringLiteral("claude:claude-opus-4-7"), QStringLiteral("claude-opus-4-7"), tr8("Anthropic")});
    options.append({QStringLiteral("claude:claude-sonnet-4-6"), QStringLiteral("claude-sonnet-4-6"), tr8("Anthropic")});
    options.append({QStringLiteral("claude:claude-haiku-4-5"), QStringLiteral("claude-haiku-4-5"), tr8("Anthropic")});
    return options;
}

static QString defaultModelForFunction(const QString &id)
{
    if (id == QStringLiteral("ask")) {
        return QStringLiteral("deepseek-v4-pro");
    }
    return QStringLiteral("deepseek-v4-flash");
}

static QString modelTitle(const QString &id)
{
    for (const ModelOption &option : modelOptions()) {
        if (option.id == id) {
            return option.title;
        }
    }
    return modelOptions().first().title;
}

static QString normalizeModelId(const QString &value, const QString &fallback = QString())
{
    const QString trimmed = value.trimmed();
    for (const ModelOption &option : modelOptions()) {
        if (option.id == trimmed) {
            return trimmed;
        }
        if (option.title == trimmed) {
            return option.id;
        }
    }

    if (trimmed.startsWith(QStringLiteral("gpt-"))) {
        return QStringLiteral("openai:") + trimmed;
    }
    if (trimmed.startsWith(QStringLiteral("claude-"))) {
        return QStringLiteral("claude:") + trimmed;
    }

    if (trimmed.startsWith(QStringLiteral("openai:"))) {
        return QStringLiteral("openai:gpt-5.5");
    }
    if (trimmed.startsWith(QStringLiteral("claude:"))) {
        return QStringLiteral("claude:claude-opus-4-8");
    }

    return fallback.trimmed().isEmpty() ? defaultModelForFunction(QString()) : fallback;
}

static QString modelProvider(const QString &model)
{
    if (model.startsWith(QStringLiteral("openai:"))) {
        return QStringLiteral("openai");
    }
    if (model.startsWith(QStringLiteral("claude:"))) {
        return QStringLiteral("claude");
    }
    return QStringLiteral("deepseek");
}

static QString providerModelId(const QString &model)
{
    const int index = model.indexOf(QStringLiteral(":"));
    return index > 0 ? model.mid(index + 1) : model;
}

static QString outputModeAutoWrite()
{
    return QStringLiteral("autoWrite");
}

static QString outputModePopup()
{
    return QStringLiteral("resultPopup");
}

static QString normalizeOutputMode(const QString &value, const QString &fallback = QString())
{
    if (value == outputModeAutoWrite() || value == outputModePopup()) {
        return value;
    }
    return fallback.isEmpty() ? outputModePopup() : fallback;
}

static QString defaultOutputModeForFunction(const QString &id)
{
    if (id == QStringLiteral("dictate")) {
        return outputModeAutoWrite();
    }
    return outputModePopup();
}

static QString outputModeTitle(const QString &mode)
{
    return normalizeOutputMode(mode) == outputModeAutoWrite() ? tr8("自动写入") : tr8("结果小框");
}

static bool defaultUseSelectionForFunction(const QString &id)
{
    return id == QStringLiteral("translate") || id == QStringLiteral("ask");
}

static bool defaultUseVoiceForFunction(const QString &id)
{
    return id != QStringLiteral("translate");
}

static int defaultFloatingBarSeconds()
{
    return 2;
}

static int defaultCountdownSeconds()
{
    return 3;
}

static int defaultResultPopupSeconds()
{
    return 0;
}

static QString speechProviderBaidu()
{
    return QStringLiteral("baidu");
}

static QString speechProviderXfyun()
{
    return QStringLiteral("xfyun");
}

static QString normalizeSpeechProvider(const QString &provider)
{
    return provider.trimmed().toLower() == speechProviderXfyun()
        ? speechProviderXfyun()
        : speechProviderBaidu();
}

static QString speechProviderTitle(const QString &provider)
{
    return normalizeSpeechProvider(provider) == speechProviderXfyun()
        ? tr8("讯飞语音听写")
        : tr8("百度语音识别");
}

static QString compactDiagnosticError(const QString &error)
{
    QString text = error.trimmed();
    if (text.isEmpty()) {
        return tr8("没有返回具体错误。");
    }
    text.replace(QStringLiteral("\n"), QStringLiteral(" "));
    if (text.size() > 180) {
        text = text.left(180) + QStringLiteral("...");
    }
    return text;
}

static QString diagnosticStatusLine(const QString &name, const QString &status, const QString &detail = QString())
{
    return detail.trimmed().isEmpty()
        ? name + tr8("：") + status
        : name + tr8("：") + status + tr8("\n  ") + detail.trimmed();
}

static int pcm16PeakLevel(const QByteArray &pcm)
{
    int peak = 0;
    for (int i = 0; i + 1 < pcm.size(); i += 2) {
        const uchar lo = static_cast<uchar>(pcm.at(i));
        const uchar hi = static_cast<uchar>(pcm.at(i + 1));
        const qint16 sample = static_cast<qint16>((static_cast<int>(hi) << 8) | static_cast<int>(lo));
        peak = qMax(peak, qAbs(static_cast<int>(sample)));
    }
    return peak;
}

static QString networkProbeLine(const QString &name, const QUrl &url, bool useSystemProxy)
{
    QNetworkAccessManager manager;
    if (useSystemProxy) {
        const QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(url));
        for (const QNetworkProxy &proxy : proxies) {
            if (proxy.type() != QNetworkProxy::NoProxy && proxy.type() != QNetworkProxy::DefaultProxy) {
                manager.setProxy(proxy);
                break;
            }
        }
    } else {
        manager.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    }

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "vocekit-Diagnostics");
    QNetworkReply *reply = manager.head(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(9000);
    loop.exec();

    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        return diagnosticStatusLine(name, tr8("失败"), tr8("连接超时。"));
    }
    timer.stop();

    const QNetworkReply::NetworkError error = reply->error();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorString = reply->errorString();
    reply->deleteLater();

    if (error == QNetworkReply::NoError || statusCode > 0) {
        return diagnosticStatusLine(name, tr8("可连接"), statusCode > 0 ? tr8("HTTP 状态：") + QString::number(statusCode) : QString());
    }
    return diagnosticStatusLine(name, tr8("失败"), compactDiagnosticError(errorString));
}

static QString displayShortcut(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return tr8("未设置");
    }
    return QString(trimmed).replace(QStringLiteral("+"), QStringLiteral(" + "));
}

static QString appBasePath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    const QString folder = dir.dirName().toLower();
    if (folder == QStringLiteral("debug") || folder == QStringLiteral("release")) {
        dir.cdUp();
    }
    return dir.absolutePath();
}

static QString autoStartRegistryValueName()
{
    return QStringLiteral("vocekit");
}

static QString autoStartCommand()
{
    return QStringLiteral("\"") + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + QStringLiteral("\"");
}

static bool setWindowsAutoStartEnabled(bool enabled, QString *error = nullptr)
{
#ifdef Q_OS_WIN
    QSettings runKey(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat
    );
    if (enabled) {
        runKey.setValue(autoStartRegistryValueName(), autoStartCommand());
    } else {
        runKey.remove(autoStartRegistryValueName());
    }
    runKey.sync();
    if (runKey.status() != QSettings::NoError) {
        if (error) {
            *error = enabled
                ? tr8("无法写入 Windows 当前用户启动项。请检查安全软件是否拦截注册表写入。")
                : tr8("无法删除 Windows 当前用户启动项。请检查安全软件是否拦截注册表修改。");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(enabled)
    if (error) {
        *error = tr8("当前版本只支持 Windows 开机自启动。");
    }
    return false;
#endif
}

static bool windowsAutoStartEnabled()
{
#ifdef Q_OS_WIN
    QSettings runKey(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat
    );
    const QString command = runKey.value(autoStartRegistryValueName()).toString().trimmed();
    return !command.isEmpty() && command.compare(autoStartCommand(), Qt::CaseInsensitive) == 0;
#else
    return false;
#endif
}

static QString defaultRecordDirectory()
{
    return QDir(appBasePath()).filePath(QStringLiteral("records"));
}

static QString recordDirectoryForDate(const QString &recordDirectory, const QDate &date = QDate::currentDate())
{
    const QString basePath = recordDirectory.trimmed().isEmpty() ? defaultRecordDirectory() : recordDirectory;
    return QDir(QDir(basePath).filePath(tr8("总录音文件"))).filePath(date.toString(QStringLiteral("yyyy-MM-dd")));
}

static void openDirectoryPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QDir().mkpath(trimmed);
    QDesktopServices::openUrl(QUrl::fromLocalFile(trimmed));
}

// 文件路径和复制工具：服务于历史记录备份、导入和导出，避免覆盖同名文件。
static QString normalizedPathForCompare(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).replace(QChar('\\'), QChar('/')).toLower();
}

static bool pathIsSameOrInside(const QString &candidate, const QString &parent)
{
    const QString childPath = normalizedPathForCompare(candidate);
    const QString parentPath = normalizedPathForCompare(parent);
    return childPath == parentPath || childPath.startsWith(parentPath + QStringLiteral("/"));
}

static QString safeFileNamePart(const QString &text, const QString &fallback = QStringLiteral("history"))
{
    QString safe = text.trimmed();
    safe.replace(QRegExp(QStringLiteral("[\\\\/:*?\"<>|\\r\\n\\t]+")), QStringLiteral("_"));
    safe.replace(QRegExp(QStringLiteral("\\s+")), QStringLiteral(" "));
    safe = safe.trimmed();
    if (safe.isEmpty()) {
        safe = fallback;
    }
    if (safe.size() > 72) {
        safe = safe.left(72).trimmed();
    }
    return safe;
}

static QString historyBackupFolderName() { return tr8("备份文件"); }
static QString historyAllAudioFolderName() { return tr8("总录音文件"); }
static QString historyAllTextFolderName() { return tr8("总文本文件"); }
static QString historyAllDetailFolderName() { return tr8("总详细记录文件"); }
static QString historyTextSubFolderName() { return tr8("文本记录"); }
static QString historyAudioSubFolderName() { return tr8("录音记录"); }
static QString historyDetailSubFolderName() { return tr8("详细记录"); }

static bool isHistoryDateFolderName(const QString &name)
{
    return QRegExp(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$")).exactMatch(name);
}

static bool isReservedHistoryRootFolder(const QString &name)
{
    return name == historyBackupFolderName()
        || name == historyAllAudioFolderName()
        || name == historyAllTextFolderName()
        || name == historyAllDetailFolderName();
}

static QString historyRootPath(const QString &recordDirectory)
{
    const QString trimmed = recordDirectory.trimmed();
    return trimmed.isEmpty() ? defaultRecordDirectory() : trimmed;
}

static QString historyBackupDirectory(const QString &recordDirectory)
{
    return QDir(historyRootPath(recordDirectory)).filePath(historyBackupFolderName());
}

static QString historyAllDateDirectory(const QString &recordDirectory, const QString &folderName, const QString &date)
{
    return QDir(QDir(historyRootPath(recordDirectory)).filePath(folderName)).filePath(date);
}

static QString historyModeDirectory(const QString &recordDirectory, const QString &modeTitle)
{
    return QDir(historyRootPath(recordDirectory)).filePath(safeFileNamePart(modeTitle, tr8("未命名功能")));
}

static QString historyModeDateDirectory(const QString &recordDirectory, const QString &modeTitle, const QString &date)
{
    return QDir(historyModeDirectory(recordDirectory, modeTitle)).filePath(date);
}

static QString historyModeDateSubDirectory(const QString &recordDirectory, const QString &modeTitle, const QString &date, const QString &subFolder)
{
    return QDir(historyModeDateDirectory(recordDirectory, modeTitle, date)).filePath(subFolder);
}

static void ensureHistoryRootStructure(const QString &recordDirectory)
{
    QDir root(historyRootPath(recordDirectory));
    root.mkpath(QStringLiteral("."));
    root.mkpath(historyBackupFolderName());
    root.mkpath(historyAllAudioFolderName());
    root.mkpath(historyAllTextFolderName());
    root.mkpath(historyAllDetailFolderName());
}

static void ensureHistoryModeDateStructure(const QString &recordDirectory, const QString &modeTitle, const QString &date)
{
    ensureHistoryRootStructure(recordDirectory);
    QDir().mkpath(historyModeDateSubDirectory(recordDirectory, modeTitle, date, historyTextSubFolderName()));
    QDir().mkpath(historyModeDateSubDirectory(recordDirectory, modeTitle, date, historyAudioSubFolderName()));
    QDir().mkpath(historyModeDateSubDirectory(recordDirectory, modeTitle, date, historyDetailSubFolderName()));
    QDir().mkpath(historyAllDateDirectory(recordDirectory, historyAllAudioFolderName(), date));
    QDir().mkpath(historyAllDateDirectory(recordDirectory, historyAllTextFolderName(), date));
    QDir().mkpath(historyAllDateDirectory(recordDirectory, historyAllDetailFolderName(), date));
}

static QString historyTextFromJsonObject(const QJsonObject &item)
{
    const QString timeText = QString(item.value(QStringLiteral("time")).toString()).replace(QStringLiteral("T"), QStringLiteral(" "));
    QStringList parts;
    parts << tr8("功能：") + item.value(QStringLiteral("mode")).toString();
    parts << tr8("时间：") + timeText;
    parts << tr8("录音：") + (item.value(QStringLiteral("audio")).toString().trimmed().isEmpty() ? tr8("本次没有录音") : item.value(QStringLiteral("audio")).toString());
    parts << tr8("输入内容：\n") + (item.value(QStringLiteral("input")).toString().trimmed().isEmpty() ? tr8("无") : item.value(QStringLiteral("input")).toString());
    parts << tr8("模型输出：\n") + (item.value(QStringLiteral("output")).toString().trimmed().isEmpty() ? tr8("无") : item.value(QStringLiteral("output")).toString());
    parts << tr8("错误：\n") + (item.value(QStringLiteral("error")).toString().trimmed().isEmpty() ? tr8("无") : item.value(QStringLiteral("error")).toString());
    return parts.join(QStringLiteral("\n\n"));
}

static QString uniqueFilePath(const QString &targetPath)
{
    if (!QFileInfo::exists(targetPath)) {
        return targetPath;
    }
    QFileInfo info(targetPath);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : QStringLiteral(".") + info.suffix();
    for (int i = 1; i < 10000; ++i) {
        const QString candidate = info.dir().filePath(base + QStringLiteral("_") + QString::number(i) + suffix);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return info.dir().filePath(base + QStringLiteral("_copy_") + QString::number(QDateTime::currentMSecsSinceEpoch()) + suffix);
}

static bool copyFileToPath(const QString &sourcePath, const QString &targetPath, bool keepExisting, QString *error, int *fileCount)
{
    QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists() && !targetInfo.dir().mkpath(QStringLiteral("."))) {
        if (error) {
            *error = tr8("无法创建目标目录：") + targetInfo.dir().absolutePath();
        }
        return false;
    }

    QString finalPath = targetPath;
    if (QFileInfo::exists(finalPath)) {
        if (keepExisting) {
            finalPath = uniqueFilePath(finalPath);
        } else if (!QFile::remove(finalPath)) {
            if (error) {
                *error = tr8("无法覆盖文件：") + finalPath;
            }
            return false;
        }
    }

    if (!QFile::copy(sourcePath, finalPath)) {
        if (error) {
            *error = tr8("无法复制文件：") + sourcePath;
        }
        return false;
    }
    if (fileCount) {
        ++(*fileCount);
    }
    return true;
}

static bool copyDirectoryContentsRecursive(const QString &sourcePath, const QString &targetPath, bool keepExistingFiles, QString *error, int *fileCount)
{
    QDir source(sourcePath);
    if (!source.exists()) {
        if (error) {
            *error = tr8("源目录不存在：") + sourcePath;
        }
        return false;
    }
    QDir target(targetPath);
    if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = tr8("无法创建目标目录：") + targetPath;
        }
        return false;
    }

    const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString targetEntryPath = target.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryContentsRecursive(entry.absoluteFilePath(), targetEntryPath, keepExistingFiles, error, fileCount)) {
                return false;
            }
        } else if (entry.isFile()) {
            if (!copyFileToPath(entry.absoluteFilePath(), targetEntryPath, keepExistingFiles, error, fileCount)) {
                return false;
            }
        }
    }
    return true;
}

static bool copyDirectoryContentsRecursiveExcept(const QString &sourcePath, const QString &targetPath, const QSet<QString> &excludedFolderNames, bool keepExistingFiles, QString *error, int *fileCount)
{
    QDir source(sourcePath);
    if (!source.exists()) {
        if (error) {
            *error = tr8("源目录不存在：") + sourcePath;
        }
        return false;
    }
    QDir target(targetPath);
    if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = tr8("无法创建目标目录：") + targetPath;
        }
        return false;
    }

    const QFileInfoList entries = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir() && excludedFolderNames.contains(entry.fileName())) {
            continue;
        }
        const QString targetEntryPath = target.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryContentsRecursiveExcept(entry.absoluteFilePath(), targetEntryPath, excludedFolderNames, keepExistingFiles, error, fileCount)) {
                return false;
            }
        } else if (entry.isFile()) {
            if (!copyFileToPath(entry.absoluteFilePath(), targetEntryPath, keepExistingFiles, error, fileCount)) {
                return false;
            }
        }
    }
    return true;
}

// 应用配置中心：负责读取和保存 settings.json，包括快捷键、模型、提示词锁定、历史加载数量等。
class AppSettings
{
public:
    AppSettings()
    {
        resetDefaults();
    }

    void resetDefaults()
    {
        m_hotkeys.clear();
        for (const HotkeyDef &def : hotkeyDefs()) {
            m_hotkeys.insert(def.id, def.defaultValue);
        }
        m_trayResident = true;
        m_autoStartEnabled = windowsAutoStartEnabled();
        m_strongSelectionEnabled = false;
        m_floatingBarEnabled = true;
        m_dictatePolishEnabled = false;
        m_useSystemProxy = false;
        m_preRecordCountdownEnabled = false;
        m_recordingBeepEnabled = false;
        m_speechProvider = speechProviderBaidu();
        m_recordDirectory.clear();
        m_historyInitialLoadCount = 12;
        m_historyLoadMoreCount = 25;
        m_favoriteFolders.clear();
        m_hasFloatingBarPosition = false;
        m_floatingBarPosition = QPoint();
        m_hasResultPopupGeometry = false;
        m_resultPopupGeometry = QRect();
        m_functionOrder.clear();
        m_models.clear();
        m_outputModes.clear();
        m_useSelection.clear();
        m_useVoice.clear();
        m_floatingBarSeconds.clear();
        m_resultPopupSeconds.clear();
        m_countdownSeconds.clear();
        m_recordingBeepEnabledByFunction.clear();
        m_recordingBeepPaths.clear();
        for (const HotkeyDef &def : coreFunctionDefs()) {
            m_models.insert(def.id, defaultModelForFunction(def.id));
            m_outputModes.insert(def.id, defaultOutputModeForFunction(def.id));
            m_useSelection.insert(def.id, defaultUseSelectionForFunction(def.id));
            m_useVoice.insert(def.id, defaultUseVoiceForFunction(def.id));
            m_floatingBarSeconds.insert(def.id, defaultFloatingBarSeconds());
            m_resultPopupSeconds.insert(def.id, defaultResultPopupSeconds());
            m_countdownSeconds.insert(def.id, defaultCountdownSeconds());
            m_recordingBeepEnabledByFunction.insert(def.id, true);
            m_recordingBeepPaths.insert(def.id, QString());
        }

        m_customFunctions.clear();
        m_customFunctions.append({
            QStringLiteral("custom_1"),
            tr8("自定义功能 1"),
            QStringLiteral("Ctrl+Alt+1"),
            QStringLiteral("deepseek-v4-flash"),
            outputModePopup(),
            true,
            true,
            defaultFloatingBarSeconds(),
            defaultResultPopupSeconds(),
            defaultCountdownSeconds(),
            true,
            QString(),
            tr8("请根据选中文本和我的语音要求完成任务，输出可以直接使用的结果。")
        });
    }

    QString hotkey(const QString &id) const
    {
        return m_hotkeys.value(id);
    }

    void setHotkey(const QString &id, const QString &value)
    {
        m_hotkeys.insert(id, value);
    }

    QString settingsPath() const
    {
        return QDir(appBasePath()).filePath(QStringLiteral("config/settings.json"));
    }

    void load()
    {
        QFile file(settingsPath());
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonObject root = doc.object();
        if (root.contains(QStringLiteral("trayResident"))) {
            m_trayResident = root.value(QStringLiteral("trayResident")).toBool(true);
        }
        if (root.contains(QStringLiteral("autoStartEnabled"))) {
            m_autoStartEnabled = root.value(QStringLiteral("autoStartEnabled")).toBool(false);
        } else {
            m_autoStartEnabled = windowsAutoStartEnabled();
        }
        if (root.contains(QStringLiteral("strongSelectionEnabled"))) {
            m_strongSelectionEnabled = root.value(QStringLiteral("strongSelectionEnabled")).toBool(false);
        }
        if (root.contains(QStringLiteral("floatingBarEnabled"))) {
            m_floatingBarEnabled = root.value(QStringLiteral("floatingBarEnabled")).toBool(true);
        }
        if (root.contains(QStringLiteral("promptLocked"))) {
            m_promptLocked = root.value(QStringLiteral("promptLocked")).toBool(false);
        }
        if (root.contains(QStringLiteral("dictatePolishEnabled"))) {
            m_dictatePolishEnabled = root.value(QStringLiteral("dictatePolishEnabled")).toBool(false);
        }
        if (root.contains(QStringLiteral("useSystemProxy"))) {
            m_useSystemProxy = root.value(QStringLiteral("useSystemProxy")).toBool(false);
        }
        if (root.contains(QStringLiteral("preRecordCountdownEnabled"))) {
            m_preRecordCountdownEnabled = root.value(QStringLiteral("preRecordCountdownEnabled")).toBool(false);
        }
        if (root.contains(QStringLiteral("recordingBeepEnabled"))) {
            m_recordingBeepEnabled = root.value(QStringLiteral("recordingBeepEnabled")).toBool(false);
        }
        if (root.contains(QStringLiteral("speechProvider"))) {
            m_speechProvider = normalizeSpeechProvider(root.value(QStringLiteral("speechProvider")).toString());
        }
        if (root.contains(QStringLiteral("recordPath"))) {
            const QString value = root.value(QStringLiteral("recordPath")).toString().trimmed();
            if (!value.isEmpty() && value != tr8("本地按日期保存")) {
                setRecordDirectoryPath(value);
            }
        }
        if (root.contains(QStringLiteral("historyInitialLoadCount"))) {
            setHistoryInitialLoadCount(root.value(QStringLiteral("historyInitialLoadCount")).toInt(m_historyInitialLoadCount));
        }
        if (root.contains(QStringLiteral("historyLoadMoreCount"))) {
            setHistoryLoadMoreCount(root.value(QStringLiteral("historyLoadMoreCount")).toInt(m_historyLoadMoreCount));
        }
        m_favoriteFolders.clear();
        const QJsonArray favoriteFolders = root.value(QStringLiteral("favoriteFolders")).toArray();
        for (const QJsonValue &value : favoriteFolders) {
            addFavoriteFolder(value.toString());
        }
        const QJsonObject floatingBarPosition = root.value(QStringLiteral("floatingBarPosition")).toObject();
        if (floatingBarPosition.contains(QStringLiteral("x")) && floatingBarPosition.contains(QStringLiteral("y"))) {
            setFloatingBarPosition(QPoint(
                floatingBarPosition.value(QStringLiteral("x")).toInt(),
                floatingBarPosition.value(QStringLiteral("y")).toInt()
            ));
        }
        const QJsonObject resultPopupGeometry = root.value(QStringLiteral("resultPopupGeometry")).toObject();
        if (resultPopupGeometry.contains(QStringLiteral("x"))
            && resultPopupGeometry.contains(QStringLiteral("y"))
            && resultPopupGeometry.contains(QStringLiteral("width"))
            && resultPopupGeometry.contains(QStringLiteral("height"))) {
            setResultPopupGeometry(QRect(
                resultPopupGeometry.value(QStringLiteral("x")).toInt(),
                resultPopupGeometry.value(QStringLiteral("y")).toInt(),
                qBound(640, resultPopupGeometry.value(QStringLiteral("width")).toInt(760), 2000),
                qBound(460, resultPopupGeometry.value(QStringLiteral("height")).toInt(520), 1600)
            ));
        }
        const int legacyFloatingBarSeconds = qBound(
            0,
            root.value(QStringLiteral("floatingBarDisplaySeconds")).toInt(defaultFloatingBarSeconds()),
            60
        );
        const int legacyResultPopupSeconds = qBound(
            0,
            root.value(QStringLiteral("resultPopupDisplaySeconds")).toInt(defaultResultPopupSeconds()),
            600
        );
        const QJsonObject hotkeys = root.value(QStringLiteral("hotkeys")).toObject();
        for (const HotkeyDef &def : hotkeyDefs()) {
            const QString value = hotkeys.value(def.id).toString();
            if (!value.trimmed().isEmpty()) {
                m_hotkeys.insert(def.id, value.trimmed());
            }
        }

        const QJsonObject models = root.value(QStringLiteral("models")).toObject();
        for (const HotkeyDef &def : coreFunctionDefs()) {
            const QString value = models.value(def.id).toString();
            if (!value.trimmed().isEmpty()) {
                m_models.insert(def.id, normalizeModelId(value.trimmed(), defaultModelForFunction(def.id)));
            }
        }

        const QJsonObject outputModes = root.value(QStringLiteral("outputModes")).toObject();
        for (const HotkeyDef &def : coreFunctionDefs()) {
            const QString value = outputModes.value(def.id).toString();
            if (!value.trimmed().isEmpty()) {
                m_outputModes.insert(def.id, normalizeOutputMode(value.trimmed(), defaultOutputModeForFunction(def.id)));
            }
        }

        const QJsonObject inputModes = root.value(QStringLiteral("inputModes")).toObject();
        const QJsonObject displayTimes = root.value(QStringLiteral("displayTimes")).toObject();
        for (const HotkeyDef &def : coreFunctionDefs()) {
            const QJsonObject item = inputModes.value(def.id).toObject();
            if (item.contains(QStringLiteral("useSelection"))) {
                m_useSelection.insert(def.id, item.value(QStringLiteral("useSelection")).toBool(defaultUseSelectionForFunction(def.id)));
            }
            if (item.contains(QStringLiteral("useVoice"))) {
                m_useVoice.insert(def.id, item.value(QStringLiteral("useVoice")).toBool(defaultUseVoiceForFunction(def.id)));
            }
            const QJsonObject timeItem = displayTimes.value(def.id).toObject();
            m_floatingBarSeconds.insert(
                def.id,
                qBound(0, timeItem.value(QStringLiteral("floatingBarSeconds")).toInt(legacyFloatingBarSeconds), 60)
            );
            m_resultPopupSeconds.insert(
                def.id,
                qBound(0, timeItem.value(QStringLiteral("resultPopupSeconds")).toInt(legacyResultPopupSeconds), 600)
            );
            m_countdownSeconds.insert(
                def.id,
                qBound(0, timeItem.value(QStringLiteral("countdownSeconds")).toInt(defaultCountdownSeconds()), 60)
            );
            m_recordingBeepEnabledByFunction.insert(
                def.id,
                timeItem.value(QStringLiteral("recordingBeepEnabled")).toBool(true)
            );
            m_recordingBeepPaths.insert(
                def.id,
                timeItem.value(QStringLiteral("recordingBeepPath")).toString().trimmed()
            );
        }

        if (root.contains(QStringLiteral("customFunctions"))) {
            m_customFunctions.clear();
            const QJsonArray customFunctions = root.value(QStringLiteral("customFunctions")).toArray();
            for (const QJsonValue &value : customFunctions) {
                const QJsonObject item = value.toObject();
                CustomFunctionDef fn;
                fn.id = item.value(QStringLiteral("id")).toString();
                fn.name = item.value(QStringLiteral("name")).toString();
                fn.shortcut = item.value(QStringLiteral("shortcut")).toString();
                fn.model = normalizeModelId(item.value(QStringLiteral("model")).toString(QStringLiteral("deepseek-v4-flash")), QStringLiteral("deepseek-v4-flash"));
                fn.outputMode = normalizeOutputMode(item.value(QStringLiteral("outputMode")).toString(), outputModePopup());
                fn.useSelection = item.value(QStringLiteral("useSelection")).toBool(true);
                fn.useVoice = item.value(QStringLiteral("useVoice")).toBool(true);
                fn.floatingBarSeconds = qBound(0, item.value(QStringLiteral("floatingBarSeconds")).toInt(legacyFloatingBarSeconds), 60);
                fn.resultPopupSeconds = qBound(0, item.value(QStringLiteral("resultPopupSeconds")).toInt(legacyResultPopupSeconds), 600);
                fn.countdownSeconds = qBound(0, item.value(QStringLiteral("countdownSeconds")).toInt(defaultCountdownSeconds()), 60);
                fn.recordingBeepEnabled = item.value(QStringLiteral("recordingBeepEnabled")).toBool(true);
                fn.recordingBeepPath = item.value(QStringLiteral("recordingBeepPath")).toString().trimmed();
                fn.prompt = item.value(QStringLiteral("prompt")).toString();
                if (!fn.id.trimmed().isEmpty() && !fn.name.trimmed().isEmpty()) {
                    m_customFunctions.append(fn);
                }
            }
        }

        m_functionOrder.clear();
        const QJsonArray functionOrder = root.value(QStringLiteral("functionOrder")).toArray();
        for (const QJsonValue &value : functionOrder) {
            const QString id = value.toString().trimmed();
            if (!id.isEmpty() && !m_functionOrder.contains(id)) {
                m_functionOrder.append(id);
            }
        }
    }

    bool save() const
    {
        QDir dir(appBasePath());
        if (!dir.exists(QStringLiteral("config"))) {
            dir.mkpath(QStringLiteral("config"));
        }

        QJsonObject hotkeys;
        for (const HotkeyDef &def : hotkeyDefs()) {
            hotkeys.insert(def.id, m_hotkeys.value(def.id));
        }
        QJsonObject models;
        for (const HotkeyDef &def : coreFunctionDefs()) {
            models.insert(def.id, modelFor(def.id));
        }
        QJsonObject outputModes;
        for (const HotkeyDef &def : coreFunctionDefs()) {
            outputModes.insert(def.id, outputModeFor(def.id));
        }
        QJsonObject inputModes;
        QJsonObject displayTimes;
        for (const HotkeyDef &def : coreFunctionDefs()) {
            QJsonObject item;
            item.insert(QStringLiteral("useSelection"), useSelectionFor(def.id));
            item.insert(QStringLiteral("useVoice"), useVoiceFor(def.id));
            inputModes.insert(def.id, item);

            QJsonObject timeItem;
            timeItem.insert(QStringLiteral("floatingBarSeconds"), floatingBarSecondsFor(def.id));
            timeItem.insert(QStringLiteral("resultPopupSeconds"), resultPopupSecondsFor(def.id));
            timeItem.insert(QStringLiteral("countdownSeconds"), countdownSecondsFor(def.id));
            timeItem.insert(QStringLiteral("recordingBeepEnabled"), recordingBeepEnabledFor(def.id));
            timeItem.insert(QStringLiteral("recordingBeepPath"), recordingBeepPathFor(def.id));
            displayTimes.insert(def.id, timeItem);
        }

        QJsonObject root;
        root.insert(QStringLiteral("hotkeys"), hotkeys);
        root.insert(QStringLiteral("models"), models);
        root.insert(QStringLiteral("outputModes"), outputModes);
        root.insert(QStringLiteral("inputModes"), inputModes);
        root.insert(QStringLiteral("displayTimes"), displayTimes);
        root.insert(QStringLiteral("trayResident"), m_trayResident);
        root.insert(QStringLiteral("autoStartEnabled"), m_autoStartEnabled);
        root.insert(QStringLiteral("strongSelectionEnabled"), m_strongSelectionEnabled);
        root.insert(QStringLiteral("floatingBarEnabled"), m_floatingBarEnabled);
        root.insert(QStringLiteral("promptLocked"), m_promptLocked);
        root.insert(QStringLiteral("dictatePolishEnabled"), m_dictatePolishEnabled);
        root.insert(QStringLiteral("useSystemProxy"), m_useSystemProxy);
        root.insert(QStringLiteral("preRecordCountdownEnabled"), m_preRecordCountdownEnabled);
        root.insert(QStringLiteral("recordingBeepEnabled"), m_recordingBeepEnabled);
        root.insert(QStringLiteral("speechProvider"), speechProvider());
        root.insert(QStringLiteral("recordPath"), recordDirectoryPath());
        root.insert(QStringLiteral("historyInitialLoadCount"), historyInitialLoadCount());
        root.insert(QStringLiteral("historyLoadMoreCount"), historyLoadMoreCount());
        QJsonArray favoriteFolders;
        for (const QString &folder : m_favoriteFolders) {
            favoriteFolders.append(folder);
        }
        root.insert(QStringLiteral("favoriteFolders"), favoriteFolders);
        if (m_hasFloatingBarPosition) {
            QJsonObject position;
            position.insert(QStringLiteral("x"), m_floatingBarPosition.x());
            position.insert(QStringLiteral("y"), m_floatingBarPosition.y());
            root.insert(QStringLiteral("floatingBarPosition"), position);
        }
        if (m_hasResultPopupGeometry) {
            QJsonObject geometry;
            geometry.insert(QStringLiteral("x"), m_resultPopupGeometry.x());
            geometry.insert(QStringLiteral("y"), m_resultPopupGeometry.y());
            geometry.insert(QStringLiteral("width"), m_resultPopupGeometry.width());
            geometry.insert(QStringLiteral("height"), m_resultPopupGeometry.height());
            root.insert(QStringLiteral("resultPopupGeometry"), geometry);
        }
        QJsonArray functionOrder;
        for (const QString &id : functionOrderIds()) {
            functionOrder.append(id);
        }
        root.insert(QStringLiteral("functionOrder"), functionOrder);
        root.insert(QStringLiteral("targetLanguage"), tr8("简体中文"));

        QJsonArray customFunctions;
        for (const CustomFunctionDef &fn : m_customFunctions) {
            QJsonObject item;
            item.insert(QStringLiteral("id"), fn.id);
            item.insert(QStringLiteral("name"), fn.name);
            item.insert(QStringLiteral("shortcut"), fn.shortcut);
            item.insert(QStringLiteral("model"), fn.model);
            item.insert(QStringLiteral("outputMode"), normalizeOutputMode(fn.outputMode, outputModePopup()));
            item.insert(QStringLiteral("useSelection"), fn.useSelection);
            item.insert(QStringLiteral("useVoice"), fn.useVoice);
            item.insert(QStringLiteral("floatingBarSeconds"), qBound(0, fn.floatingBarSeconds, 60));
            item.insert(QStringLiteral("resultPopupSeconds"), qBound(0, fn.resultPopupSeconds, 600));
            item.insert(QStringLiteral("countdownSeconds"), qBound(0, fn.countdownSeconds, 60));
            item.insert(QStringLiteral("recordingBeepEnabled"), fn.recordingBeepEnabled);
            item.insert(QStringLiteral("recordingBeepPath"), fn.recordingBeepPath.trimmed());
            item.insert(QStringLiteral("prompt"), fn.prompt);
            customFunctions.append(item);
        }
        root.insert(QStringLiteral("customFunctions"), customFunctions);

        QFile file(settingsPath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        return true;
    }

    bool conflictsWithOther(const QString &id, const QString &value, QString *otherTitle) const
    {
        const QString normalized = value.trimmed().toLower();
        if (normalized.isEmpty()) {
            return false;
        }

        for (const HotkeyDef &def : hotkeyDefs()) {
            if (def.id == id) {
                continue;
            }
            if (m_hotkeys.value(def.id).trimmed().toLower() == normalized) {
                if (otherTitle) {
                    *otherTitle = def.title;
                }
                return true;
            }
        }

        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                continue;
            }
            if (fn.shortcut.trimmed().toLower() == normalized) {
                if (otherTitle) {
                    *otherTitle = fn.name;
                }
                return true;
            }
        }

        return false;
    }

    QVector<CustomFunctionDef> customFunctions() const
    {
        return m_customFunctions;
    }

    QString nextCustomFunctionId() const
    {
        int maxNumber = 0;
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id.startsWith(QStringLiteral("custom_"))) {
                maxNumber = qMax(maxNumber, fn.id.mid(7).toInt());
            }
        }
        return QStringLiteral("custom_%1").arg(maxNumber + 1);
    }

    QString suggestedCustomShortcut() const
    {
        for (int i = 1; i <= 9; ++i) {
            const QString value = QStringLiteral("Ctrl+Alt+%1").arg(i);
            QString otherTitle;
            if (!conflictsWithOther(QString(), value, &otherTitle)) {
                return value;
            }
        }
        return QString();
    }

    void addCustomFunction(const CustomFunctionDef &fn)
    {
        m_customFunctions.append(fn);
    }

    void updateCustomFunction(const CustomFunctionDef &fn)
    {
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == fn.id) {
                m_customFunctions[i] = fn;
                return;
            }
        }
        m_customFunctions.append(fn);
    }

    void removeCustomFunction(const QString &id)
    {
        for (int i = m_customFunctions.size() - 1; i >= 0; --i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions.remove(i);
            }
        }
    }

    QString modelFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return fn.model.trimmed().isEmpty() ? QStringLiteral("deepseek-v4-flash") : fn.model;
            }
        }
        return m_models.value(id, defaultModelForFunction(id));
    }

    void setModelFor(const QString &id, const QString &model)
    {
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].model = model;
                return;
            }
        }
        m_models.insert(id, model);
    }

    QString outputModeFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return normalizeOutputMode(fn.outputMode, outputModePopup());
            }
        }
        return normalizeOutputMode(m_outputModes.value(id), defaultOutputModeForFunction(id));
    }

    void setOutputModeFor(const QString &id, const QString &mode)
    {
        const QString normalized = normalizeOutputMode(mode, defaultOutputModeForFunction(id));
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].outputMode = normalized;
                return;
            }
        }
        m_outputModes.insert(id, normalized);
    }

    bool useSelectionFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return fn.useSelection;
            }
        }
        return m_useSelection.value(id, defaultUseSelectionForFunction(id));
    }

    void setUseSelectionFor(const QString &id, bool enabled)
    {
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].useSelection = enabled;
                return;
            }
        }
        m_useSelection.insert(id, enabled);
    }

    bool useVoiceFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return fn.useVoice;
            }
        }
        return m_useVoice.value(id, defaultUseVoiceForFunction(id));
    }

    void setUseVoiceFor(const QString &id, bool enabled)
    {
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].useVoice = enabled;
                return;
            }
        }
        m_useVoice.insert(id, enabled);
    }

    int floatingBarSecondsFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return qBound(0, fn.floatingBarSeconds, 60);
            }
        }
        return qBound(0, m_floatingBarSeconds.value(id, defaultFloatingBarSeconds()), 60);
    }

    void setFloatingBarSecondsFor(const QString &id, int seconds)
    {
        const int normalized = qBound(0, seconds, 60);
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].floatingBarSeconds = normalized;
                return;
            }
        }
        m_floatingBarSeconds.insert(id, normalized);
    }

    int resultPopupSecondsFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return qBound(0, fn.resultPopupSeconds, 600);
            }
        }
        return qBound(0, m_resultPopupSeconds.value(id, defaultResultPopupSeconds()), 600);
    }

    void setResultPopupSecondsFor(const QString &id, int seconds)
    {
        const int normalized = qBound(0, seconds, 600);
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].resultPopupSeconds = normalized;
                return;
            }
        }
        m_resultPopupSeconds.insert(id, normalized);
    }

    int countdownSecondsFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return qBound(0, fn.countdownSeconds, 60);
            }
        }
        return qBound(0, m_countdownSeconds.value(id, defaultCountdownSeconds()), 60);
    }

    void setCountdownSecondsFor(const QString &id, int seconds)
    {
        const int normalized = qBound(0, seconds, 60);
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].countdownSeconds = normalized;
                return;
            }
        }
        m_countdownSeconds.insert(id, normalized);
    }

    bool recordingBeepEnabledFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return fn.recordingBeepEnabled;
            }
        }
        return m_recordingBeepEnabledByFunction.value(id, true);
    }

    void setRecordingBeepEnabledFor(const QString &id, bool enabled)
    {
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].recordingBeepEnabled = enabled;
                return;
            }
        }
        m_recordingBeepEnabledByFunction.insert(id, enabled);
    }

    QString recordingBeepPathFor(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_customFunctions) {
            if (fn.id == id) {
                return fn.recordingBeepPath.trimmed();
            }
        }
        return m_recordingBeepPaths.value(id).trimmed();
    }

    void setRecordingBeepPathFor(const QString &id, const QString &path)
    {
        const QString trimmed = path.trimmed();
        for (int i = 0; i < m_customFunctions.size(); ++i) {
            if (m_customFunctions[i].id == id) {
                m_customFunctions[i].recordingBeepPath = trimmed;
                return;
            }
        }
        m_recordingBeepPaths.insert(id, trimmed);
    }

    QStringList defaultFunctionOrderIds() const
    {
        QStringList ids;
        for (const HotkeyDef &def : coreFunctionDefs()) {
            ids.append(def.id);
        }
        for (const CustomFunctionDef &fn : m_customFunctions) {
            ids.append(fn.id);
        }
        return ids;
    }

    QStringList functionOrderIds() const
    {
        const QStringList defaults = defaultFunctionOrderIds();
        QStringList ordered;
        for (const QString &id : m_functionOrder) {
            if (defaults.contains(id) && !ordered.contains(id)) {
                ordered.append(id);
            }
        }
        for (const QString &id : defaults) {
            if (!ordered.contains(id)) {
                ordered.append(id);
            }
        }
        return ordered;
    }

    bool moveFunctionInOrder(const QString &id, int delta)
    {
        QStringList ids = functionOrderIds();
        const int index = ids.indexOf(id);
        if (index < 0) {
            return false;
        }
        const int nextIndex = qBound(0, index + delta, ids.size() - 1);
        if (nextIndex == index) {
            return false;
        }
        ids.move(index, nextIndex);
        m_functionOrder = ids;
        return true;
    }

    bool setFunctionOrderIds(const QStringList &ids)
    {
        const QStringList defaults = defaultFunctionOrderIds();
        QStringList normalized;
        for (const QString &id : ids) {
            if (defaults.contains(id) && !normalized.contains(id)) {
                normalized.append(id);
            }
        }
        for (const QString &id : defaults) {
            if (!normalized.contains(id)) {
                normalized.append(id);
            }
        }
        if (normalized == functionOrderIds()) {
            return false;
        }
        m_functionOrder = normalized;
        return true;
    }

    bool trayResident() const
    {
        return m_trayResident;
    }

    void setTrayResident(bool enabled)
    {
        m_trayResident = enabled;
    }

    bool autoStartEnabled() const
    {
        return m_autoStartEnabled;
    }

    void setAutoStartEnabled(bool enabled)
    {
        m_autoStartEnabled = enabled;
    }

    bool strongSelectionEnabled() const
    {
        return m_strongSelectionEnabled;
    }

    void setStrongSelectionEnabled(bool enabled)
    {
        m_strongSelectionEnabled = enabled;
    }

    bool floatingBarEnabled() const
    {
        return m_floatingBarEnabled;
    }

    void setFloatingBarEnabled(bool enabled)
    {
        m_floatingBarEnabled = enabled;
    }

    bool promptLocked() const
    {
        return m_promptLocked;
    }

    void setPromptLocked(bool locked)
    {
        m_promptLocked = locked;
    }

    bool dictatePolishEnabled() const
    {
        return m_dictatePolishEnabled;
    }

    void setDictatePolishEnabled(bool enabled)
    {
        m_dictatePolishEnabled = enabled;
    }

    bool useSystemProxy() const
    {
        return m_useSystemProxy;
    }

    void setUseSystemProxy(bool enabled)
    {
        m_useSystemProxy = enabled;
    }

    bool preRecordCountdownEnabled() const
    {
        return m_preRecordCountdownEnabled;
    }

    void setPreRecordCountdownEnabled(bool enabled)
    {
        m_preRecordCountdownEnabled = enabled;
    }

    bool recordingBeepEnabled() const
    {
        return m_recordingBeepEnabled;
    }

    void setRecordingBeepEnabled(bool enabled)
    {
        m_recordingBeepEnabled = enabled;
    }

    QString recordDirectoryPath() const
    {
        const QString trimmed = m_recordDirectory.trimmed();
        if (trimmed.isEmpty() || trimmed == tr8("本地按日期保存")) {
            return defaultRecordDirectory();
        }
        QDir dir(trimmed);
        if (dir.isRelative()) {
            return QDir(appBasePath()).absoluteFilePath(trimmed);
        }
        return QDir::cleanPath(trimmed);
    }

    void setRecordDirectoryPath(const QString &path)
    {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty() || trimmed == QStringLiteral("records")) {
            m_recordDirectory.clear();
            return;
        }
        QDir dir(trimmed);
        const QString cleanPath = dir.isRelative()
            ? QDir::cleanPath(QDir(appBasePath()).absoluteFilePath(trimmed))
            : QDir::cleanPath(trimmed);
        if (cleanPath == QDir::cleanPath(defaultRecordDirectory())) {
            m_recordDirectory.clear();
            return;
        }
        m_recordDirectory = cleanPath;
    }

    void resetRecordDirectory()
    {
        m_recordDirectory.clear();
    }

    bool usesDefaultRecordDirectory() const
    {
        return m_recordDirectory.trimmed().isEmpty();
    }

    QString speechProvider() const
    {
        return normalizeSpeechProvider(m_speechProvider);
    }

    void setSpeechProvider(const QString &provider)
    {
        m_speechProvider = normalizeSpeechProvider(provider);
    }

    int historyInitialLoadCount() const
    {
        return qBound(5, m_historyInitialLoadCount, 200);
    }

    void setHistoryInitialLoadCount(int count)
    {
        m_historyInitialLoadCount = qBound(5, count, 200);
    }

    int historyLoadMoreCount() const
    {
        return qBound(5, m_historyLoadMoreCount, 200);
    }

    void setHistoryLoadMoreCount(int count)
    {
        m_historyLoadMoreCount = qBound(5, count, 200);
    }

    QStringList favoriteFolders() const
    {
        return m_favoriteFolders;
    }

    bool addFavoriteFolder(const QString &name)
    {
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty() || m_favoriteFolders.contains(trimmed)) {
            return false;
        }
        m_favoriteFolders.append(trimmed);
        return true;
    }

    bool hasFloatingBarPosition() const
    {
        return m_hasFloatingBarPosition;
    }

    QPoint floatingBarPosition() const
    {
        return m_floatingBarPosition;
    }

    void setFloatingBarPosition(const QPoint &position)
    {
        m_hasFloatingBarPosition = true;
        m_floatingBarPosition = position;
    }

    bool hasResultPopupGeometry() const
    {
        return m_hasResultPopupGeometry;
    }

    QRect resultPopupGeometry() const
    {
        return m_resultPopupGeometry;
    }

    void setResultPopupGeometry(const QRect &geometry)
    {
        if (geometry.width() <= 0 || geometry.height() <= 0) {
            return;
        }
        m_hasResultPopupGeometry = true;
        m_resultPopupGeometry = QRect(
            geometry.topLeft(),
            QSize(qBound(640, geometry.width(), 2000), qBound(460, geometry.height(), 1600))
        );
    }

private:
    QMap<QString, QString> m_hotkeys;
    QMap<QString, QString> m_models;
    QMap<QString, QString> m_outputModes;
    QMap<QString, bool> m_useSelection;
    QMap<QString, bool> m_useVoice;
    QMap<QString, int> m_floatingBarSeconds;
    QMap<QString, int> m_resultPopupSeconds;
    QMap<QString, int> m_countdownSeconds;
    QMap<QString, bool> m_recordingBeepEnabledByFunction;
    QMap<QString, QString> m_recordingBeepPaths;
    QVector<CustomFunctionDef> m_customFunctions;
    QStringList m_functionOrder;
    bool m_trayResident = true;
    bool m_autoStartEnabled = false;
    bool m_strongSelectionEnabled = false;
    bool m_floatingBarEnabled = true;
    bool m_promptLocked = false;
    bool m_dictatePolishEnabled = false;
    bool m_useSystemProxy = false;
    bool m_preRecordCountdownEnabled = false;
    bool m_recordingBeepEnabled = false;
    QString m_speechProvider = speechProviderBaidu();
    QString m_recordDirectory;
    int m_historyInitialLoadCount = 12;
    int m_historyLoadMoreCount = 25;
    QStringList m_favoriteFolders;
    bool m_hasFloatingBarPosition = false;
    QPoint m_floatingBarPosition;
    bool m_hasResultPopupGeometry = false;
    QRect m_resultPopupGeometry;
};

// 界面样式工具：统一字体、卡片、按钮和窗口位置，减少各个页面重复写样式。
static QFont appFont(int pointSize = 10, int weight = QFont::Normal)
{
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

static QString cardStyle()
{
    return QStringLiteral(
        "QFrame#card {"
        "  background: #ffffff;"
        "  border: 1px solid #e4e7ec;"
        "  border-radius: 8px;"
        "}"
    );
}

static QString buttonStyle(const QString &bg, const QString &fg = QStringLiteral("#ffffff"))
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 12px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background: %3;"
        "}"
    ).arg(bg, fg, bg == QStringLiteral("#ffffff") ? QStringLiteral("#f3f4f6") : QStringLiteral("#263244"));
}

static QString compactButtonStyle(const QString &bg, const QString &fg = QStringLiteral("#ffffff"))
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  color: %2;"
        "  border: %3;"
        "  border-radius: 6px;"
        "  padding: 0 12px;"
        "  margin: 0;"
        "  font-weight: 600;"
        "  min-height: 40px;"
        "}"
        "QPushButton:hover {"
        "  background: %4;"
        "}"
    ).arg(
        bg,
        fg,
        bg == QStringLiteral("#ffffff") ? QStringLiteral("1px solid #e4e7ec") : QStringLiteral("none"),
        bg == QStringLiteral("#ffffff") ? QStringLiteral("#f3f4f6") : QStringLiteral("#263244")
    );
}

static QMenu *recordDirectoryOpenMenu(QWidget *parent, QObject *receiver, const std::function<QString()> &recordDirectoryProvider)
{
    auto *menu = new QMenu(parent);
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: #ffffff; border: 1px solid #d0d5dd; padding: 6px; }"
        "QMenu::item { padding: 8px 28px 8px 12px; color: #111827; }"
        "QMenu::item:selected { background: #eef2ff; }"
    ));

    QAction *current = menu->addAction(tr8("当前保存目录"));
    QObject::connect(current, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        ensureHistoryRootStructure(recordDirectoryProvider());
        openDirectoryPath(historyRootPath(recordDirectoryProvider()));
    });

    QAction *backup = menu->addAction(tr8("备份文件"));
    QObject::connect(backup, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        ensureHistoryRootStructure(recordDirectoryProvider());
        openDirectoryPath(historyBackupDirectory(recordDirectoryProvider()));
    });

    QAction *todayAudio = menu->addAction(tr8("今天总录音目录"));
    QObject::connect(todayAudio, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        openDirectoryPath(recordDirectoryForDate(recordDirectoryProvider()));
    });

    QAction *todayText = menu->addAction(tr8("今天总文本目录"));
    QObject::connect(todayText, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        openDirectoryPath(historyAllDateDirectory(recordDirectoryProvider(), historyAllTextFolderName(), QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    });

    QAction *todayDetail = menu->addAction(tr8("今天总详细记录目录"));
    QObject::connect(todayDetail, &QAction::triggered, receiver, [recordDirectoryProvider]() {
        openDirectoryPath(historyAllDateDirectory(recordDirectoryProvider(), historyAllDetailFolderName(), QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));
    });

    QAction *defaultDir = menu->addAction(tr8("默认保存目录"));
    QObject::connect(defaultDir, &QAction::triggered, receiver, []() {
        ensureHistoryRootStructure(defaultRecordDirectory());
        openDirectoryPath(defaultRecordDirectory());
    });

    return menu;
}

static QPoint clampedTopLeftToScreen(const QPoint &topLeft, const QSize &size)
{
    int screenNumber = QApplication::desktop()->screenNumber(topLeft);
    if (screenNumber < 0) {
        screenNumber = QApplication::desktop()->screenNumber(QCursor::pos());
    }
    const QRect screen = QApplication::desktop()->availableGeometry(screenNumber);
    const int maxX = qMax(screen.left(), screen.right() - size.width() + 1);
    const int maxY = qMax(screen.top(), screen.bottom() - size.height() + 1);
    return QPoint(
        qBound(screen.left(), topLeft.x(), maxX),
        qBound(screen.top(), topLeft.y(), maxY)
    );
}

// 主页功能卡片：支持拖动排序，用户可以调整听写、翻译、问答和自定义功能的显示顺序。
class ModeCardFrame : public QFrame
{
public:
    explicit ModeCardFrame(const QString &id, QWidget *parent = nullptr)
        : QFrame(parent), m_id(id)
    {
        setAcceptDrops(true);
        setCursor(Qt::OpenHandCursor);
        setToolTip(tr8("双击编辑，拖动调整顺序"));
    }

    void setDropCallback(const std::function<void(const QString &, const QString &, bool)> &callback)
    {
        m_dropCallback = callback;
    }

    void setDoubleClickCallback(const std::function<void()> &callback)
    {
        m_doubleClickCallback = callback;
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragStartPosition = event->pos();
            setCursor(Qt::ClosedHandCursor);
        }
        QFrame::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        setCursor(Qt::OpenHandCursor);
        QFrame::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_doubleClickCallback) {
            m_doubleClickCallback();
            event->accept();
            return;
        }
        QFrame::mouseDoubleClickEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton)) {
            QFrame::mouseMoveEvent(event);
            return;
        }
        if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
            QFrame::mouseMoveEvent(event);
            return;
        }

        auto *drag = new QDrag(this);
        auto *mime = new QMimeData;
        mime->setData(mimeType(), m_id.toUtf8());
        drag->setMimeData(mime);

        QPixmap pixmap = grab();
        if (!pixmap.isNull()) {
            QPixmap transparent(pixmap.size());
            transparent.fill(Qt::transparent);
            QPainter painter(&transparent);
            painter.setOpacity(0.82);
            painter.drawPixmap(0, 0, pixmap);
            painter.end();
            drag->setPixmap(transparent);
            drag->setHotSpot(event->pos());
        }

        drag->exec(Qt::MoveAction);
        setCursor(Qt::OpenHandCursor);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (acceptsEvent(event)) {
            setDropHighlighted(true);
            event->acceptProposedAction();
            return;
        }
        QFrame::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (acceptsEvent(event)) {
            event->acceptProposedAction();
            return;
        }
        QFrame::dragMoveEvent(event);
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        setDropHighlighted(false);
        QFrame::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        setDropHighlighted(false);
        if (!acceptsEvent(event)) {
            QFrame::dropEvent(event);
            return;
        }

        const QString sourceId = QString::fromUtf8(event->mimeData()->data(mimeType()));
        const bool dropAfter = event->pos().y() > height() / 2 || event->pos().x() > width() / 2;
        if (m_dropCallback) {
            m_dropCallback(sourceId, m_id, dropAfter);
        }
        event->acceptProposedAction();
    }

private:
    static QString mimeType()
    {
        return QStringLiteral("application/x-voiceassistant-function-id");
    }

    bool acceptsEvent(const QDropEvent *event) const
    {
        if (!event || !event->mimeData()->hasFormat(mimeType())) {
            return false;
        }
        const QString sourceId = QString::fromUtf8(event->mimeData()->data(mimeType()));
        return !sourceId.isEmpty() && sourceId != m_id;
    }

    void setDropHighlighted(bool highlighted)
    {
        setStyleSheet(cardStyle() + (highlighted
            ? QStringLiteral("QFrame#card { border: 2px solid #2563eb; background: #f8fbff; }")
            : QString()));
    }

    QString m_id;
    QPoint m_dragStartPosition;
    std::function<void(const QString &, const QString &, bool)> m_dropCallback;
    std::function<void()> m_doubleClickCallback;
};

// 历史记录行：点击整行打开详情，同时保留右侧“操作”菜单和批量选择。
class HistoryRowFrame : public QFrame
{
public:
    explicit HistoryRowFrame(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

    void setClickCallback(const std::function<void()> &callback)
    {
        m_clickCallback = callback;
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && m_clickCallback) {
            m_clickCallback();
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    std::function<void()> m_clickCallback;
};

// 接口密钥结构：集中保存语音识别和大模型服务的密钥，不把密钥散落在业务代码里。
struct SecretConfig
{
    QString deepseekApiKey;
    QString openaiApiKey;
    QString anthropicApiKey;
    QString baiduApiKey;
    QString baiduSecretKey;
    QString baiduAppId;
    QString xfyunAppId;
    QString xfyunApiKey;
    QString xfyunApiSecret;

    bool hasDeepSeek() const { return !deepseekApiKey.trimmed().isEmpty(); }
    bool hasOpenAI() const { return !openaiApiKey.trimmed().isEmpty(); }
    bool hasAnthropic() const { return !anthropicApiKey.trimmed().isEmpty(); }
    bool hasBaidu() const { return !baiduApiKey.trimmed().isEmpty() && !baiduSecretKey.trimmed().isEmpty(); }
    bool hasXfyun() const
    {
        return !xfyunAppId.trimmed().isEmpty()
            && !xfyunApiKey.trimmed().isEmpty()
            && !xfyunApiSecret.trimmed().isEmpty();
    }
};

static QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

static bool writeTextFile(const QString &path, const QString &text)
{
    QFileInfo info(path);
    QDir dir = info.dir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(text.toUtf8());
    return true;
}

// 提示词目标：把左侧“提示词”页和设置页里的提示词编辑绑定到同一份文件或配置。
struct PromptTargetInfo
{
    PromptTargetInfo() : custom(false) {}
    PromptTargetInfo(const QString &targetId, const QString &targetTitle, const QString &targetFileName, const QString &targetFallback, bool isCustom)
        : id(targetId), title(targetTitle), fileName(targetFileName), fallback(targetFallback), custom(isCustom)
    {
    }

    QString id;
    QString title;
    QString fileName;
    QString fallback;
    bool custom;
};

static QVector<PromptTargetInfo> sharedPromptTargets(AppSettings *settings)
{
    QVector<PromptTargetInfo> targets;
    targets.append(PromptTargetInfo(QStringLiteral("dictate"), tr8("听写提示词"), QStringLiteral("asr.txt"), tr8("整理语音识别文本，只输出可直接粘贴的结果。"), false));
    targets.append(PromptTargetInfo(QStringLiteral("translate"), tr8("翻译提示词"), QStringLiteral("translate.txt"), tr8("翻译为简体中文，只输出翻译结果。"), false));
    targets.append(PromptTargetInfo(QStringLiteral("ask"), tr8("问答提示词"), QStringLiteral("qa.txt"), tr8("基于选中文本回答用户问题。"), false));
    if (settings) {
        for (const CustomFunctionDef &fn : settings->customFunctions()) {
            targets.append(PromptTargetInfo(fn.id, fn.name + tr8("提示词"), QString(), fn.prompt, true));
        }
    }
    return targets;
}

static PromptTargetInfo sharedPromptTargetForId(AppSettings *settings, const QString &id)
{
    const QVector<PromptTargetInfo> targets = sharedPromptTargets(settings);
    for (const PromptTargetInfo &target : targets) {
        if (target.id == id) {
            return target;
        }
    }
    return targets.isEmpty() ? PromptTargetInfo() : targets.first();
}

static QString sharedPromptText(AppSettings *settings, const PromptTargetInfo &target)
{
    QString text;
    if (target.custom) {
        if (settings) {
            for (const CustomFunctionDef &fn : settings->customFunctions()) {
                if (fn.id == target.id) {
                    text = fn.prompt;
                    break;
                }
            }
        }
    } else if (!target.fileName.trimmed().isEmpty()) {
        text = readTextFile(QDir(appBasePath()).filePath(QStringLiteral("prompts/") + target.fileName)).trimmed();
    }
    return text.trimmed().isEmpty() ? target.fallback : text;
}

static bool saveSharedPromptText(AppSettings *settings, const PromptTargetInfo &target, const QString &text, QString *error)
{
    if (target.custom) {
        if (!settings) {
            if (error) {
                *error = tr8("设置对象不可用。");
            }
            return false;
        }
        bool found = false;
        for (CustomFunctionDef fn : settings->customFunctions()) {
            if (fn.id == target.id) {
                fn.prompt = text;
                settings->updateCustomFunction(fn);
                found = true;
                break;
            }
        }
        if (!found) {
            if (error) {
                *error = tr8("没有找到对应的自定义功能。");
            }
            return false;
        }
        if (!settings->save()) {
            if (error) {
                *error = tr8("无法写入 config/settings.json。");
            }
            return false;
        }
        return true;
    }

    const QString path = QDir(appBasePath()).filePath(QStringLiteral("prompts/") + target.fileName);
    if (!writeTextFile(path, text)) {
        if (error) {
            *error = tr8("无法写入提示词文件。");
        }
        return false;
    }
    return true;
}

static SecretConfig loadSecrets()
{
    SecretConfig secrets;
    const QString jsonPath = QDir(appBasePath()).filePath(QStringLiteral("config/secrets.json"));
    QFile jsonFile(jsonPath);
    if (jsonFile.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(jsonFile.readAll()).object();
        secrets.deepseekApiKey = root.value(QStringLiteral("deepseek_api_key")).toString();
        secrets.openaiApiKey = root.value(QStringLiteral("openai_api_key")).toString();
        secrets.anthropicApiKey = root.value(QStringLiteral("anthropic_api_key")).toString();
        secrets.baiduApiKey = root.value(QStringLiteral("baidu_api_key")).toString();
        secrets.baiduSecretKey = root.value(QStringLiteral("baidu_secret_key")).toString();
        secrets.baiduAppId = root.value(QStringLiteral("baidu_app_id")).toString();
        secrets.xfyunAppId = root.value(QStringLiteral("xfyun_app_id")).toString();
        secrets.xfyunApiKey = root.value(QStringLiteral("xfyun_api_key")).toString();
        secrets.xfyunApiSecret = root.value(QStringLiteral("xfyun_api_secret")).toString();
    }
    return secrets;
}

static bool saveSecrets(const SecretConfig &secrets)
{
    QDir dir(appBasePath());
    if (!dir.exists(QStringLiteral("config"))) {
        dir.mkpath(QStringLiteral("config"));
    }

    QJsonObject root;
    root.insert(QStringLiteral("deepseek_api_key"), secrets.deepseekApiKey.trimmed());
    root.insert(QStringLiteral("openai_api_key"), secrets.openaiApiKey.trimmed());
    root.insert(QStringLiteral("anthropic_api_key"), secrets.anthropicApiKey.trimmed());
    root.insert(QStringLiteral("baidu_api_key"), secrets.baiduApiKey.trimmed());
    root.insert(QStringLiteral("baidu_secret_key"), secrets.baiduSecretKey.trimmed());
    root.insert(QStringLiteral("baidu_app_id"), secrets.baiduAppId.trimmed());
    root.insert(QStringLiteral("xfyun_app_id"), secrets.xfyunAppId.trimmed());
    root.insert(QStringLiteral("xfyun_api_key"), secrets.xfyunApiKey.trimmed());
    root.insert(QStringLiteral("xfyun_api_secret"), secrets.xfyunApiSecret.trimmed());

    QFile file(dir.filePath(QStringLiteral("config/secrets.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

// 提示词读取：每个功能使用独立提示词文件，缺失时回退到内置默认提示词。
static QString promptText(const QString &fileName, const QString &fallback)
{
    const QString path = QDir(appBasePath()).filePath(QStringLiteral("prompts/") + fileName);
    const QString text = readTextFile(path).trimmed();
    return text.isEmpty() ? fallback : text;
}

static QByteArray wavFromPcm(const QByteArray &pcm, int sampleRate, int channels, int bitsPerSample)
{
    QByteArray wav;
    QDataStream stream(&wav, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    const int byteRate = sampleRate * channels * bitsPerSample / 8;
    const int blockAlign = channels * bitsPerSample / 8;
    const int dataSize = pcm.size();

    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + dataSize);
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << quint16(1);
    stream << quint16(channels);
    stream << quint32(sampleRate);
    stream << quint32(byteRate);
    stream << quint16(blockAlign);
    stream << quint16(bitsPerSample);
    stream.writeRawData("data", 4);
    stream << quint32(dataSize);
    wav.append(pcm);
    return wav;
}

// Windows 键盘模拟和 UI Automation：用于读取选中文字、粘贴结果和替换选区。
static void sendCtrlKey(WORD key)
{
#ifdef Q_OS_WIN
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = key;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
#else
    Q_UNUSED(key);
#endif
}

static void sendSingleKey(WORD key)
{
#ifdef Q_OS_WIN
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = key;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
#else
    Q_UNUSED(key);
#endif
}

#ifdef Q_OS_WIN
template <typename T>
static void releaseComObject(T *&object)
{
    if (object) {
        object->Release();
        object = nullptr;
    }
}

static QString textFromBstr(BSTR value)
{
    if (!value) {
        return QString();
    }
    return QString::fromWCharArray(value, static_cast<int>(SysStringLen(value)));
}

static QString selectedTextFromAutomationElement(IUIAutomationElement *element)
{
    if (!element) {
        return QString();
    }

    IUnknown *patternUnknown = nullptr;
    HRESULT hr = element->GetCurrentPattern(kUiaTextPatternId, &patternUnknown);
    if (FAILED(hr) || !patternUnknown) {
        return QString();
    }

    IUIAutomationTextPattern *textPattern = nullptr;
    hr = patternUnknown->QueryInterface(kIidIUiAutomationTextPattern, reinterpret_cast<void **>(&textPattern));
    releaseComObject(patternUnknown);
    if (FAILED(hr) || !textPattern) {
        return QString();
    }

    IUIAutomationTextRangeArray *selection = nullptr;
    hr = textPattern->GetSelection(&selection);
    releaseComObject(textPattern);
    if (FAILED(hr) || !selection) {
        return QString();
    }

    int length = 0;
    selection->get_Length(&length);
    QString result;
    for (int i = 0; i < length; ++i) {
        IUIAutomationTextRange *range = nullptr;
        if (SUCCEEDED(selection->GetElement(i, &range)) && range) {
            BSTR text = nullptr;
            if (SUCCEEDED(range->GetText(-1, &text)) && text) {
                result += textFromBstr(text);
                SysFreeString(text);
            }
            releaseComObject(range);
        }
    }
    releaseComObject(selection);
    return result;
}

static QString selectedTextViaUiAutomation()
{
    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return QString();
    }

    IUIAutomation *automation = nullptr;
    HRESULT hr = CoCreateInstance(
        kClsidCuiAutomation,
        nullptr,
        CLSCTX_INPROC_SERVER,
        kIidIUiAutomation,
        reinterpret_cast<void **>(&automation)
    );
    if (FAILED(hr) || !automation) {
        if (shouldUninitialize) {
            CoUninitialize();
        }
        return QString();
    }

    QString result;
    IUIAutomationElement *focused = nullptr;
    if (SUCCEEDED(automation->GetFocusedElement(&focused)) && focused) {
        result = selectedTextFromAutomationElement(focused);
    }
    releaseComObject(focused);

    if (result.trimmed().isEmpty()) {
        POINT cursorPoint;
        if (GetCursorPos(&cursorPoint)) {
            IUIAutomationElement *element = nullptr;
            if (SUCCEEDED(automation->ElementFromPoint(cursorPoint, &element)) && element) {
                IUIAutomationTreeWalker *walker = nullptr;
                automation->get_RawViewWalker(&walker);
                for (int depth = 0; depth < 12 && element && result.trimmed().isEmpty(); ++depth) {
                    result = selectedTextFromAutomationElement(element);
                    if (!result.trimmed().isEmpty()) {
                        break;
                    }
                    if (!walker) {
                        break;
                    }
                    IUIAutomationElement *parent = nullptr;
                    if (FAILED(walker->GetParentElement(element, &parent)) || !parent) {
                        break;
                    }
                    releaseComObject(element);
                    element = parent;
                }
                releaseComObject(element);
                releaseComObject(walker);
            }
        }
    }

    releaseComObject(automation);
    if (shouldUninitialize) {
        CoUninitialize();
    }
    return result.trimmed();
}
#endif

struct ClipboardSnapshot
{
    QString text;
    QString html;
    QList<QUrl> urls;
    QVariant imageData;
    bool hasText = false;
    bool hasHtml = false;
    bool hasUrls = false;
    bool hasImage = false;
};

static ClipboardSnapshot captureClipboardSnapshot(const QMimeData *source)
{
    ClipboardSnapshot snapshot;
    if (!source) {
        return snapshot;
    }
    snapshot.hasText = source->hasText();
    if (snapshot.hasText) {
        snapshot.text = source->text();
    }
    snapshot.hasHtml = source->hasHtml();
    if (snapshot.hasHtml) {
        snapshot.html = source->html();
    }
    snapshot.hasUrls = source->hasUrls();
    if (snapshot.hasUrls) {
        snapshot.urls = source->urls();
    }
    snapshot.hasImage = source->hasImage();
    if (snapshot.hasImage) {
        snapshot.imageData = source->imageData();
    }
    return snapshot;
}

static QMimeData *mimeDataFromClipboardSnapshot(const ClipboardSnapshot &snapshot)
{
    auto *data = new QMimeData;
    if (snapshot.hasText) {
        data->setText(snapshot.text);
    }
    if (snapshot.hasHtml) {
        data->setHtml(snapshot.html);
    }
    if (snapshot.hasUrls) {
        data->setUrls(snapshot.urls);
    }
    if (snapshot.hasImage) {
        data->setImageData(snapshot.imageData);
    }
    return data;
}

static QString selectedTextViaClipboardCopy(NativeWindowHandle window)
{
#ifdef Q_OS_WIN
    if (window) {
        SetForegroundWindow(window);
        QThread::msleep(90);
        QApplication::processEvents();
    }

    QClipboard *clipboard = QApplication::clipboard();
    const ClipboardSnapshot previous = captureClipboardSnapshot(clipboard->mimeData());
    const QString sentinel = QStringLiteral("__VOICE_ASSISTANT_SELECTION_SENTINEL__") + QUuid::createUuid().toString();

    clipboard->setText(sentinel);
    QApplication::processEvents();
    QThread::msleep(60);

    sendCtrlKey('C');
    QString result;
    for (int i = 0; i < 10; ++i) {
        QThread::msleep(40);
        QApplication::processEvents();
        result = clipboard->text();
        if (result != sentinel) {
            break;
        }
    }

    clipboard->setMimeData(mimeDataFromClipboardSnapshot(previous));
    if (result == sentinel) {
        return QString();
    }
    return result.trimmed();
#else
    Q_UNUSED(window);
    return QString();
#endif
}

// 剪贴板桥接：封装选中文字读取和结果写入，尽量恢复用户原来的剪贴板内容。
class ClipboardBridge
{
public:
    static QString selectedText(bool strongSelectionEnabled = false, NativeWindowHandle window = nullptr)
    {
#ifdef Q_OS_WIN
        const QString normalResult = selectedTextViaUiAutomation();
        if (!normalResult.trimmed().isEmpty() || !strongSelectionEnabled) {
            return normalResult;
        }
        return selectedTextViaClipboardCopy(window);
#else
        Q_UNUSED(strongSelectionEnabled);
        Q_UNUSED(window);
        return QString();
#endif
    }

    static void pasteText(const QString &text)
    {
        QClipboard *clipboard = QApplication::clipboard();
        const QString previous = clipboard->text();
        clipboard->setText(text);
        QThread::msleep(60);
        sendCtrlKey('V');
        QTimer::singleShot(500, [previous]() {
            QApplication::clipboard()->setText(previous);
        });
    }

    static void pasteTextToWindow(const QString &text, NativeWindowHandle window, bool replaceSelection = true, bool hasSelection = false)
    {
#ifdef Q_OS_WIN
        if (window) {
            SetForegroundWindow(window);
            QThread::msleep(100);
            QApplication::processEvents();
        }
        if (!replaceSelection && hasSelection) {
            sendSingleKey(VK_RIGHT);
            QThread::msleep(80);
            QApplication::processEvents();
        }
#else
        Q_UNUSED(window);
        Q_UNUSED(replaceSelection);
        Q_UNUSED(hasSelection);
#endif
        pasteText(text);
    }
};

// 音频采集设备：边写入 PCM 数据边计算峰值，用来驱动浮动条里的声音波形。
class AudioCaptureDevice : public QIODevice
{
public:
    explicit AudioCaptureDevice(QFile *file, QObject *parent = nullptr)
        : QIODevice(parent), m_file(file)
    {
    }

    bool open(OpenMode mode) override
    {
        return QIODevice::open(mode);
    }

    int takePeakLevel()
    {
        const int value = m_peak;
        m_peak = 0;
        return value;
    }

protected:
    qint64 readData(char *, qint64) override
    {
        return -1;
    }

    qint64 writeData(const char *data, qint64 len) override
    {
        if (!m_file || !m_file->isOpen()) {
            return -1;
        }
        const QByteArray pcm(data, static_cast<int>(len));
        m_peak = qMax(m_peak, pcm16PeakLevel(pcm));
        return m_file->write(data, len);
    }

private:
    QFile *m_file = nullptr;
    int m_peak = 0;
};

// 录音器：负责启动麦克风、保存 PCM/WAV 文件，并把音频交给语音识别接口。
class AudioRecorder
{
public:
    AudioRecorder() {}
    ~AudioRecorder() { stop(); }

    bool isRecording() const { return m_audioInput != nullptr; }
    QString lastWavPath() const { return m_lastWavPath; }

    bool start(const QString &modeName, const QString &recordDirectory, QString *error)
    {
        if (m_audioInput) {
            return true;
        }

        QAudioFormat format;
        format.setSampleRate(16000);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec(QStringLiteral("audio/pcm"));
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
        if (!info.isFormatSupported(format)) {
            format = info.nearestFormat(format);
            if (format.sampleRate() != 16000 || format.channelCount() != 1 || format.sampleSize() != 16) {
                if (error) {
                    *error = tr8("当前麦克风不支持 16k 单声道录音。");
                }
                return false;
            }
        }

        const QString date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        ensureHistoryModeDateStructure(recordDirectory, modeName, date);
        QDir dir(historyModeDateSubDirectory(recordDirectory, modeName, date, historyAudioSubFolderName()));
        const QString safeMode = QString(modeName).replace(QRegExp(QStringLiteral("[^A-Za-z0-9_\\u4e00-\\u9fa5]")), QStringLiteral("_"));
        m_rawPath = dir.filePath(QDateTime::currentDateTime().toString(QStringLiteral("HHmmss_")) + safeMode + QStringLiteral(".pcm"));
        m_lastWavPath = m_rawPath;
        m_lastWavPath.replace(QStringLiteral(".pcm"), QStringLiteral(".wav"));

        m_file = new QFile(m_rawPath);
        if (!m_file->open(QIODevice::WriteOnly)) {
            if (error) {
                *error = tr8("无法创建录音文件。");
            }
            delete m_file;
            m_file = nullptr;
            return false;
        }

        m_capture = new AudioCaptureDevice(m_file);
        m_capture->open(QIODevice::WriteOnly);

        m_audioInput = new QAudioInput(format);
        m_audioInput->start(m_capture);
        m_timer.start();
        return true;
    }

    int takePeakLevel()
    {
        return m_capture ? m_capture->takePeakLevel() : 0;
    }

    QByteArray stop()
    {
        if (!m_audioInput) {
            return QByteArray();
        }

        m_audioInput->stop();
        delete m_audioInput;
        m_audioInput = nullptr;

        if (m_capture) {
            m_capture->close();
            delete m_capture;
            m_capture = nullptr;
        }

        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }

        QFile raw(m_rawPath);
        QByteArray pcm;
        if (raw.open(QIODevice::ReadOnly)) {
            pcm = raw.readAll();
            raw.close();
            raw.remove();
        }

        QFile wav(m_lastWavPath);
        if (wav.open(QIODevice::WriteOnly)) {
            wav.write(wavFromPcm(pcm, 16000, 1, 16));
            wav.close();
        }
        return pcm;
    }

private:
    QAudioInput *m_audioInput = nullptr;
    AudioCaptureDevice *m_capture = nullptr;
    QFile *m_file = nullptr;
    QString m_rawPath;
    QString m_lastWavPath;
    QElapsedTimer m_timer;
};

static QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
{
    const int blockSize = 64;
    QByteArray normalizedKey = key;
    if (normalizedKey.size() > blockSize) {
        normalizedKey = QCryptographicHash::hash(normalizedKey, QCryptographicHash::Sha256);
    }
    normalizedKey = normalizedKey.leftJustified(blockSize, '\0', true);

    QByteArray innerPad(blockSize, char(0x36));
    QByteArray outerPad(blockSize, char(0x5c));
    for (int i = 0; i < blockSize; ++i) {
        innerPad[i] = char(innerPad.at(i) ^ normalizedKey.at(i));
        outerPad[i] = char(outerPad.at(i) ^ normalizedKey.at(i));
    }
    const QByteArray innerHash = QCryptographicHash::hash(innerPad + message, QCryptographicHash::Sha256);
    return QCryptographicHash::hash(outerPad + innerHash, QCryptographicHash::Sha256);
}

// API 客户端：统一封装百度/讯飞语音识别，以及 DeepSeek、OpenAI、Claude 大模型请求。
class ApiClient
{
public:
    ApiClient()
    {
        setUseSystemProxy(false);
    }

    void reloadSecrets()
    {
        m_secrets = loadSecrets();
        m_baiduToken.clear();
        m_tokenExpire = QDateTime();
    }

    void setUseSystemProxy(bool enabled)
    {
        m_useSystemProxy = enabled;
        if (m_useSystemProxy) {
            QNetworkProxyFactory::setUseSystemConfiguration(true);
            m_network.setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
        } else {
            QNetworkProxyFactory::setUseSystemConfiguration(false);
            m_network.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        }
    }

    bool hasBaidu() const { return m_secrets.hasBaidu(); }
    bool hasXfyun() const { return m_secrets.hasXfyun(); }
    bool hasDeepSeek() const { return m_secrets.hasDeepSeek(); }
    bool hasOpenAI() const { return m_secrets.hasOpenAI(); }
    bool hasAnthropic() const { return m_secrets.hasAnthropic(); }
    QString speechProviderConfigurationError(const QString &provider) const
    {
        if (normalizeSpeechProvider(provider) == speechProviderXfyun()) {
            return m_secrets.hasXfyun()
                ? QString()
                : tr8("缺少讯飞语音听写密钥。请在“设置 -> 接口”中填写讯飞 AppID、API Key 和 API Secret。");
        }
        return m_secrets.hasBaidu()
            ? QString()
            : tr8("缺少百度语音识别密钥。请在“设置 -> 接口”中填写百度 API Key 和 Secret Key。");
    }

    bool hasModelProvider(const QString &model) const
    {
        const QString provider = modelProvider(model);
        if (provider == QStringLiteral("openai")) {
            return m_secrets.hasOpenAI();
        }
        if (provider == QStringLiteral("claude")) {
            return m_secrets.hasAnthropic();
        }
        return m_secrets.hasDeepSeek();
    }

    QString testBaiduCredential(QString *error)
    {
        if (!m_secrets.hasBaidu()) {
            if (error) {
                *error = tr8("未填写百度 API Key 或 Secret Key。");
            }
            return QString();
        }
        const QString token = baiduAccessToken(error);
        return token.isEmpty() ? QString() : tr8("百度令牌获取成功。");
    }

    QString testXfyunCredential(QString *error)
    {
        if (!m_secrets.hasXfyun()) {
            if (error) {
                *error = tr8("未填写讯飞 AppID、API Key 或 API Secret。");
            }
            return QString();
        }

        QString requestError;
        const QByteArray silence(16000 * 2, char(0));
        const QString result = xfyunAsr(silence, &requestError);
        if (!result.trimmed().isEmpty()) {
            return tr8("讯飞连接、鉴权和识别返回成功。");
        }
        if (requestError.contains(tr8("没有识别到语音"))) {
            return tr8("讯飞连接和鉴权成功，静音测试未识别到语音。");
        }
        if (error) {
            *error = requestError;
        }
        return QString();
    }

    QString testModelProvider(const QString &provider, QString *error)
    {
        QString model;
        if (provider == QStringLiteral("openai")) {
            if (!m_secrets.hasOpenAI()) {
                if (error) {
                    *error = tr8("未填写 OpenAI API Key。");
                }
                return QString();
            }
            model = QStringLiteral("openai:gpt-5.5");
        } else if (provider == QStringLiteral("claude")) {
            if (!m_secrets.hasAnthropic()) {
                if (error) {
                    *error = tr8("未填写 Anthropic API Key。");
                }
                return QString();
            }
            model = QStringLiteral("claude:claude-haiku-4-5");
        } else {
            if (!m_secrets.hasDeepSeek()) {
                if (error) {
                    *error = tr8("未填写 DeepSeek API Key。");
                }
                return QString();
            }
            model = QStringLiteral("deepseek-v4-flash");
        }

        QString requestError;
        const QString result = chatCompletion(
            model,
            tr8("你是接口自检助手。只回复 OK。"),
            tr8("请只回复 OK，用于确认接口可用。"),
            &requestError
        );
        if (result.trimmed().isEmpty()) {
            if (error) {
                *error = compactDiagnosticError(requestError);
            }
            return QString();
        }
        return tr8("模型接口返回成功。");
    }

    QString speechAsr(const QString &provider, const QByteArray &pcm, QString *error)
    {
        if (normalizeSpeechProvider(provider) == speechProviderXfyun()) {
            return xfyunAsr(pcm, error);
        }
        return baiduAsr(pcm, error);
    }

    QString baiduAsr(const QByteArray &pcm, QString *error)
    {
        if (!m_secrets.hasBaidu()) {
            if (error) {
                *error = speechProviderConfigurationError(speechProviderBaidu());
            }
            return QString();
        }
        if (pcm.isEmpty()) {
            if (error) {
                *error = tr8("录音为空。");
            }
            return QString();
        }

        const QString token = baiduAccessToken(error);
        if (token.isEmpty()) {
            return QString();
        }

        QJsonObject body;
        body.insert(QStringLiteral("format"), QStringLiteral("pcm"));
        body.insert(QStringLiteral("rate"), 16000);
        body.insert(QStringLiteral("channel"), 1);
        body.insert(QStringLiteral("cuid"), QHostInfo::localHostName().isEmpty() ? QStringLiteral("voicedock") : QHostInfo::localHostName());
        body.insert(QStringLiteral("token"), token);
        body.insert(QStringLiteral("len"), pcm.size());
        body.insert(QStringLiteral("speech"), QString::fromLatin1(pcm.toBase64()));
        body.insert(QStringLiteral("dev_pid"), 1537);

        QNetworkRequest request(QUrl(QStringLiteral("https://vop.baidu.com/server_api")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QByteArray response = postJson(request, QJsonDocument(body).toJson(QJsonDocument::Compact), error, 70000);
        if (response.isEmpty()) {
            return QString();
        }

        const QJsonObject root = QJsonDocument::fromJson(response).object();
        const int errNo = root.value(QStringLiteral("err_no")).toInt(-1);
        if (errNo != 0) {
            if (error) {
                *error = tr8("百度识别失败：") + root.value(QStringLiteral("err_msg")).toString(QString::number(errNo));
            }
            return QString();
        }
        const QJsonArray result = root.value(QStringLiteral("result")).toArray();
        return result.isEmpty() ? QString() : result.first().toString();
    }

    QString xfyunAsr(const QByteArray &pcm, QString *error)
    {
        if (!m_secrets.hasXfyun()) {
            if (error) {
                *error = speechProviderConfigurationError(speechProviderXfyun());
            }
            return QString();
        }
        if (pcm.isEmpty()) {
            if (error) {
                *error = tr8("录音为空。");
            }
            return QString();
        }

        const QString host = QStringLiteral("iat-api.xfyun.cn");
        const QString path = QStringLiteral("/v2/iat");
        const QString date = QLocale::c().toString(
            QDateTime::currentDateTimeUtc(),
            QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'")
        );
        const QByteArray signatureOrigin = (
            QStringLiteral("host: ") + host
            + QStringLiteral("\ndate: ") + date
            + QStringLiteral("\nGET ") + path + QStringLiteral(" HTTP/1.1")
        ).toUtf8();
        const QByteArray signature = hmacSha256(m_secrets.xfyunApiSecret.toUtf8(), signatureOrigin).toBase64();
        const QByteArray authorizationOrigin = (
            QStringLiteral("api_key=\"") + m_secrets.xfyunApiKey
            + QStringLiteral("\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"")
            + QString::fromLatin1(signature) + QStringLiteral("\"")
        ).toUtf8();

        QUrl url(QStringLiteral("wss://") + host + path);
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("authorization"), QString::fromLatin1(authorizationOrigin.toBase64()));
        query.addQueryItem(QStringLiteral("date"), date);
        query.addQueryItem(QStringLiteral("host"), host);
        url.setQuery(query);

        QWebSocket socket;
        socket.setProxy(m_useSystemProxy ? QNetworkProxy(QNetworkProxy::DefaultProxy) : QNetworkProxy(QNetworkProxy::NoProxy));

        QEventLoop loop;
        QTimer timeout;
        QTimer frameTimer;
        timeout.setSingleShot(true);
        timeout.setInterval(75000);
        frameTimer.setInterval(40);

        int offset = 0;
        bool firstFrame = true;
        bool finalFrameSent = false;
        bool completed = false;
        QString requestError;
        QStringList resultParts;

        auto sendFrame = [&]() {
            if (socket.state() != QAbstractSocket::ConnectedState || finalFrameSent) {
                return;
            }

            const int status = firstFrame ? 0 : (offset >= pcm.size() ? 2 : 1);
            const QByteArray chunk = status == 2 ? QByteArray() : pcm.mid(offset, 1280);
            offset += chunk.size();

            QJsonObject root;
            if (status == 0) {
                QJsonObject common;
                common.insert(QStringLiteral("app_id"), m_secrets.xfyunAppId);
                root.insert(QStringLiteral("common"), common);

                QJsonObject business;
                business.insert(QStringLiteral("language"), QStringLiteral("zh_cn"));
                business.insert(QStringLiteral("domain"), QStringLiteral("iat"));
                business.insert(QStringLiteral("accent"), QStringLiteral("mandarin"));
                business.insert(QStringLiteral("vad_eos"), 10000);
                root.insert(QStringLiteral("business"), business);
            }

            QJsonObject data;
            data.insert(QStringLiteral("status"), status);
            data.insert(QStringLiteral("format"), QStringLiteral("audio/L16;rate=16000"));
            data.insert(QStringLiteral("encoding"), QStringLiteral("raw"));
            data.insert(QStringLiteral("audio"), QString::fromLatin1(chunk.toBase64()));
            root.insert(QStringLiteral("data"), data);
            socket.sendTextMessage(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));

            firstFrame = false;
            if (status == 2) {
                finalFrameSent = true;
                frameTimer.stop();
            }
        };

        QObject::connect(&socket, &QWebSocket::connected, &loop, [&]() {
            sendFrame();
            frameTimer.start();
        });
        QObject::connect(&frameTimer, &QTimer::timeout, &loop, sendFrame);
        QObject::connect(&socket, &QWebSocket::textMessageReceived, &loop, [&](const QString &message) {
            const QJsonObject root = QJsonDocument::fromJson(message.toUtf8()).object();
            const int code = root.value(QStringLiteral("code")).toInt(-1);
            if (code != 0) {
                requestError = tr8("讯飞识别失败：") + root.value(QStringLiteral("message")).toString(QString::number(code));
                socket.close();
                loop.quit();
                return;
            }

            QString part;
            const QJsonObject data = root.value(QStringLiteral("data")).toObject();
            const QJsonArray words = data.value(QStringLiteral("result")).toObject().value(QStringLiteral("ws")).toArray();
            for (const QJsonValue &wordValue : words) {
                const QJsonArray candidates = wordValue.toObject().value(QStringLiteral("cw")).toArray();
                if (!candidates.isEmpty()) {
                    part += candidates.first().toObject().value(QStringLiteral("w")).toString();
                }
            }
            if (!part.isEmpty()) {
                resultParts.append(part);
            }
            if (data.value(QStringLiteral("status")).toInt() == 2) {
                completed = true;
                socket.close();
                loop.quit();
            }
        });
        QObject::connect(
            &socket,
            static_cast<void (QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error),
            &loop,
            [&](QAbstractSocket::SocketError) {
                requestError = xfyunNetworkErrorMessage(socket.errorString());
                loop.quit();
            }
        );
        QObject::connect(&socket, &QWebSocket::disconnected, &loop, [&]() {
            if (!completed && requestError.isEmpty()) {
                requestError = xfyunNetworkErrorMessage(tr8("远端主机关闭了这个连接"));
            }
            loop.quit();
        });
        QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
            requestError = tr8("讯飞语音听写网络请求超时。");
            socket.abort();
            loop.quit();
        });

        timeout.start();
        socket.open(url);
        loop.exec();
        frameTimer.stop();
        timeout.stop();

        const QString result = resultParts.join(QString()).trimmed();
        if (result.isEmpty() && error) {
            *error = requestError.isEmpty() ? tr8("讯飞没有识别到语音。") : requestError;
        }
        return result;
    }

    QString chatCompletion(const QString &model, const QString &systemPrompt, const QString &userText, QString *error)
    {
        const QString provider = modelProvider(model);
        if (provider == QStringLiteral("openai")) {
            return openaiChat(providerModelId(model), systemPrompt, userText, error);
        }
        if (provider == QStringLiteral("claude")) {
            return claudeChat(providerModelId(model), systemPrompt, userText, error);
        }
        return deepseekChat(model, systemPrompt, userText, error);
    }

    QString chatCompletionStream(
        const QString &model,
        const QString &systemPrompt,
        const QString &userText,
        const std::function<void(const QString &)> &onDelta,
        QString *error
    )
    {
        const QString provider = modelProvider(model);
        if (provider == QStringLiteral("openai")) {
            return openaiChatStream(providerModelId(model), systemPrompt, userText, onDelta, error);
        }
        if (provider == QStringLiteral("claude")) {
            return claudeChatStream(providerModelId(model), systemPrompt, userText, onDelta, error);
        }
        return deepseekChatStream(model, systemPrompt, userText, onDelta, error);
    }

    QString deepseekChat(const QString &model, const QString &systemPrompt, const QString &userText, QString *error)
    {
        if (!m_secrets.hasDeepSeek()) {
            if (error) {
                *error = tr8("缺少 DeepSeek 密钥。请在“设置 -> 接口”中填写 DeepSeek API Key。");
            }
            return QString();
        }

        QJsonArray messages;
        QJsonObject system;
        system.insert(QStringLiteral("role"), QStringLiteral("system"));
        system.insert(QStringLiteral("content"), systemPrompt);
        messages.append(system);
        QJsonObject user;
        user.insert(QStringLiteral("role"), QStringLiteral("user"));
        user.insert(QStringLiteral("content"), userText);
        messages.append(user);

        QJsonObject body;
        body.insert(QStringLiteral("model"), model.isEmpty() ? QStringLiteral("deepseek-v4-flash") : model);
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("temperature"), 0.2);
        body.insert(QStringLiteral("max_tokens"), 1024);
        QJsonObject thinking;
        thinking.insert(QStringLiteral("type"), QStringLiteral("disabled"));
        body.insert(QStringLiteral("thinking"), thinking);

        QNetworkRequest request(QUrl(QStringLiteral("https://api.deepseek.com/chat/completions")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_secrets.deepseekApiKey.toUtf8());

        QString requestError;
        const QByteArray response = postJson(request, QJsonDocument(body).toJson(QJsonDocument::Compact), &requestError, 25000);
        if (response.isEmpty()) {
            if (error) {
                *error = tr8("DeepSeek 模型阶段失败：") + requestError;
            }
            return QString();
        }

        const QJsonObject root = QJsonDocument::fromJson(response).object();
        if (root.contains(QStringLiteral("error"))) {
            const QJsonObject err = root.value(QStringLiteral("error")).toObject();
            if (error) {
                *error = tr8("DeepSeek 调用失败：") + err.value(QStringLiteral("message")).toString();
            }
            return QString();
        }
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            if (error) {
                *error = tr8("DeepSeek 没有返回结果。");
            }
            return QString();
        }
        return choices.first().toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString().trimmed();
    }

private:
    QString deepseekChatStream(const QString &model, const QString &systemPrompt, const QString &userText, const std::function<void(const QString &)> &onDelta, QString *error)
    {
        if (!m_secrets.hasDeepSeek()) {
            if (error) {
                *error = tr8("缺少 DeepSeek 密钥。请在“设置 -> 接口”中填写 DeepSeek API Key。");
            }
            return QString();
        }

        QJsonArray messages;
        QJsonObject system;
        system.insert(QStringLiteral("role"), QStringLiteral("system"));
        system.insert(QStringLiteral("content"), systemPrompt);
        messages.append(system);
        QJsonObject user;
        user.insert(QStringLiteral("role"), QStringLiteral("user"));
        user.insert(QStringLiteral("content"), userText);
        messages.append(user);

        QJsonObject body;
        body.insert(QStringLiteral("model"), model.isEmpty() ? QStringLiteral("deepseek-v4-flash") : model);
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("temperature"), 0.2);
        body.insert(QStringLiteral("max_tokens"), 1024);
        body.insert(QStringLiteral("stream"), true);
        QJsonObject thinking;
        thinking.insert(QStringLiteral("type"), QStringLiteral("disabled"));
        body.insert(QStringLiteral("thinking"), thinking);

        QNetworkRequest request(QUrl(QStringLiteral("https://api.deepseek.com/chat/completions")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "text/event-stream");
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_secrets.deepseekApiKey.toUtf8());

        return openAiCompatibleStream(request, QJsonDocument(body).toJson(QJsonDocument::Compact), onDelta, error, tr8("DeepSeek"));
    }

    QString openaiChat(const QString &model, const QString &systemPrompt, const QString &userText, QString *error)
    {
        if (!m_secrets.hasOpenAI()) {
            if (error) {
                *error = tr8("缺少 OpenAI 密钥。请在“设置 -> 接口”中填写 OpenAI API Key。");
            }
            return QString();
        }

        QJsonArray messages;
        QJsonObject system;
        system.insert(QStringLiteral("role"), QStringLiteral("system"));
        system.insert(QStringLiteral("content"), systemPrompt);
        messages.append(system);
        QJsonObject user;
        user.insert(QStringLiteral("role"), QStringLiteral("user"));
        user.insert(QStringLiteral("content"), userText);
        messages.append(user);

        QJsonObject body;
        body.insert(QStringLiteral("model"), model.trimmed().isEmpty() ? QStringLiteral("gpt-5.5") : model);
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("temperature"), 0.2);
        body.insert(QStringLiteral("max_tokens"), 1024);

        QNetworkRequest request(QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_secrets.openaiApiKey.toUtf8());

        QString requestError;
        const QByteArray response = postJson(request, QJsonDocument(body).toJson(QJsonDocument::Compact), &requestError, 35000);
        if (response.isEmpty()) {
            if (error) {
                *error = tr8("GPT 模型阶段失败：") + requestError;
            }
            return QString();
        }

        const QJsonObject root = QJsonDocument::fromJson(response).object();
        if (root.contains(QStringLiteral("error"))) {
            const QJsonObject err = root.value(QStringLiteral("error")).toObject();
            if (error) {
                *error = tr8("GPT 调用失败：") + err.value(QStringLiteral("message")).toString();
            }
            return QString();
        }
        const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
        if (choices.isEmpty()) {
            if (error) {
                *error = tr8("GPT 没有返回结果。");
            }
            return QString();
        }
        return choices.first().toObject().value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString().trimmed();
    }

    QString openaiChatStream(const QString &model, const QString &systemPrompt, const QString &userText, const std::function<void(const QString &)> &onDelta, QString *error)
    {
        if (!m_secrets.hasOpenAI()) {
            if (error) {
                *error = tr8("缺少 OpenAI 密钥。请在“设置 -> 接口”中填写 OpenAI API Key。");
            }
            return QString();
        }

        QJsonArray messages;
        QJsonObject system;
        system.insert(QStringLiteral("role"), QStringLiteral("system"));
        system.insert(QStringLiteral("content"), systemPrompt);
        messages.append(system);
        QJsonObject user;
        user.insert(QStringLiteral("role"), QStringLiteral("user"));
        user.insert(QStringLiteral("content"), userText);
        messages.append(user);

        QJsonObject body;
        body.insert(QStringLiteral("model"), model.trimmed().isEmpty() ? QStringLiteral("gpt-5.5") : model);
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("temperature"), 0.2);
        body.insert(QStringLiteral("max_tokens"), 1024);
        body.insert(QStringLiteral("stream"), true);

        QNetworkRequest request(QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "text/event-stream");
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_secrets.openaiApiKey.toUtf8());

        return openAiCompatibleStream(request, QJsonDocument(body).toJson(QJsonDocument::Compact), onDelta, error, tr8("GPT"));
    }

    QString claudeChat(const QString &model, const QString &systemPrompt, const QString &userText, QString *error)
    {
        if (!m_secrets.hasAnthropic()) {
            if (error) {
                *error = tr8("缺少 Claude 密钥。请在“设置 -> 接口”中填写 Anthropic API Key。");
            }
            return QString();
        }

        QJsonArray messages;
        QJsonObject user;
        user.insert(QStringLiteral("role"), QStringLiteral("user"));
        user.insert(QStringLiteral("content"), userText);
        messages.append(user);

        QJsonObject body;
        body.insert(QStringLiteral("model"), model.trimmed().isEmpty() ? QStringLiteral("claude-haiku-4-5") : model);
        body.insert(QStringLiteral("system"), systemPrompt);
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("temperature"), 0.2);
        body.insert(QStringLiteral("max_tokens"), 1024);

        QNetworkRequest request(QUrl(QStringLiteral("https://api.anthropic.com/v1/messages")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("x-api-key", m_secrets.anthropicApiKey.toUtf8());
        request.setRawHeader("anthropic-version", QByteArray("2023-06-01"));

        QString requestError;
        const QByteArray response = postJson(request, QJsonDocument(body).toJson(QJsonDocument::Compact), &requestError, 35000);
        if (response.isEmpty()) {
            if (error) {
                *error = tr8("Claude 模型阶段失败：") + requestError;
            }
            return QString();
        }

        const QJsonObject root = QJsonDocument::fromJson(response).object();
        if (root.contains(QStringLiteral("error"))) {
            const QJsonObject err = root.value(QStringLiteral("error")).toObject();
            if (error) {
                *error = tr8("Claude 调用失败：") + err.value(QStringLiteral("message")).toString();
            }
            return QString();
        }

        QStringList parts;
        const QJsonArray content = root.value(QStringLiteral("content")).toArray();
        for (const QJsonValue &value : content) {
            const QJsonObject block = value.toObject();
            if (block.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
                parts.append(block.value(QStringLiteral("text")).toString());
            }
        }
        const QString result = parts.join(QStringLiteral("\n")).trimmed();
        if (result.isEmpty() && error) {
            *error = tr8("Claude 没有返回结果。");
        }
        return result;
    }

    QString claudeChatStream(const QString &model, const QString &systemPrompt, const QString &userText, const std::function<void(const QString &)> &onDelta, QString *error)
    {
        if (!m_secrets.hasAnthropic()) {
            if (error) {
                *error = tr8("缺少 Claude 密钥。请在“设置 -> 接口”中填写 Anthropic API Key。");
            }
            return QString();
        }

        QJsonArray messages;
        QJsonObject user;
        user.insert(QStringLiteral("role"), QStringLiteral("user"));
        user.insert(QStringLiteral("content"), userText);
        messages.append(user);

        QJsonObject body;
        body.insert(QStringLiteral("model"), model.trimmed().isEmpty() ? QStringLiteral("claude-haiku-4-5") : model);
        body.insert(QStringLiteral("system"), systemPrompt);
        body.insert(QStringLiteral("messages"), messages);
        body.insert(QStringLiteral("temperature"), 0.2);
        body.insert(QStringLiteral("max_tokens"), 1024);
        body.insert(QStringLiteral("stream"), true);

        QNetworkRequest request(QUrl(QStringLiteral("https://api.anthropic.com/v1/messages")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Accept", "text/event-stream");
        request.setRawHeader("x-api-key", m_secrets.anthropicApiKey.toUtf8());
        request.setRawHeader("anthropic-version", QByteArray("2023-06-01"));

        QString result;
        QString streamError;
        postJsonEventStream(
            request,
            QJsonDocument(body).toJson(QJsonDocument::Compact),
            [&](const QByteArray &eventData) {
                const QJsonObject root = QJsonDocument::fromJson(eventData).object();
                if (root.contains(QStringLiteral("error"))) {
                    streamError = root.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
                    return;
                }
                if (root.value(QStringLiteral("type")).toString() == QStringLiteral("content_block_delta")) {
                    const QString delta = root.value(QStringLiteral("delta")).toObject().value(QStringLiteral("text")).toString();
                    if (!delta.isEmpty()) {
                        result += delta;
                        if (onDelta) {
                            onDelta(delta);
                        }
                    }
                }
            },
            error,
            90000
        );
        if (!streamError.isEmpty()) {
            if (error) {
                *error = tr8("Claude 调用失败：") + streamError;
            }
            return QString();
        }
        if (result.trimmed().isEmpty() && error && error->trimmed().isEmpty()) {
            *error = tr8("Claude 没有返回结果。");
        }
        return result.trimmed();
    }

    QString openAiCompatibleStream(
        const QNetworkRequest &request,
        const QByteArray &body,
        const std::function<void(const QString &)> &onDelta,
        QString *error,
        const QString &providerName
    )
    {
        QString result;
        QString streamError;
        postJsonEventStream(
            request,
            body,
            [&](const QByteArray &eventData) {
                const QByteArray trimmed = eventData.trimmed();
                if (trimmed == QByteArray("[DONE]")) {
                    return;
                }
                const QJsonObject root = QJsonDocument::fromJson(trimmed).object();
                if (root.contains(QStringLiteral("error"))) {
                    streamError = root.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
                    return;
                }
                const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
                if (choices.isEmpty()) {
                    return;
                }
                const QJsonObject deltaObject = choices.first().toObject().value(QStringLiteral("delta")).toObject();
                const QString delta = deltaObject.value(QStringLiteral("content")).toString();
                if (!delta.isEmpty()) {
                    result += delta;
                    if (onDelta) {
                        onDelta(delta);
                    }
                }
            },
            error,
            90000
        );
        if (!streamError.isEmpty()) {
            if (error) {
                *error = providerName + tr8(" 调用失败：") + streamError;
            }
            return QString();
        }
        if (result.trimmed().isEmpty() && error && error->trimmed().isEmpty()) {
            *error = providerName + tr8(" 没有返回结果。");
        }
        return result.trimmed();
    }

    QString baiduAccessToken(QString *error)
    {
        if (!m_baiduToken.isEmpty() && m_tokenExpire > QDateTime::currentDateTime().addSecs(60)) {
            return m_baiduToken;
        }

        QUrl url(QStringLiteral("https://aip.baidubce.com/oauth/2.0/token"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("client_credentials"));
        query.addQueryItem(QStringLiteral("client_id"), m_secrets.baiduApiKey);
        query.addQueryItem(QStringLiteral("client_secret"), m_secrets.baiduSecretKey);
        url.setQuery(query);

        QNetworkRequest request(url);
        const QByteArray response = get(request, error, 30000);
        if (response.isEmpty()) {
            return QString();
        }

        const QJsonObject root = QJsonDocument::fromJson(response).object();
        const QString token = root.value(QStringLiteral("access_token")).toString();
        if (token.isEmpty()) {
            if (error) {
                *error = tr8("百度令牌获取失败。");
            }
            return QString();
        }
        m_baiduToken = token;
        const int expires = root.value(QStringLiteral("expires_in")).toInt(2592000);
        m_tokenExpire = QDateTime::currentDateTime().addSecs(expires);
        return m_baiduToken;
    }

    QByteArray get(const QNetworkRequest &request, QString *error, int timeoutMs)
    {
        QNetworkReply *reply = m_network.get(request);
        return waitReply(reply, error, timeoutMs);
    }

    QByteArray postJson(const QNetworkRequest &request, const QByteArray &body, QString *error, int timeoutMs)
    {
        QNetworkReply *reply = m_network.post(request, body);
        return waitReply(reply, error, timeoutMs);
    }

    void postJsonEventStream(
        const QNetworkRequest &request,
        const QByteArray &body,
        const std::function<void(const QByteArray &)> &onEventData,
        QString *error,
        int timeoutMs
    )
    {
        QNetworkReply *reply = m_network.post(request, body);
        QEventLoop loop;
        QTimer timer;
        QByteArray buffer;
        timer.setSingleShot(true);

        auto processBuffer = [&]() {
            int newline = -1;
            while ((newline = buffer.indexOf('\n')) >= 0) {
                QByteArray line = buffer.left(newline).trimmed();
                buffer.remove(0, newline + 1);
                if (line.startsWith("data:")) {
                    const QByteArray payload = line.mid(5).trimmed();
                    if (!payload.isEmpty() && onEventData) {
                        onEventData(payload);
                    }
                }
            }
        };

        QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&]() {
            buffer += reply->readAll();
            processBuffer();
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();

        if (timer.isActive()) {
            timer.stop();
        } else {
            reply->abort();
            reply->deleteLater();
            if (error) {
                *error = tr8("网络请求超时。");
            }
            return;
        }

        buffer += reply->readAll();
        if (!buffer.isEmpty()) {
            buffer.append('\n');
            processBuffer();
        }

        const QNetworkReply::NetworkError networkError = reply->error();
        const QString errorString = reply->errorString();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError && error) {
            if (errorString.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)
                || errorString.contains(QStringLiteral("ssl"), Qt::CaseInsensitive)) {
                *error = tr8("网络请求失败：SSL 运行库缺失或版本不匹配。请确认程序目录中存在 libeay32.dll 和 ssleay32.dll。原始错误：") + errorString;
            } else if (statusCode == 401 || errorString.contains(QStringLiteral("authentication"), Qt::CaseInsensitive)) {
                *error = tr8("网络请求失败：接口认证失败。请检查百度、讯飞、DeepSeek、OpenAI 或 Claude 密钥是否正确。原始错误：") + errorString;
            } else {
                *error = tr8("网络请求失败：") + errorString;
            }
        }
    }

    QByteArray waitReply(QNetworkReply *reply, QString *error, int timeoutMs)
    {
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();

        if (timer.isActive()) {
            timer.stop();
        } else {
            reply->abort();
            reply->deleteLater();
            if (error) {
                *error = tr8("网络请求超时。");
            }
            return QByteArray();
        }

        const QByteArray data = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString errorString = reply->errorString();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError) {
            if (error) {
                if (errorString.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)
                    || errorString.contains(QStringLiteral("ssl"), Qt::CaseInsensitive)) {
                    *error = tr8("网络请求失败：SSL 运行库缺失或版本不匹配。请确认程序目录中存在 libeay32.dll 和 ssleay32.dll。原始错误：") + errorString;
                } else if (statusCode == 401 || errorString.contains(QStringLiteral("authentication"), Qt::CaseInsensitive)) {
                    *error = tr8("网络请求失败：接口认证失败。请检查百度、讯飞、DeepSeek、OpenAI 或 Claude 密钥是否正确。原始错误：") + errorString;
                } else {
                    *error = tr8("网络请求失败：") + errorString;
                }
            }
            return QByteArray();
        }
        return data;
    }

    SecretConfig m_secrets = loadSecrets();
    QNetworkAccessManager m_network;
    QString m_baiduToken;
    QDateTime m_tokenExpire;
    bool m_useSystemProxy = false;
};

// 全局快捷键：使用 Windows RegisterHotKey 接收后台快捷键，不依赖主窗口焦点。
class GlobalHotkeys : public QAbstractNativeEventFilter
{
public:
    ~GlobalHotkeys()
    {
        unregisterAll();
    }

    void setCallback(const std::function<void(const QString &)> &callback)
    {
        m_callback = callback;
    }

    void registerFromSettings(const AppSettings &settings)
    {
        unregisterAll();
        int nativeId = 100;
        for (const HotkeyDef &def : hotkeyDefs()) {
            registerHotkey(nativeId++, def.id, settings.hotkey(def.id));
        }
        for (const CustomFunctionDef &fn : settings.customFunctions()) {
            registerHotkey(nativeId++, fn.id, fn.shortcut);
        }
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override
    {
        Q_UNUSED(eventType);
        Q_UNUSED(result);
#ifdef Q_OS_WIN
        MSG *msg = static_cast<MSG *>(message);
        if (msg && msg->message == WM_HOTKEY) {
            const int nativeId = static_cast<int>(msg->wParam);
            if (m_ids.contains(nativeId) && m_callback) {
                m_callback(m_ids.value(nativeId));
            }
            return true;
        }
#else
        Q_UNUSED(message);
#endif
        return false;
    }

private:
    bool registerHotkey(int nativeId, const QString &logicalId, const QString &shortcut)
    {
#ifdef Q_OS_WIN
        uint modifiers = 0;
        uint key = 0;
        if (!parseShortcut(shortcut, &modifiers, &key)) {
            return false;
        }
        if (RegisterHotKey(nullptr, nativeId, modifiers, key)) {
            m_ids.insert(nativeId, logicalId);
            return true;
        }
#else
        Q_UNUSED(nativeId);
        Q_UNUSED(logicalId);
        Q_UNUSED(shortcut);
#endif
        return false;
    }

    void unregisterAll()
    {
#ifdef Q_OS_WIN
        for (int nativeId : m_ids.keys()) {
            UnregisterHotKey(nullptr, nativeId);
        }
#endif
        m_ids.clear();
    }

    bool parseShortcut(const QString &shortcut, uint *modifiers, uint *nativeKey) const
    {
        const QKeySequence sequence(shortcut);
        if (sequence.isEmpty()) {
            return false;
        }
        const int value = sequence[0];
        uint mods = 0;
        if (value & Qt::CTRL) mods |= MOD_CONTROL;
        if (value & Qt::ALT) mods |= MOD_ALT;
        if (value & Qt::SHIFT) mods |= MOD_SHIFT;
        if (value & Qt::META) mods |= MOD_WIN;

        const int key = value & ~(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::META);
        uint vk = 0;
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            vk = static_cast<uint>('A' + key - Qt::Key_A);
        } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            vk = static_cast<uint>('0' + key - Qt::Key_0);
        } else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
            vk = static_cast<uint>(VK_F1 + key - Qt::Key_F1);
        } else if (key == Qt::Key_Space) {
            vk = VK_SPACE;
        } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            vk = VK_RETURN;
        } else if (key == Qt::Key_Escape) {
            vk = VK_ESCAPE;
        } else if (key == Qt::Key_Tab) {
            vk = VK_TAB;
        }

        if (vk == 0 || mods == 0) {
            return false;
        }
        *modifiers = mods;
        *nativeKey = vk;
        return true;
    }

    QMap<int, QString> m_ids;
    std::function<void(const QString &)> m_callback;
};

// 录音波形控件：显示麦克风是否有声音输入，只在录音相关状态下展示。
class WaveformMeter : public QWidget
{
public:
    explicit WaveformMeter(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(126, 34);
        m_levels.fill(0, 16);
    }

    void reset()
    {
        m_levels.fill(0, 16);
        m_displayLevel = 0;
        update();
    }

    void setPeak(int peak)
    {
        const int noiseFloor = 70;
        const int fullScalePeak = 2200;
        const int cleanPeak = qMax(0, peak - noiseFloor);
        int normalized = 0;
        if (cleanPeak > 0) {
            const double ratio = qMin(1.0, cleanPeak / static_cast<double>(fullScalePeak));
            normalized = qBound(0, static_cast<int>(std::sqrt(ratio) * 110.0), 100);
        }

        if (normalized > m_displayLevel) {
            m_displayLevel = (m_displayLevel + normalized * 3) / 4;
        } else {
            m_displayLevel = (m_displayLevel * 3 + normalized) / 4;
        }
        if (!m_levels.isEmpty()) {
            m_levels.removeFirst();
        }
        m_levels.append(m_displayLevel);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF outer = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(QColor(QStringLiteral("#344155")), 1));
        painter.setBrush(QColor(QStringLiteral("#172033")));
        painter.drawRoundedRect(outer, 9, 9);

        const bool active = m_displayLevel > 8;
        painter.setPen(Qt::NoPen);
        painter.setBrush(active ? QColor(QStringLiteral("#22c55e")) : QColor(QStringLiteral("#64748b")));
        painter.drawEllipse(QRectF(10, height() / 2 - 3, 6, 6));

        const int count = m_levels.size();
        const qreal startX = 24;
        const qreal gap = 3;
        const qreal barWidth = 3.8;
        const qreal maxHeight = height() - 12;
        for (int i = 0; i < count; ++i) {
            const int level = m_levels.at(i);
            const qreal heightRatio = level / 100.0;
            const qreal barHeight = qMax<qreal>(5, 5 + heightRatio * (maxHeight - 5));
            const qreal x = startX + i * (barWidth + gap);
            const qreal y = (this->height() - barHeight) / 2.0;

            QColor color = active ? QColor(QStringLiteral("#34d399")) : QColor(QStringLiteral("#516075"));
            if (active && i > count - 5) {
                color = QColor(QStringLiteral("#a7f3d0"));
            }
            color.setAlpha(active ? qBound(130, 110 + level, 245) : 150);
            painter.setBrush(color);
            painter.drawRoundedRect(QRectF(x, y, barWidth, barHeight), 2, 2);
        }
    }

private:
    QVector<int> m_levels;
    int m_displayLevel = 0;
};

// 浮动条状态点：用颜色和轻微动画提示录音、处理、完成和错误状态。
class FloatingStatusIndicator : public QWidget
{
public:
    explicit FloatingStatusIndicator(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(40, 40);
        m_timer.setInterval(45);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_phase = (m_phase + 1) % 1000;
            update();
        });
    }

    void setRecording(bool recording)
    {
        if (m_recording == recording) {
            return;
        }
        m_recording = recording;
        update();
    }

    void startPulse()
    {
        if (!m_timer.isActive()) {
            m_timer.start();
        }
    }

    void stopPulse()
    {
        m_timer.stop();
        m_phase = 0;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal wave = (std::sin(m_phase / 8.0) + 1.0) / 2.0;
        const QColor base = m_recording
            ? QColor(QStringLiteral("#fb7185"))
            : QColor(QStringLiteral("#60a5fa"));
        const QColor core = m_recording
            ? QColor(QStringLiteral("#f43f5e"))
            : QColor(QStringLiteral("#3b82f6"));

        painter.setPen(Qt::NoPen);
        QColor halo = base;
        halo.setAlpha(m_recording ? 32 + static_cast<int>(wave * 58) : 22 + static_cast<int>(wave * 38));
        const qreal haloRadius = m_recording ? 15.0 + wave * 5.0 : 13.0 + wave * 3.0;
        painter.setBrush(halo);
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0), haloRadius, haloRadius);

        painter.setBrush(QColor(QStringLiteral("#1f2937")));
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0), 15.5, 15.5);

        QColor ring = base;
        ring.setAlpha(m_recording ? 185 : 150);
        painter.setBrush(ring);
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0), 11.5, 11.5);

        QColor inner = core;
        inner.setAlpha(235);
        const qreal innerRadius = m_recording ? 5.2 + wave * 1.8 : 4.6 + wave * 1.1;
        painter.setBrush(inner);
        painter.drawEllipse(QPointF(width() / 2.0, height() / 2.0), innerRadius, innerRadius);

        if (m_recording) {
            QColor shine(QStringLiteral("#ffe4e6"));
            shine.setAlpha(190);
            painter.setBrush(shine);
            painter.drawEllipse(QPointF(width() / 2.0 - 3.0, height() / 2.0 - 3.0), 1.7, 1.7);
        }
    }

private:
    QTimer m_timer;
    int m_phase = 0;
    bool m_recording = false;
};

// 浮动条：后台模式下的临时状态界面，显示录音、识别、处理、错误和快捷操作。
class FloatingBar : public QWidget
{
public:
    explicit FloatingBar(AppSettings *settings = nullptr, QWidget *parent = nullptr)
        : QWidget(parent), m_settings(settings)
    {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setFixedSize(720, 76);

        auto *root = new QFrame(this);
        root->setObjectName(QStringLiteral("root"));
        root->setGeometry(rect());
        root->setCursor(Qt::OpenHandCursor);
        root->installEventFilter(this);
        root->setStyleSheet(QStringLiteral(
            "QFrame#root {"
            "  background: #111827;"
            "  border: 1px solid #2f3a4a;"
            "  border-radius: 8px;"
            "}"
            "QLabel { color: #f9fafb; }"
        ));

        auto *layout = new QHBoxLayout(root);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(12);

        m_indicator = new FloatingStatusIndicator;

        auto *textBox = new QWidget;
        textBox->setCursor(Qt::OpenHandCursor);
        textBox->installEventFilter(this);
        auto *textLayout = new QVBoxLayout(textBox);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        m_title = new QLabel(tr8("等待快捷键"));
        m_title->setFont(appFont(11, QFont::DemiBold));
        m_title->setCursor(Qt::OpenHandCursor);
        m_title->installEventFilter(this);

        m_subtitle = new QLabel(tr8("按快捷键开始使用"));
        m_subtitle->setFont(appFont(9));
        m_subtitle->setStyleSheet(QStringLiteral("color: #aeb7c5;"));
        m_subtitle->setCursor(Qt::OpenHandCursor);
        m_subtitle->installEventFilter(this);

        textLayout->addWidget(m_title);
        textLayout->addWidget(m_subtitle);

        auto *copy = smallActionButton(tr8("复制"));
        auto *undo = smallActionButton(tr8("撤销"));
        auto *retry = smallActionButton(tr8("重试"));
        connect(copy, &QPushButton::clicked, this, [this]() {
            if (!m_lastResult.isEmpty()) {
                QApplication::clipboard()->setText(m_lastResult);
                setStatus(tr8("已复制"), tr8("结果已复制到剪贴板"));
                hideLater();
            }
        });

        m_waveform = new WaveformMeter;
        m_waveform->setVisible(false);
        m_waveform->installEventFilter(this);
        m_indicator->installEventFilter(this);

        layout->addWidget(m_indicator);
        layout->addWidget(textBox, 1);
        layout->addWidget(m_waveform);
        layout->addWidget(copy);
        layout->addWidget(undo);
        layout->addWidget(retry);
    }

    void setStatus(const QString &title, const QString &subtitle)
    {
        if (!m_enabled || m_suppressed) {
            hide();
            return;
        }
        ++m_statusGeneration;
        m_title->setText(title);
        m_subtitle->setText(subtitle);
        placeNearBottom();
        show();
        raise();
        if (m_indicator) {
            m_indicator->startPulse();
        }
    }

    void setResult(const QString &title, const QString &result)
    {
        m_lastResult = result;
        QString preview = result;
        preview.replace(QStringLiteral("\n"), QStringLiteral(" "));
        if (preview.size() > 42) {
            preview = preview.left(42) + QStringLiteral("...");
        }
        setStatus(title, preview);
    }

    QString lastResult() const
    {
        return m_lastResult;
    }

    void hideLater(int msec = -1)
    {
        const int delay = msec >= 0 ? msec : m_autoHideMsec;
        const int generation = m_statusGeneration;
        QTimer::singleShot(delay, this, [this, generation]() {
            if (generation == m_statusGeneration) {
                hide();
            }
        });
    }

    void setAutoHideMsec(int msec)
    {
        m_autoHideMsec = qBound(1000, msec, 60000);
    }

    void setEnabledVisible(bool enabled)
    {
        m_enabled = enabled;
        if (!m_enabled) {
            ++m_statusGeneration;
            if (m_indicator) {
                m_indicator->stopPulse();
            }
            hide();
        }
    }

    void setSuppressed(bool suppressed)
    {
        m_suppressed = suppressed;
        if (m_suppressed) {
            ++m_statusGeneration;
            if (m_indicator) {
                m_indicator->stopPulse();
            }
            hide();
        }
    }

    void setWaveformVisible(bool visible)
    {
        if (m_indicator) {
            m_indicator->setRecording(visible);
            if (visible) {
                m_indicator->startPulse();
            }
        }
        if (m_waveform) {
            m_waveform->setVisible(visible);
            if (!visible) {
                m_waveform->reset();
            }
        }
    }

    void setWaveformLevel(int peak)
    {
        if (!m_waveform || !m_waveform->isVisible()) {
            return;
        }
        m_waveform->setPeak(peak);
    }

    void placeNearBottom()
    {
        if (m_settings && m_settings->hasFloatingBarPosition()) {
            move(clampedTopLeftToScreen(m_settings->floatingBarPosition(), size()));
            return;
        }
        const QRect screen = QApplication::desktop()->availableGeometry();
        move(screen.center().x() - width() / 2, screen.bottom() - height() - 28);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragStartGlobal = mouse->globalPos();
                m_dragStartPosition = pos();
                setCursor(Qt::ClosedHandCursor);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            move(clampedTopLeftToScreen(m_dragStartPosition + (mouse->globalPos() - m_dragStartGlobal), size()));
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragging = false;
                setCursor(Qt::ArrowCursor);
                saveCurrentPosition();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void hideEvent(QHideEvent *event) override
    {
        if (m_indicator) {
            m_indicator->stopPulse();
            m_indicator->setRecording(false);
        }
        QWidget::hideEvent(event);
    }

private:
    QPushButton *smallActionButton(const QString &text)
    {
        auto *button = new QPushButton(text);
        button->setFixedHeight(32);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: #263244;"
            "  color: #f9fafb;"
            "  border: 1px solid #3b4658;"
            "  border-radius: 6px;"
            "  padding: 0 10px;"
            "}"
            "QPushButton:hover { background: #344155; }"
        ));
        return button;
    }

    void saveCurrentPosition()
    {
        if (!m_settings) {
            return;
        }
        m_settings->setFloatingBarPosition(pos());
        m_settings->save();
    }

    AppSettings *m_settings = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    FloatingStatusIndicator *m_indicator = nullptr;
    WaveformMeter *m_waveform = nullptr;
    QString m_lastResult;
    bool m_enabled = true;
    bool m_suppressed = false;
    bool m_dragging = false;
    QPoint m_dragStartGlobal;
    QPoint m_dragStartPosition;
    int m_autoHideMsec = 2000;
    int m_statusGeneration = 0;
};

// 帮助弹窗：复用在设置问号和其它说明入口，展示当前页面的简短帮助。
class HelpDialog : public QDialog
{
public:
    HelpDialog(const QString &helpTitle, const QString &helpText, QWidget *parent = nullptr)
        : QDialog(parent), m_helpTitle(helpTitle), m_helpText(helpText)
    {
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::EnterWhatsThisMode) {
            QWhatsThis::leaveWhatsThisMode();
            showAttentionInformation(this, m_helpTitle, m_helpText);
            return true;
        }
        return QDialog::event(event);
    }

private:
    QString m_helpTitle;
    QString m_helpText;
};

// 结果小框：大模型返回后给用户选择复制、写入、替换选中、重新生成和继续追问。
class ResultChoicePopup : public QWidget
{
public:
    ResultChoicePopup(AppSettings *settings, const QString &title, const QString &result, NativeWindowHandle targetWindow, bool hasSelection, int autoCloseMsec, QWidget *parent = nullptr)
        : QWidget(parent),
          m_settings(settings),
          m_result(result),
          m_initialResult(result),
          m_targetWindow(targetWindow),
          m_hasSelection(hasSelection),
          m_autoCloseMsec(qMax(0, autoCloseMsec))
    {
        setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_DeleteOnClose);
        setMinimumSize(640, 460);
        resize(760, 520);
        setFont(appFont());
        setStyleSheet(QStringLiteral(
            "QWidget#resultPopup { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 8px; }"
            "QLabel { color: #111827; background: transparent; }"
        ));
        setObjectName(QStringLiteral("resultPopup"));

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);

        auto *top = new QHBoxLayout;
        auto *heading = new QLabel(title);
        heading->setFont(appFont(13, QFont::DemiBold));
        m_hint = new QLabel(tr8("请选择下一步操作"));
        m_hint->setStyleSheet(QStringLiteral("color: #667085;"));
        top->addWidget(heading);
        top->addStretch();
        top->addWidget(m_hint);
        layout->addLayout(top);

        m_editor = new QTextEdit;
        m_editor->setReadOnly(false);
        m_editor->setAcceptRichText(false);
        m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
        m_editor->setPlainText(result);
        m_editor->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "  padding: 10px;"
            "}"
        ));
        layout->addWidget(m_editor, 1);
        connect(m_editor, &QTextEdit::textChanged, this, [this]() {
            if (m_editor) {
                m_result = m_editor->toPlainText();
            }
            if (!m_programmaticTextChange && !m_busy) {
                m_userEdited = true;
            }
            updateActionState();
        });

        auto *advanced = new QHBoxLayout;
        advanced->addStretch();
        m_regenerateButton = popupButton(tr8("重新生成"), false);
        m_retryModelButton = popupButton(tr8("换模型"), false);
        m_followUpButton = popupButton(tr8("继续追问"), false);
        auto *expand = popupButton(tr8("展开全文"), false);
        advanced->addWidget(m_regenerateButton);
        advanced->addWidget(m_retryModelButton);
        advanced->addWidget(m_followUpButton);
        advanced->addWidget(expand);
        layout->addLayout(advanced);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        m_copyButton = popupButton(tr8("复制"), false);
        m_writeButton = popupButton(tr8("写入"), true);
        m_replaceButton = popupButton(tr8("替换选中"), false);
        m_replaceButton->setEnabled(hasSelection);
        m_replaceButton->setToolTip(hasSelection ? tr8("用结果替换原来选中的文本") : tr8("当前没有可替换的选中文本"));
        auto *close = popupButton(tr8("关闭"), false);

        buttons->addWidget(m_copyButton);
        buttons->addWidget(m_writeButton);
        buttons->addWidget(m_replaceButton);
        buttons->addWidget(close);
        layout->addLayout(buttons);

        connect(m_copyButton, &QPushButton::clicked, this, [this]() {
            syncResultFromEditor();
            QApplication::clipboard()->setText(m_result);
            m_hint->setText(tr8("已复制"));
        });
        connect(m_writeButton, &QPushButton::clicked, this, [this]() {
            syncResultFromEditor();
            const QString result = m_result;
            const NativeWindowHandle targetWindow = m_targetWindow;
            const bool hasSelection = m_hasSelection;
            this->close();
            ClipboardBridge::pasteTextToWindow(result, targetWindow, false, hasSelection);
        });
        connect(m_replaceButton, &QPushButton::clicked, this, [this]() {
            syncResultFromEditor();
            const QString result = m_result;
            const NativeWindowHandle targetWindow = m_targetWindow;
            this->close();
            ClipboardBridge::pasteTextToWindow(result, targetWindow, true, true);
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
        connect(expand, &QPushButton::clicked, this, [this]() {
            showExpandedResult();
        });
        connect(close, &QPushButton::clicked, this, &QWidget::close);
        updateActionState();
    }

    void setActionCallbacks(
        const std::function<void()> &onRegenerate,
        const std::function<void(const QString &)> &onRetryModel,
        const std::function<void(const QString &)> &onFollowUp
    )
    {
        m_onRegenerate = onRegenerate;
        m_onRetryModel = onRetryModel;
        m_onFollowUp = onFollowUp;
        updateActionState();
    }

    void setDraftCallback(const std::function<void(const QString &)> &onDraft)
    {
        m_onDraft = onDraft;
    }

    void setCurrentModel(const QString &model)
    {
        m_currentModel = model;
    }

    QString currentModel() const
    {
        return m_currentModel;
    }

    void setResultText(const QString &result, bool resetDraftState = true)
    {
        m_result = result;
        if (m_editor) {
            m_programmaticTextChange = true;
            m_editor->setPlainText(result);
            m_editor->moveCursor(QTextCursor::End);
            m_programmaticTextChange = false;
        }
        if (resetDraftState) {
            m_initialResult = result;
            m_userEdited = false;
            m_draftSaved = false;
        }
        updateActionState();
    }

    void appendResultText(const QString &text)
    {
        if (text.isEmpty()) {
            return;
        }
        m_result += text;
        if (m_editor) {
            m_programmaticTextChange = true;
            m_editor->moveCursor(QTextCursor::End);
            m_editor->insertPlainText(text);
            m_editor->moveCursor(QTextCursor::End);
            m_programmaticTextChange = false;
        }
        m_initialResult = m_result;
        updateActionState();
    }

    QString resultText() const
    {
        if (m_editor) {
            return m_editor->toPlainText();
        }
        return m_result;
    }

    void setBusy(bool busy, const QString &hint = QString())
    {
        m_busy = busy;
        if (m_editor) {
            m_editor->setReadOnly(busy);
        }
        if (m_hint) {
            m_hint->setText(hint.trimmed().isEmpty()
                ? (busy ? tr8("正在生成") : tr8("请选择下一步操作"))
                : hint);
        }
        updateActionState();
    }

    void showNearBottom()
    {
        const QRect screen = QApplication::desktop()->availableGeometry();
        if (m_settings && m_settings->hasResultPopupGeometry()) {
            const QRect saved = m_settings->resultPopupGeometry();
            resize(saved.size());
            move(clampedTopLeftToScreen(saved.topLeft(), size()));
        } else {
            const int targetWidth = qMin(780, qMax(640, screen.width() - 80));
            const int targetHeight = qMin(540, qMax(460, screen.height() - 140));
            resize(targetWidth, targetHeight);
            move(screen.center().x() - width() / 2, screen.bottom() - height() - 96);
        }
        show();
        raise();
        activateWindow();
        if (m_autoCloseMsec > 0) {
            m_hint->setText(tr8("将在 %1 秒后自动关闭").arg((m_autoCloseMsec + 999) / 1000));
            QTimer::singleShot(m_autoCloseMsec, this, &QWidget::close);
        }
    }

private:
    QPushButton *popupButton(const QString &text, bool primary)
    {
        auto *button = new QPushButton(text);
        button->setMinimumHeight(40);
        button->setMinimumWidth(64);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(primary
            ? buttonStyle(QStringLiteral("#111827"))
            : buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        return button;
    }

    void updateActionState()
    {
        syncResultFromEditor();
        const bool hasResult = !m_result.trimmed().isEmpty();
        if (m_copyButton) m_copyButton->setEnabled(!m_busy && hasResult);
        if (m_writeButton) m_writeButton->setEnabled(!m_busy && hasResult);
        if (m_replaceButton) m_replaceButton->setEnabled(!m_busy && hasResult && m_hasSelection);
        if (m_regenerateButton) m_regenerateButton->setEnabled(!m_busy && static_cast<bool>(m_onRegenerate));
        if (m_retryModelButton) m_retryModelButton->setEnabled(!m_busy && static_cast<bool>(m_onRetryModel));
        if (m_followUpButton) m_followUpButton->setEnabled(!m_busy && static_cast<bool>(m_onFollowUp));
    }

    void chooseModelAndRetry()
    {
        if (!m_onRetryModel) {
            return;
        }

        HelpDialog dialog(
            tr8("换模型帮助"),
            tr8("这里用于用另一个模型重新生成当前结果。选择模型后点击“重试”，软件会保留上一次输入内容，只更换模型重新调用。"),
            this
        );
        dialog.setWindowTitle(tr8("换模型重试"));
        dialog.setMinimumWidth(420);
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 16, 18, 16);
        layout->setSpacing(12);

        auto *label = new QLabel(tr8("选择一个模型重新生成当前结果。"));
        label->setWordWrap(true);
        layout->addWidget(label);

        auto *combo = new QComboBox;
        for (const ModelOption &option : modelOptions()) {
            combo->addItem(option.title, option.id);
        }
        const int currentIndex = combo->findData(m_currentModel);
        combo->setCurrentIndex(currentIndex >= 0 ? currentIndex : 0);
        layout->addWidget(combo);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *cancel = popupButton(tr8("取消"), false);
        auto *ok = popupButton(tr8("重试"), true);
        buttons->addWidget(cancel);
        buttons->addWidget(ok);
        layout->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);

        if (dialog.exec() == QDialog::Accepted) {
            m_onRetryModel(combo->currentData().toString());
        }
    }

    void askFollowUp()
    {
        if (!m_onFollowUp) {
            return;
        }

        HelpDialog dialog(
            tr8("继续追问帮助"),
            tr8("这里用于在当前结果基础上继续补充要求。输入新的问题、修改要求或追问内容后点击“发送”，软件会结合上一次输入和当前结果继续处理。"),
            this
        );
        dialog.setWindowTitle(tr8("继续追问"));
        dialog.setMinimumSize(520, 320);
        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 16, 18, 16);
        layout->setSpacing(12);

        auto *label = new QLabel(tr8("输入新的追问或补充要求，会基于上一次输入和当前结果继续处理。"));
        label->setWordWrap(true);
        layout->addWidget(label);

        auto *editor = new QTextEdit;
        editor->setPlaceholderText(tr8("例如：再简短一点，或者继续解释第二点。"));
        layout->addWidget(editor, 1);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *cancel = popupButton(tr8("取消"), false);
        auto *ok = popupButton(tr8("发送"), true);
        buttons->addWidget(cancel);
        buttons->addWidget(ok);
        layout->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);

        if (dialog.exec() == QDialog::Accepted) {
            const QString text = editor->toPlainText().trimmed();
            if (!text.isEmpty()) {
                m_onFollowUp(text);
            }
        }
    }

    void showExpandedResult()
    {
        syncResultFromEditor();
        const QString previous = m_result;
        HelpDialog dialog(
            tr8("完整结果帮助"),
            tr8("这里用于查看和编辑完整输出。你可以直接修改文本，关闭窗口后会同步回结果小框；点击“复制”会复制当前编辑后的完整内容。"),
            this
        );
        dialog.setWindowTitle(tr8("完整结果"));
        dialog.setMinimumSize(760, 560);
        auto *layout = new QVBoxLayout(&dialog);
        auto *editor = new QTextEdit;
        editor->setReadOnly(false);
        editor->setAcceptRichText(false);
        editor->setLineWrapMode(QTextEdit::WidgetWidth);
        editor->setPlainText(m_result);
        layout->addWidget(editor, 1);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *copy = popupButton(tr8("复制"), false);
        auto *close = popupButton(tr8("关闭"), true);
        buttons->addWidget(copy);
        buttons->addWidget(close);
        layout->addLayout(buttons);

        connect(copy, &QPushButton::clicked, &dialog, [editor]() {
            QApplication::clipboard()->setText(editor->toPlainText());
        });
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        dialog.exec();
        const QString updated = editor->toPlainText();
        setResultText(updated, false);
        if (updated != previous) {
            m_userEdited = true;
        }
    }

    void syncResultFromEditor()
    {
        if (m_editor) {
            m_result = m_editor->toPlainText();
        }
    }

    bool shouldSaveDraft() const
    {
        return m_userEdited
            && !m_draftSaved
            && !m_busy
            && !m_result.trimmed().isEmpty()
            && m_result != m_initialResult
            && static_cast<bool>(m_onDraft);
    }

    void saveDraftIfNeeded()
    {
        syncResultFromEditor();
        if (!shouldSaveDraft()) {
            return;
        }
        m_draftSaved = true;
        m_onDraft(m_result);
    }

    void saveGeometryPreference()
    {
        if (!m_settings) {
            return;
        }
        m_settings->setResultPopupGeometry(QRect(pos(), size()));
        m_settings->save();
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        saveDraftIfNeeded();
        saveGeometryPreference();
        QWidget::closeEvent(event);
    }

private:
    AppSettings *m_settings = nullptr;
    QString m_result;
    QString m_initialResult;
    NativeWindowHandle m_targetWindow = nullptr;
    bool m_hasSelection = false;
    int m_autoCloseMsec = 0;
    bool m_busy = false;
    bool m_programmaticTextChange = false;
    bool m_userEdited = false;
    bool m_draftSaved = false;
    QString m_currentModel;
    QTextEdit *m_editor = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_writeButton = nullptr;
    QPushButton *m_replaceButton = nullptr;
    QPushButton *m_regenerateButton = nullptr;
    QPushButton *m_retryModelButton = nullptr;
    QPushButton *m_followUpButton = nullptr;
    std::function<void()> m_onRegenerate;
    std::function<void(const QString &)> m_onRetryModel;
    std::function<void(const QString &)> m_onFollowUp;
    std::function<void(const QString &)> m_onDraft;
};

// 设置页标签滚轮过滤器：让设置区域较小时，鼠标滚轮仍能顺畅滚动当前页面。
class TabBarWheelFilter : public QObject
{
public:
    explicit TabBarWheelFilter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *tabBar = qobject_cast<QTabBar *>(watched);
        if (!tabBar || event->type() != QEvent::Wheel) {
            return QObject::eventFilter(watched, event);
        }

        auto *wheel = static_cast<QWheelEvent *>(event);
        const QPoint delta = wheel->angleDelta();
        const int amount = qAbs(delta.x()) > qAbs(delta.y()) ? delta.x() : delta.y();
        if (amount == 0) {
            return QObject::eventFilter(watched, event);
        }

        const int direction = amount < 0 ? 1 : -1;
        const int nextIndex = qBound(0, tabBar->currentIndex() + direction, tabBar->count() - 1);
        if (nextIndex == tabBar->currentIndex()) {
            return QObject::eventFilter(watched, event);
        }

        tabBar->setCurrentIndex(nextIndex);
        event->accept();
        return true;
    }
};

// 设置面板：嵌入主界面，保留后台行为、快捷键和接口配置；功能级配置统一放到“功能自定义”页。
class SettingsPanel : public QWidget
{
public:
    explicit SettingsPanel(AppSettings *settings, const std::function<void()> &onChanged, QWidget *parent = nullptr, int initialTab = 0)
        : QWidget(parent), m_settings(settings), m_onChanged(onChanged)
    {
        setObjectName(QStringLiteral("settingsPanel"));
        setFont(appFont());
        setStyleSheet(QStringLiteral("QWidget#settingsPanel { background: #f6f7f9; } QLabel { color: #111827; }"));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(28, 24, 28, 24);
        root->setSpacing(16);

        auto *top = new QHBoxLayout;
        auto *heading = new QLabel(tr8("设置"));
        heading->setFont(appFont(24, QFont::DemiBold));
        auto *help = new QPushButton(tr8("帮助"));
        help->setFixedHeight(36);
        help->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(help, &QPushButton::clicked, this, [this]() {
            showSettingsHelp();
        });
        top->addWidget(heading, 1);
        top->addWidget(help);
        root->addLayout(top);

        m_tabs = new QTabWidget;
        m_tabs->setStyleSheet(QStringLiteral(
            "QTabWidget::pane { border: 1px solid #dde2ea; background: #ffffff; border-radius: 8px; }"
            "QTabBar::tab { padding: 9px 16px; color: #4b5563; }"
            "QTabBar::tab:selected { color: #111827; font-weight: 600; }"
        ));
        m_tabs->addTab(generalTab(), tr8("常用设置"));
        m_tabs->addTab(shortcutsTab(), tr8("快捷键"));
        m_tabs->addTab(apiTab(), tr8("接口"));
        setCurrentTab(initialTab);
        root->addWidget(m_tabs, 1);
    }

    void setCurrentTab(int index)
    {
        if (m_tabs && index >= 0 && index < m_tabs->count()) {
            m_tabs->setCurrentIndex(index);
        }
    }

    bool savePendingSecrets(bool showConfirmation = false)
    {
        return saveSecretsFromUi(showConfirmation);
    }

    void refreshFromSettings()
    {
        for (const HotkeyDef &def : hotkeyDefs()) {
            if (m_hotkeyLabels.contains(def.id)) {
                m_hotkeyLabels.value(def.id)->setText(displayShortcut(m_settings->hotkey(def.id)));
            }
        }
        if (m_autoStartBox && m_autoStartBox->isChecked() != m_settings->autoStartEnabled()) {
            m_autoStartBox->blockSignals(true);
            m_autoStartBox->setChecked(m_settings->autoStartEnabled());
            m_autoStartBox->blockSignals(false);
        }
        if (m_strongSelectionBox && m_strongSelectionBox->isChecked() != m_settings->strongSelectionEnabled()) {
            m_strongSelectionBox->blockSignals(true);
            m_strongSelectionBox->setChecked(m_settings->strongSelectionEnabled());
            m_strongSelectionBox->blockSignals(false);
        }
        refreshRecordDirectoryLabel();
        setComboCurrentData(m_speechProviderBox, m_settings->speechProvider());
        updateSpeechSecretRows();
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::EnterWhatsThisMode) {
            QWhatsThis::leaveWhatsThisMode();
            showSettingsHelp();
            return true;
        }
        return QWidget::event(event);
    }

private:
    AppSettings *m_settings;
    std::function<void()> m_onChanged;
    QTabWidget *m_tabs = nullptr;
    QMap<QString, QLabel *> m_hotkeyLabels;
    QMap<QString, QComboBox *> m_modelBoxes;
    QMap<QString, QComboBox *> m_outputModeBoxes;
    QVBoxLayout *m_customListLayout = nullptr;
    QMap<QString, QLineEdit *> m_customNameEdits;
    QMap<QString, QKeySequenceEdit *> m_customShortcutEdits;
    QMap<QString, QComboBox *> m_customModelBoxes;
    QMap<QString, QComboBox *> m_customOutputModeBoxes;
    QMap<QString, QCheckBox *> m_customSelectionBoxes;
    QMap<QString, QCheckBox *> m_customVoiceBoxes;
    QMap<QString, QSpinBox *> m_customFloatingBarTimeBoxes;
    QMap<QString, QSpinBox *> m_customResultPopupTimeBoxes;
    QMap<QString, QTextEdit *> m_customPromptEdits;
    QLineEdit *m_deepseekKeyEdit = nullptr;
    QLineEdit *m_openaiKeyEdit = nullptr;
    QLineEdit *m_anthropicKeyEdit = nullptr;
    QLineEdit *m_baiduApiKeyEdit = nullptr;
    QLineEdit *m_baiduSecretKeyEdit = nullptr;
    QLineEdit *m_baiduAppIdEdit = nullptr;
    QLineEdit *m_xfyunAppIdEdit = nullptr;
    QLineEdit *m_xfyunApiKeyEdit = nullptr;
    QLineEdit *m_xfyunApiSecretEdit = nullptr;
    QComboBox *m_speechProviderBox = nullptr;
    QCheckBox *m_autoStartBox = nullptr;
    QCheckBox *m_strongSelectionBox = nullptr;
    QWidget *m_baiduApiKeyRow = nullptr;
    QWidget *m_baiduSecretKeyRow = nullptr;
    QWidget *m_baiduAppIdRow = nullptr;
    QWidget *m_xfyunAppIdRow = nullptr;
    QWidget *m_xfyunApiKeyRow = nullptr;
    QWidget *m_xfyunApiSecretRow = nullptr;
    QLabel *m_recordDirectoryLabel = nullptr;
    QComboBox *m_settingsPromptSelector = nullptr;
    QTextEdit *m_settingsPromptEditor = nullptr;
    QPushButton *m_settingsPromptSaveButton = nullptr;
    QCheckBox *m_settingsPromptLock = nullptr;

    void setComboCurrentData(QComboBox *box, const QString &value)
    {
        if (!box) {
            return;
        }
        const int index = box->findData(value);
        if (index < 0 || index == box->currentIndex()) {
            return;
        }
        box->blockSignals(true);
        box->setCurrentIndex(index);
        box->blockSignals(false);
    }

    void clearCustomFunctionRows()
    {
        if (!m_customListLayout) {
            return;
        }
        while (QLayoutItem *item = m_customListLayout->takeAt(0)) {
            if (QWidget *widget = item->widget()) {
                widget->deleteLater();
            }
            delete item;
        }
        m_customNameEdits.clear();
        m_customShortcutEdits.clear();
        m_customModelBoxes.clear();
        m_customOutputModeBoxes.clear();
        m_customSelectionBoxes.clear();
        m_customVoiceBoxes.clear();
        m_customFloatingBarTimeBoxes.clear();
        m_customResultPopupTimeBoxes.clear();
        m_customPromptEdits.clear();
    }

    void rebuildCustomFunctionRows()
    {
        if (!m_customListLayout) {
            return;
        }
        clearCustomFunctionRows();
        m_customListLayout->addStretch();
        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            addCustomFunctionRow(fn);
        }
    }

    void showSettingsHelp()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(tr8("设置帮助"));
        dialog.setMinimumSize(680, 560);
        dialog.setFont(appFont());
        dialog.setStyleSheet(QStringLiteral(
            "QDialog { background: #f6f7f9; }"
            "QLabel { color: #111827; }"
        ));

        auto *root = new QVBoxLayout(&dialog);
        root->setContentsMargins(22, 20, 22, 18);
        root->setSpacing(14);

        auto *title = new QLabel(tr8("设置帮助"));
        title->setFont(appFont(18, QFont::DemiBold));
        root->addWidget(title);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background: transparent; }"
        ));

        auto *holder = new QWidget;
        auto *items = new QVBoxLayout(holder);
        items->setContentsMargins(0, 0, 10, 0);
        items->setSpacing(12);

        items->addWidget(settingsHelpCard(
            tr8("常用设置"),
            tr8("控制软件后台行为。这里可以设置托盘常驻、开机自启动、强力选中、浮动条是否显示、听写后是否调用模型整理、是否使用系统代理，以及录音和历史记录保存位置。")
        ));
        items->addWidget(settingsHelpCard(
            tr8("快捷键"),
            tr8("修改听写、翻译、问答和打开主界面的快捷键。自定义功能快捷键在左侧“功能自定义”里修改。")
        ));
        items->addWidget(settingsHelpCard(
            tr8("功能自定义"),
            tr8("功能自定义不在设置页签里，而在主界面左侧“功能自定义”里。那里可以调整听写、翻译、问答和自定义功能的模型、输入方式、展现方式、显示时间和提示词。")
        ));
        items->addWidget(settingsHelpCard(
            tr8("接口"),
            tr8("填写语音识别和大模型接口密钥。语音识别分为百度和讯飞；大模型分为 DeepSeek、OpenAI 和 Claude。接口页顶部有固定的保存按钮，不需要滑到最底部再保存。")
        ));
        items->addWidget(settingsHelpCard(
            tr8("常见问题"),
            tr8("常见问题不在设置页签里，而在主界面左侧“常见问题”里。那里按弹窗报错原文整理了解决办法，适合测试人员遇到问题时直接对照。")
        ));
        items->addStretch();

        scroll->setWidget(holder);
        root->addWidget(scroll, 1);

        auto *footer = new QHBoxLayout;
        footer->addStretch();
        auto *close = new QPushButton(tr8("关闭"));
        close->setFixedHeight(36);
        close->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        footer->addWidget(close);
        root->addLayout(footer);

        dialog.exec();
    }

    QWidget *settingsHelpCard(const QString &title, const QString &text)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(8);

        auto *titleLabel = new QLabel(title);
        titleLabel->setFont(appFont(12, QFont::DemiBold));

        auto *body = new QLabel(text);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        body->setStyleSheet(QStringLiteral("color: #475467;"));

        layout->addWidget(titleLabel);
        layout->addWidget(body);
        return frame;
    }

    QWidget *shortcutsTab()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(22, 22, 22, 22);
        layout->setSpacing(14);

        for (const HotkeyDef &def : hotkeyDefs()) {
            layout->addWidget(hotkeyRow(def));
        }
        layout->addStretch();
        return page;
    }

    QWidget *generalTab()
    {
        auto *page = new QWidget;
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *holder = new QWidget;
        auto *layout = new QVBoxLayout(holder);
        layout->setContentsMargins(22, 22, 22, 22);
        layout->setSpacing(14);

        layout->addWidget(toggleRow(
            tr8("托盘常驻"),
            tr8("开启时关闭窗口只隐藏到托盘；关闭时关闭窗口会退出程序。"),
            m_settings->trayResident(),
            [this](bool enabled) {
                m_settings->setTrayResident(enabled);
                saveAndRefresh();
            }
        ));

        layout->addWidget(autoStartRow());

        layout->addWidget(strongSelectionRow());

        layout->addWidget(toggleRow(
            tr8("启用浮动条"),
            tr8("开启后只在语音输入、识别和处理时临时显示；语音输入结束后自动关闭。"),
            m_settings->floatingBarEnabled(),
            [this](bool enabled) {
                m_settings->setFloatingBarEnabled(enabled);
                saveAndRefresh();
            }
        ));

        layout->addWidget(toggleRow(
            tr8("启用录音倒计时"),
            tr8("开启后按下快捷键不会立刻录音，会先显示 3 秒倒计时，避免还没准备好就开始说。"),
            m_settings->preRecordCountdownEnabled(),
            [this](bool enabled) {
                m_settings->setPreRecordCountdownEnabled(enabled);
                saveAndRefresh();
            }
        ));

        layout->addWidget(toggleRow(
            tr8("启用录音提示音"),
            tr8("开启后开始录音前会播放系统提示音。和倒计时可以同时使用，也可以单独使用。"),
            m_settings->recordingBeepEnabled(),
            [this](bool enabled) {
                m_settings->setRecordingBeepEnabled(enabled);
                saveAndRefresh();
            }
        ));

        layout->addWidget(toggleRow(
            tr8("听写后调用模型整理"),
            tr8("关闭时听写会跳过大模型，识别完成后直接写入，速度更快；开启后会更自然但更慢。"),
            m_settings->dictatePolishEnabled(),
            [this](bool enabled) {
                m_settings->setDictatePolishEnabled(enabled);
                saveAndRefresh();
            }
        ));

        layout->addWidget(toggleRow(
            tr8("使用系统代理"),
            tr8("默认关闭，软件直连网络，不跟随 VPN 或 Windows 代理。注意：TUN 模式、透明代理或虚拟网卡会在网卡层接管流量，软件直连也可能被代理。"),
            m_settings->useSystemProxy(),
            [this](bool enabled) {
                m_settings->setUseSystemProxy(enabled);
                saveAndRefresh();
            }
        ));

        layout->addWidget(recordDirectoryRow());
        layout->addWidget(historyLoadCountRow());

        layout->addStretch();
        scroll->setWidget(holder);
        pageLayout->addWidget(scroll);
        return page;
    }

    QWidget *modelsTab()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(22, 22, 22, 22);
        layout->setSpacing(14);

        for (const HotkeyDef &def : coreFunctionDefs()) {
            layout->addWidget(modelRow(def.id, def.title, def.hint));
        }

        layout->addStretch();
        return page;
    }

    QWidget *outputModesTab()
    {
        auto *page = new QWidget;
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *holder = new QWidget;
        auto *layout = new QVBoxLayout(holder);
        layout->setContentsMargins(22, 22, 22, 22);
        layout->setSpacing(14);

        for (const HotkeyDef &def : coreFunctionDefs()) {
            layout->addWidget(outputModeRow(def.id, def.title, def.hint));
        }

        layout->addStretch();
        scroll->setWidget(holder);
        pageLayout->addWidget(scroll);
        return page;
    }

    QWidget *promptTab()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(22, 22, 22, 22);
        layout->setSpacing(12);

        auto *tools = new QHBoxLayout;
        m_settingsPromptSelector = new QComboBox;
        m_settingsPromptSelector->setFixedHeight(34);
        m_settingsPromptSelector->setMinimumWidth(280);
        m_settingsPromptSelector->setStyleSheet(QStringLiteral(
            "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
        ));
        connect(m_settingsPromptSelector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]() {
            loadSettingsPromptEditor();
        });

        m_settingsPromptLock = new QCheckBox(tr8("锁定提示词"));
        m_settingsPromptLock->setFont(appFont(10, QFont::DemiBold));
        m_settingsPromptLock->setChecked(m_settings->promptLocked());
        connect(m_settingsPromptLock, &QCheckBox::toggled, this, [this](bool locked) {
            m_settings->setPromptLocked(locked);
            updateSettingsPromptLock();
            saveAndRefresh();
        });

        tools->addWidget(m_settingsPromptSelector);
        tools->addWidget(m_settingsPromptLock);
        tools->addStretch();
        layout->addLayout(tools);

        m_settingsPromptEditor = new QTextEdit;
        m_settingsPromptEditor->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #dde2ea;"
            "  border-radius: 8px;"
            "  padding: 10px;"
            "}"
        ));
        layout->addWidget(m_settingsPromptEditor, 1);

        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(14);
        buttons->setContentsMargins(0, 4, 0, 0);
        m_settingsPromptSaveButton = new QPushButton(tr8("保存提示词"));
        m_settingsPromptSaveButton->setFixedHeight(42);
        m_settingsPromptSaveButton->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(m_settingsPromptSaveButton, &QPushButton::clicked, this, [this]() {
            saveSettingsPromptFromEditor();
        });

        auto *reload = new QPushButton(tr8("重新读取"));
        reload->setFixedHeight(42);
        reload->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(reload, &QPushButton::clicked, this, [this]() {
            loadSettingsPromptEditor();
        });

        auto *openFolder = new QPushButton(tr8("打开提示词目录"));
        openFolder->setFixedHeight(42);
        openFolder->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(openFolder, &QPushButton::clicked, this, []() {
            const QString path = QDir(appBasePath()).filePath(QStringLiteral("prompts"));
            QDir().mkpath(path);
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });

        buttons->addWidget(m_settingsPromptSaveButton);
        buttons->addWidget(reload);
        buttons->addWidget(openFolder);
        buttons->addStretch();
        layout->addLayout(buttons);

        refreshSettingsPromptSelector();
        return page;
    }

    void refreshSettingsPromptSelector()
    {
        if (!m_settingsPromptSelector) {
            return;
        }
        const QString previous = m_settingsPromptSelector->currentData().toString();
        m_settingsPromptSelector->blockSignals(true);
        m_settingsPromptSelector->clear();
        for (const PromptTargetInfo &target : sharedPromptTargets(m_settings)) {
            m_settingsPromptSelector->addItem(target.title, target.id);
        }
        const int index = m_settingsPromptSelector->findData(previous);
        if (index >= 0) {
            m_settingsPromptSelector->setCurrentIndex(index);
        }
        m_settingsPromptSelector->blockSignals(false);
        loadSettingsPromptEditor();
    }

    void loadSettingsPromptEditor()
    {
        if (!m_settingsPromptSelector || !m_settingsPromptEditor) {
            return;
        }
        const PromptTargetInfo target = sharedPromptTargetForId(m_settings, m_settingsPromptSelector->currentData().toString());
        m_settingsPromptEditor->setPlainText(sharedPromptText(m_settings, target));
        updateSettingsPromptLock();
    }

    void updateSettingsPromptLock()
    {
        const bool locked = m_settings->promptLocked();
        if (m_settingsPromptLock && m_settingsPromptLock->isChecked() != locked) {
            m_settingsPromptLock->blockSignals(true);
            m_settingsPromptLock->setChecked(locked);
            m_settingsPromptLock->blockSignals(false);
        }
        if (m_settingsPromptEditor) {
            m_settingsPromptEditor->setDisabled(locked);
        }
        if (m_settingsPromptSaveButton) {
            m_settingsPromptSaveButton->setDisabled(locked);
        }
    }

    void saveSettingsPromptFromEditor()
    {
        if (!m_settingsPromptSelector || !m_settingsPromptEditor) {
            return;
        }
        if (m_settings->promptLocked()) {
            showAttentionInformation(this, tr8("提示词已锁定"), tr8("请先取消锁定后再修改提示词。"));
            return;
        }

        const PromptTargetInfo target = sharedPromptTargetForId(m_settings, m_settingsPromptSelector->currentData().toString());
        QString error;
        if (!saveSharedPromptText(m_settings, target, m_settingsPromptEditor->toPlainText(), &error)) {
            showAttentionWarning(this, tr8("保存失败"), error.isEmpty() ? tr8("无法保存提示词。") : error);
            return;
        }

        saveAndRefresh();
        showAttentionInformation(this, tr8("已保存"), tr8("提示词已保存，并已同步到主界面左侧提示词页。"));
    }

    QWidget *customFunctionsTab()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(22, 22, 22, 22);
        layout->setSpacing(12);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *holder = new QWidget;
        m_customListLayout = new QVBoxLayout(holder);
        m_customListLayout->setContentsMargins(0, 0, 0, 0);
        m_customListLayout->setSpacing(12);

        m_customNameEdits.clear();
        m_customShortcutEdits.clear();
        m_customModelBoxes.clear();
        m_customOutputModeBoxes.clear();
        m_customSelectionBoxes.clear();
        m_customVoiceBoxes.clear();
        m_customFloatingBarTimeBoxes.clear();
        m_customResultPopupTimeBoxes.clear();
        m_customPromptEdits.clear();

        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            addCustomFunctionRow(fn);
        }
        m_customListLayout->addStretch();

        scroll->setWidget(holder);
        layout->addWidget(scroll, 1);

        auto *buttons = new QHBoxLayout;
        auto *add = new QPushButton(tr8("新增自定义功能"));
        add->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        auto *save = new QPushButton(tr8("保存自定义功能"));
        save->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        buttons->addWidget(add);
        buttons->addWidget(save);
        buttons->addStretch();
        layout->addLayout(buttons);

        connect(add, &QPushButton::clicked, this, [this]() {
            CustomFunctionDef fn;
            fn.id = m_settings->nextCustomFunctionId();
            fn.name = tr8("自定义功能 ") + fn.id.mid(7);
            fn.shortcut = m_settings->suggestedCustomShortcut();
            fn.model = QStringLiteral("deepseek-v4-flash");
            fn.outputMode = outputModePopup();
            fn.useSelection = true;
            fn.useVoice = true;
            fn.floatingBarSeconds = defaultFloatingBarSeconds();
            fn.resultPopupSeconds = defaultResultPopupSeconds();
            fn.countdownSeconds = defaultCountdownSeconds();
            fn.recordingBeepEnabled = true;
            fn.recordingBeepPath.clear();
            fn.prompt = tr8("请根据选中文本和我的语音要求完成任务，输出可以直接使用的结果。");
            m_settings->addCustomFunction(fn);
            addCustomFunctionRow(fn);
            saveAndRefresh();
        });

        connect(save, &QPushButton::clicked, this, [this]() {
            saveCustomFunctionsFromUi();
        });

        return page;
    }

    QWidget *apiTab()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(22, 22, 22, 22);
        layout->setSpacing(12);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto *holder = new QWidget;
        auto *content = new QVBoxLayout(holder);
        content->setContentsMargins(0, 0, 0, 0);
        content->setSpacing(14);

        const SecretConfig secrets = loadSecrets();

        m_deepseekKeyEdit = newSecretEdit(secrets.deepseekApiKey);
        m_openaiKeyEdit = newSecretEdit(secrets.openaiApiKey);
        m_anthropicKeyEdit = newSecretEdit(secrets.anthropicApiKey);
        m_baiduApiKeyEdit = newSecretEdit(secrets.baiduApiKey);
        m_baiduSecretKeyEdit = newSecretEdit(secrets.baiduSecretKey);
        m_baiduAppIdEdit = newSecretEdit(secrets.baiduAppId);
        m_xfyunAppIdEdit = newSecretEdit(secrets.xfyunAppId);
        m_xfyunApiKeyEdit = newSecretEdit(secrets.xfyunApiKey);
        m_xfyunApiSecretEdit = newSecretEdit(secrets.xfyunApiSecret);

        QVector<QWidget *> voiceRows;
        voiceRows.append(speechProviderRow());
        m_baiduApiKeyRow = secretInputRow(tr8("百度接口密钥（API Key）"), tr8("语音转文字时使用"), m_baiduApiKeyEdit);
        m_baiduSecretKeyRow = secretInputRow(tr8("百度安全密钥（Secret Key）"), tr8("用于获取语音识别访问令牌"), m_baiduSecretKeyEdit);
        m_baiduAppIdRow = secretInputRow(tr8("百度应用编号（AppID）"), tr8("可选，便于记录应用来源"), m_baiduAppIdEdit);
        m_xfyunAppIdRow = secretInputRow(tr8("讯飞应用编号（AppID）"), tr8("讯飞语音听写应用编号"), m_xfyunAppIdEdit);
        m_xfyunApiKeyRow = secretInputRow(tr8("讯飞接口密钥（API Key）"), tr8("用于生成讯飞接口鉴权签名"), m_xfyunApiKeyEdit);
        m_xfyunApiSecretRow = secretInputRow(tr8("讯飞安全密钥（API Secret）"), tr8("用于生成讯飞接口鉴权签名"), m_xfyunApiSecretEdit);
        voiceRows.append(m_baiduApiKeyRow);
        voiceRows.append(m_baiduSecretKeyRow);
        voiceRows.append(m_baiduAppIdRow);
        voiceRows.append(m_xfyunAppIdRow);
        voiceRows.append(m_xfyunApiSecretRow);
        voiceRows.append(m_xfyunApiKeyRow);

        QVector<QWidget *> modelRows;
        modelRows.append(secretInputRow(tr8("DeepSeek 密钥（API Key）"), tr8("选择 DeepSeek 模型时使用"), m_deepseekKeyEdit));
        modelRows.append(secretInputRow(tr8("OpenAI 密钥（GPT API Key）"), tr8("选择 GPT 模型时使用"), m_openaiKeyEdit));
        modelRows.append(secretInputRow(tr8("Anthropic 密钥（Claude API Key）"), tr8("选择 Claude 模型时使用"), m_anthropicKeyEdit));

        content->addWidget(secretSection(tr8("语音识别接口"), tr8("用于听写、问答和自定义功能里的语音输入。可以选择百度语音识别或讯飞语音听写。"), voiceRows));
        content->addWidget(secretSection(tr8("大模型接口"), tr8("用于整理听写内容、翻译、问答和自定义功能。"), modelRows));
        updateSpeechSecretRows();

        content->addStretch();
        scroll->setWidget(holder);
        layout->addWidget(scroll, 1);

        auto *buttons = new QHBoxLayout;
        auto *save = new QPushButton(tr8("保存接口配置"));
        save->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        buttons->addWidget(save);
        buttons->addStretch();
        layout->addLayout(buttons);

        connect(save, &QPushButton::clicked, this, [this]() {
            saveSecretsFromUi(true);
        });

        return page;
    }

    QLineEdit *newSecretEdit(const QString &value)
    {
        auto *edit = new QLineEdit(value);
        edit->setEchoMode(QLineEdit::Password);
        edit->setMinimumHeight(34);
        edit->setPlaceholderText(tr8("在这里填写"));
        edit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "}"
        ));
        return edit;
    }

    QWidget *speechProviderRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("secretRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#secretRow {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "}"
        ));
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(tr8("当前语音识别服务"));
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        m_speechProviderBox = new QComboBox;
        m_speechProviderBox->addItem(tr8("百度语音识别"), speechProviderBaidu());
        m_speechProviderBox->addItem(tr8("讯飞语音听写"), speechProviderXfyun());
        const int index = m_speechProviderBox->findData(m_settings->speechProvider());
        m_speechProviderBox->setCurrentIndex(index >= 0 ? index : 0);
        m_speechProviderBox->setMinimumWidth(240);
        m_speechProviderBox->setFixedHeight(34);
        connect(m_speechProviderBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]() {
            updateSpeechSecretRows();
        });

        layout->addLayout(labels, 1);
        layout->addWidget(m_speechProviderBox);
        return frame;
    }

    void updateSpeechSecretRows()
    {
        const QString provider = m_speechProviderBox
            ? normalizeSpeechProvider(m_speechProviderBox->currentData().toString())
            : m_settings->speechProvider();
        const bool showBaidu = provider == speechProviderBaidu();
        const bool showXfyun = provider == speechProviderXfyun();

        const QVector<QWidget *> baiduRows = QVector<QWidget *>()
            << m_baiduApiKeyRow
            << m_baiduSecretKeyRow
            << m_baiduAppIdRow;
        for (QWidget *row : baiduRows) {
            if (row) {
                row->setVisible(showBaidu);
                row->setEnabled(showBaidu);
            }
        }

        const QVector<QWidget *> xfyunRows = QVector<QWidget *>()
            << m_xfyunAppIdRow
            << m_xfyunApiKeyRow
            << m_xfyunApiSecretRow;
        for (QWidget *row : xfyunRows) {
            if (row) {
                row->setVisible(showXfyun);
                row->setEnabled(showXfyun);
            }
        }
    }

    QWidget *secretSection(const QString &title, const QString &hint, const QVector<QWidget *> &rows)
    {
        Q_UNUSED(hint);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("apiSection"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#apiSection {"
            "  background: #ffffff;"
            "  border: 1px solid #dde2ea;"
            "  border-radius: 8px;"
            "}"
        ));

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(12);

        auto *name = new QLabel(title);
        name->setFont(appFont(13, QFont::DemiBold));
        layout->addWidget(name);

        for (QWidget *row : rows) {
            layout->addWidget(row);
        }
        return frame;
    }

    QWidget *secretInputRow(const QString &title, const QString &hint, QLineEdit *edit)
    {
        Q_UNUSED(hint);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("secretRow"));
        frame->setStyleSheet(QStringLiteral(
            "QFrame#secretRow {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "}"
        ));
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *show = new QPushButton(tr8("显示"));
        show->setFixedHeight(34);
        show->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(show, &QPushButton::clicked, this, [edit, show]() {
            if (edit->echoMode() == QLineEdit::Password) {
                edit->setEchoMode(QLineEdit::Normal);
                show->setText(tr8("隐藏"));
            } else {
                edit->setEchoMode(QLineEdit::Password);
                show->setText(tr8("显示"));
            }
        });

        layout->addLayout(labels, 1);
        layout->addWidget(edit, 2);
        layout->addWidget(show);
        return frame;
    }

    bool saveSecretsFromUi(bool showConfirmation)
    {
        SecretConfig secrets;
        secrets.deepseekApiKey = m_deepseekKeyEdit ? m_deepseekKeyEdit->text().trimmed() : QString();
        secrets.openaiApiKey = m_openaiKeyEdit ? m_openaiKeyEdit->text().trimmed() : QString();
        secrets.anthropicApiKey = m_anthropicKeyEdit ? m_anthropicKeyEdit->text().trimmed() : QString();
        secrets.baiduApiKey = m_baiduApiKeyEdit ? m_baiduApiKeyEdit->text().trimmed() : QString();
        secrets.baiduSecretKey = m_baiduSecretKeyEdit ? m_baiduSecretKeyEdit->text().trimmed() : QString();
        secrets.baiduAppId = m_baiduAppIdEdit ? m_baiduAppIdEdit->text().trimmed() : QString();
        secrets.xfyunAppId = m_xfyunAppIdEdit ? m_xfyunAppIdEdit->text().trimmed() : QString();
        secrets.xfyunApiKey = m_xfyunApiKeyEdit ? m_xfyunApiKeyEdit->text().trimmed() : QString();
        secrets.xfyunApiSecret = m_xfyunApiSecretEdit ? m_xfyunApiSecretEdit->text().trimmed() : QString();

        if (!saveSecrets(secrets)) {
            showAttentionWarning(this, tr8("保存失败"), tr8("无法写入 config/secrets.json。"));
            return false;
        }
        if (m_speechProviderBox) {
            m_settings->setSpeechProvider(m_speechProviderBox->currentData().toString());
        }
        if (!m_settings->save()) {
            showAttentionWarning(this, tr8("保存失败"), tr8("接口密钥已保存，但无法写入语音识别服务选择。"));
            return false;
        }
        if (m_onChanged) {
            m_onChanged();
        }
        if (showConfirmation) {
            showAttentionInformation(this, tr8("已保存"), tr8("接口配置和语音识别服务已保存，并会在下次调用时生效。"));
        }
        return true;
    }

    QComboBox *modelCombo(const QString &currentModel)
    {
        auto *box = new QComboBox;
        box->setFixedHeight(34);
        for (const ModelOption &option : modelOptions()) {
            box->addItem(option.title, option.id);
        }

        const int index = box->findData(currentModel);
        if (index >= 0) {
            box->setCurrentIndex(index);
        }
        box->setStyleSheet(QStringLiteral(
            "QComboBox {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "}"
        ));
        return box;
    }

    QWidget *modelRow(const QString &id, const QString &title, const QString &hint)
    {
        Q_UNUSED(hint);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *box = modelCombo(m_settings->modelFor(id));
        box->setMinimumWidth(260);
        m_modelBoxes.insert(id, box);
        connect(box, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this, id, box]() {
            m_settings->setModelFor(id, box->currentData().toString());
            saveAndRefresh();
        });

        layout->addLayout(labels, 1);
        layout->addWidget(box);
        return frame;
    }

    QComboBox *outputModeCombo(const QString &currentMode)
    {
        auto *box = new QComboBox;
        box->setFixedHeight(34);
        box->addItem(outputModeTitle(outputModeAutoWrite()), outputModeAutoWrite());
        box->addItem(outputModeTitle(outputModePopup()), outputModePopup());

        const int index = box->findData(normalizeOutputMode(currentMode));
        if (index >= 0) {
            box->setCurrentIndex(index);
        }
        box->setStyleSheet(QStringLiteral(
            "QComboBox {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "}"
        ));
        return box;
    }

    QSpinBox *displayTimeSpinBox(int seconds, bool allowManualClose, const QString &zeroText = QString())
    {
        auto *box = new QSpinBox;
        const bool allowZero = allowManualClose || !zeroText.trimmed().isEmpty();
        box->setRange(allowZero ? 0 : 1, allowManualClose ? 600 : 60);
        box->setSuffix(tr8(" 秒"));
        if (allowManualClose) {
            box->setSpecialValueText(tr8("手动关闭"));
        } else if (!zeroText.trimmed().isEmpty()) {
            box->setSpecialValueText(zeroText.trimmed());
        }
        box->setValue(seconds);
        box->setFixedSize(130, 34);
        box->setStyleSheet(QStringLiteral(
            "QSpinBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 4px 8px; }"
        ));
        return box;
    }

    QWidget *outputModeRow(const QString &id, const QString &title, const QString &hint)
    {
        Q_UNUSED(hint);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *box = outputModeCombo(m_settings->outputModeFor(id));
        box->setMinimumWidth(220);
        m_outputModeBoxes.insert(id, box);
        connect(box, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this, id, box]() {
            m_settings->setOutputModeFor(id, box->currentData().toString());
            saveAndRefresh();
        });

        auto *useSelection = new QCheckBox(tr8("读取鼠标选中的文字"));
        useSelection->setChecked(m_settings->useSelectionFor(id));
        useSelection->setFont(appFont(10, QFont::DemiBold));
        auto *useVoice = new QCheckBox(tr8("使用语音输入"));
        useVoice->setChecked(m_settings->useVoiceFor(id));
        useVoice->setFont(appFont(10, QFont::DemiBold));
        auto *floatingTime = displayTimeSpinBox(m_settings->floatingBarSecondsFor(id), false, tr8("不调用"));
        auto *popupTime = displayTimeSpinBox(m_settings->resultPopupSecondsFor(id), true);

        connect(useSelection, &QCheckBox::toggled, this, [this, id, useSelection, useVoice](bool enabled) {
            if (!enabled && !useVoice->isChecked()) {
                useSelection->blockSignals(true);
                useSelection->setChecked(true);
                useSelection->blockSignals(false);
                showAttentionInformation(this, tr8("需要输入方式"), tr8("至少需要启用“读取鼠标选中的文字”或“使用语音输入”中的一种。"));
                return;
            }
            m_settings->setUseSelectionFor(id, enabled);
            saveAndRefresh();
        });
        connect(useVoice, &QCheckBox::toggled, this, [this, id, useSelection, useVoice](bool enabled) {
            if (!enabled && !useSelection->isChecked()) {
                useVoice->blockSignals(true);
                useVoice->setChecked(true);
                useVoice->blockSignals(false);
                showAttentionInformation(this, tr8("需要输入方式"), tr8("至少需要启用“读取鼠标选中的文字”或“使用语音输入”中的一种。"));
                return;
            }
            m_settings->setUseVoiceFor(id, enabled);
            saveAndRefresh();
        });
        connect(floatingTime, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this, id](int seconds) {
            m_settings->setFloatingBarSecondsFor(id, seconds);
            saveAndRefresh();
        });
        connect(popupTime, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this, id](int seconds) {
            m_settings->setResultPopupSecondsFor(id, seconds);
            saveAndRefresh();
        });

        auto *floatingTimeRow = new QHBoxLayout;
        floatingTimeRow->setContentsMargins(0, 0, 0, 0);
        floatingTimeRow->addWidget(new QLabel(tr8("浮动条")));
        floatingTimeRow->addStretch();
        floatingTimeRow->addWidget(floatingTime);

        auto *popupTimeRow = new QHBoxLayout;
        popupTimeRow->setContentsMargins(0, 0, 0, 0);
        popupTimeRow->addWidget(new QLabel(tr8("结果小框")));
        popupTimeRow->addStretch();
        popupTimeRow->addWidget(popupTime);

        auto *controls = new QVBoxLayout;
        controls->setContentsMargins(0, 0, 0, 0);
        controls->setSpacing(8);
        controls->addWidget(box);
        controls->addWidget(useSelection);
        controls->addWidget(useVoice);
        controls->addLayout(floatingTimeRow);
        controls->addLayout(popupTimeRow);

        layout->addLayout(labels, 1);
        layout->addLayout(controls);
        return frame;
    }

    void addCustomFunctionRow(const CustomFunctionDef &fn)
    {
        if (!m_customListLayout) {
            return;
        }

        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());

        auto *outer = new QVBoxLayout(frame);
        outer->setContentsMargins(16, 14, 16, 14);
        outer->setSpacing(10);

        auto *top = new QHBoxLayout;
        auto *title = new QLabel(tr8("自定义功能"));
        title->setFont(appFont(12, QFont::DemiBold));
        auto *remove = new QPushButton(tr8("删除"));
        remove->setFont(appFont(10, QFont::DemiBold));
        remove->setFixedSize(92, 42);
        remove->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        top->addWidget(title);
        top->addStretch();
        top->addWidget(remove);
        outer->addLayout(top);

        auto *form = new QGridLayout;
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(8);

        auto *nameEdit = new QLineEdit(fn.name);
        nameEdit->setMinimumHeight(34);
        auto *shortcutEdit = new QKeySequenceEdit(QKeySequence(fn.shortcut));
        shortcutEdit->setMinimumHeight(34);
        auto *modelEdit = modelCombo(fn.model);
        auto *outputModeEdit = outputModeCombo(fn.outputMode);
        auto *useSelection = new QCheckBox(tr8("读取鼠标选中的文字"));
        useSelection->setChecked(fn.useSelection);
        useSelection->setFont(appFont(10, QFont::DemiBold));
        auto *useVoice = new QCheckBox(tr8("使用语音输入"));
        useVoice->setChecked(fn.useVoice);
        useVoice->setFont(appFont(10, QFont::DemiBold));
        auto *floatingTime = displayTimeSpinBox(fn.floatingBarSeconds, false, tr8("不调用"));
        auto *popupTime = displayTimeSpinBox(fn.resultPopupSeconds, true);
        auto *promptEdit = new QTextEdit;
        promptEdit->setPlainText(fn.prompt);
        promptEdit->setMinimumHeight(86);
        promptEdit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #dde2ea;"
            "  border-radius: 8px;"
            "  padding: 8px;"
            "}"
        ));

        form->addWidget(new QLabel(tr8("名称")), 0, 0);
        form->addWidget(nameEdit, 0, 1);
        form->addWidget(new QLabel(tr8("快捷键")), 0, 2);
        form->addWidget(shortcutEdit, 0, 3);
        form->addWidget(new QLabel(tr8("模型")), 1, 0);
        form->addWidget(modelEdit, 1, 1, 1, 3);
        form->addWidget(new QLabel(tr8("展现方式")), 2, 0);
        form->addWidget(outputModeEdit, 2, 1, 1, 3);
        form->addWidget(new QLabel(tr8("输入方式")), 3, 0);
        auto *inputModes = new QHBoxLayout;
        inputModes->setContentsMargins(0, 0, 0, 0);
        inputModes->setSpacing(18);
        inputModes->addWidget(useSelection);
        inputModes->addWidget(useVoice);
        inputModes->addStretch();
        form->addLayout(inputModes, 3, 1, 1, 3);
        form->addWidget(new QLabel(tr8("显示时间")), 4, 0);
        auto *displayTimes = new QHBoxLayout;
        displayTimes->setContentsMargins(0, 0, 0, 0);
        displayTimes->setSpacing(10);
        displayTimes->addWidget(new QLabel(tr8("浮动条")));
        displayTimes->addWidget(floatingTime);
        displayTimes->addSpacing(10);
        displayTimes->addWidget(new QLabel(tr8("结果小框")));
        displayTimes->addWidget(popupTime);
        displayTimes->addStretch();
        form->addLayout(displayTimes, 4, 1, 1, 3);
        form->addWidget(new QLabel(tr8("提示词")), 5, 0);
        form->addWidget(promptEdit, 5, 1, 1, 3);
        form->setColumnStretch(1, 1);
        form->setColumnStretch(3, 1);
        outer->addLayout(form);

        m_customNameEdits.insert(fn.id, nameEdit);
        m_customShortcutEdits.insert(fn.id, shortcutEdit);
        m_customModelBoxes.insert(fn.id, modelEdit);
        m_customOutputModeBoxes.insert(fn.id, outputModeEdit);
        m_customSelectionBoxes.insert(fn.id, useSelection);
        m_customVoiceBoxes.insert(fn.id, useVoice);
        m_customFloatingBarTimeBoxes.insert(fn.id, floatingTime);
        m_customResultPopupTimeBoxes.insert(fn.id, popupTime);
        m_customPromptEdits.insert(fn.id, promptEdit);

        const QString rowId = fn.id;
        connect(remove, &QPushButton::clicked, this, [this, rowId, frame]() {
            m_customNameEdits.remove(rowId);
            m_customShortcutEdits.remove(rowId);
            m_customModelBoxes.remove(rowId);
            m_customOutputModeBoxes.remove(rowId);
            m_customSelectionBoxes.remove(rowId);
            m_customVoiceBoxes.remove(rowId);
            m_customFloatingBarTimeBoxes.remove(rowId);
            m_customResultPopupTimeBoxes.remove(rowId);
            m_customPromptEdits.remove(rowId);
            m_settings->removeCustomFunction(rowId);
            frame->deleteLater();
            saveAndRefresh();
        });

        const int insertIndex = qMax(0, m_customListLayout->count() - 1);
        m_customListLayout->insertWidget(insertIndex, frame);
    }

    void saveCustomFunctionsFromUi()
    {
        QMap<QString, QString> usedShortcuts;
        for (const HotkeyDef &def : hotkeyDefs()) {
            const QString normalized = m_settings->hotkey(def.id).trimmed().toLower();
            if (!normalized.isEmpty()) {
                usedShortcuts.insert(normalized, def.title);
            }
        }

        for (const QString &id : m_customNameEdits.keys()) {
            const QString name = m_customNameEdits.value(id)->text().trimmed();
            const QString shortcut = m_customShortcutEdits.value(id)->keySequence().toString(QKeySequence::PortableText);
            const QString normalized = shortcut.trimmed().toLower();
            if (name.isEmpty()) {
                showAttentionWarning(this, tr8("名称不能为空"), tr8("自定义功能必须填写名称。"));
                return;
            }
            if (shortcut.trimmed().isEmpty()) {
                showAttentionWarning(this, tr8("快捷键不能为空"), name + tr8(" 还没有设置快捷键。"));
                return;
            }
            if (usedShortcuts.contains(normalized)) {
                showAttentionWarning(this, tr8("快捷键冲突"), name + tr8(" 的快捷键已经被“") + usedShortcuts.value(normalized) + tr8("”使用。"));
                return;
            }
            usedShortcuts.insert(normalized, name);
        }

        for (const QString &id : m_customNameEdits.keys()) {
            CustomFunctionDef fn;
            fn.id = id;
            fn.name = m_customNameEdits.value(id)->text().trimmed();
            fn.shortcut = m_customShortcutEdits.value(id)->keySequence().toString(QKeySequence::PortableText);
            fn.model = m_customModelBoxes.value(id)->currentData().toString();
            fn.outputMode = m_customOutputModeBoxes.value(id)->currentData().toString();
            fn.useSelection = m_customSelectionBoxes.value(id)->isChecked();
            fn.useVoice = m_customVoiceBoxes.value(id)->isChecked();
            fn.floatingBarSeconds = m_customFloatingBarTimeBoxes.value(id)->value();
            fn.resultPopupSeconds = m_customResultPopupTimeBoxes.value(id)->value();
            fn.countdownSeconds = m_settings->countdownSecondsFor(id);
            fn.recordingBeepEnabled = m_settings->recordingBeepEnabledFor(id);
            fn.recordingBeepPath = m_settings->recordingBeepPathFor(id);
            if (!fn.useSelection && !fn.useVoice) {
                showAttentionWarning(this, tr8("需要输入方式"), fn.name + tr8(" 至少需要启用选中文字或语音输入中的一种。"));
                return;
            }
            fn.prompt = m_customPromptEdits.value(id)->toPlainText();
            m_settings->updateCustomFunction(fn);
        }

        saveAndRefresh();
        showAttentionInformation(this, tr8("已保存"), tr8("自定义功能、输入方式、模型、展现方式、显示时间和提示词已保存。"));
    }

    QWidget *hotkeyRow(const HotkeyDef &def)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(10);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(def.title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *key = new QLabel(displayShortcut(m_settings->hotkey(def.id)));
        key->setAlignment(Qt::AlignCenter);
        key->setFixedHeight(34);
        key->setMinimumWidth(170);
        key->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background: #f2f4f7;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  color: #111827;"
            "  font-weight: 600;"
            "}"
        ));
        m_hotkeyLabels.insert(def.id, key);

        auto *change = new QPushButton(tr8("更改"));
        change->setFixedHeight(34);
        change->setCursor(Qt::PointingHandCursor);
        change->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(change, &QPushButton::clicked, this, [this, def]() { editHotkey(def); });

        auto *reset = new QPushButton(tr8("重置"));
        reset->setFixedHeight(34);
        reset->setCursor(Qt::PointingHandCursor);
        reset->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(reset, &QPushButton::clicked, this, [this, def]() {
            m_settings->setHotkey(def.id, def.defaultValue);
            saveAndRefresh();
        });

        layout->addLayout(labels, 1);
        layout->addWidget(key);
        layout->addWidget(change);
        layout->addWidget(reset);
        return frame;
    }

    QWidget *toggleRow(const QString &title, const QString &hint, bool checked, const std::function<void(bool)> &onChanged)
    {
        Q_UNUSED(hint);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *box = new QCheckBox;
        box->setChecked(checked);
        box->setFont(appFont(10, QFont::DemiBold));
        connect(box, &QCheckBox::toggled, this, [onChanged](bool enabled) {
            if (onChanged) {
                onChanged(enabled);
            }
        });

        layout->addLayout(labels, 1);
        layout->addWidget(box);
        return frame;
    }

    QWidget *autoStartRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(tr8("开机自启动"));
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *box = new QCheckBox;
        m_autoStartBox = box;
        box->setChecked(m_settings->autoStartEnabled());
        box->setFont(appFont(10, QFont::DemiBold));
        connect(box, &QCheckBox::toggled, this, [this, box](bool enabled) {
            QString error;
            if (!setWindowsAutoStartEnabled(enabled, &error)) {
                box->blockSignals(true);
                box->setChecked(!enabled);
                box->blockSignals(false);
                showAttentionWarning(this, tr8("开机自启动设置失败"), error);
                return;
            }

            m_settings->setAutoStartEnabled(enabled);
            saveAndRefresh();
        });

        layout->addLayout(labels, 1);
        layout->addWidget(box);
        return frame;
    }

    QWidget *strongSelectionRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(tr8("强力选中"));
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *box = new QCheckBox;
        m_strongSelectionBox = box;
        box->setChecked(m_settings->strongSelectionEnabled());
        box->setFont(appFont(10, QFont::DemiBold));
        connect(box, &QCheckBox::toggled, this, [this](bool enabled) {
            m_settings->setStrongSelectionEnabled(enabled);
            saveAndRefresh();
        });

        layout->addLayout(labels, 1);
        layout->addWidget(box);
        return frame;
    }

    QWidget *recordDirectoryRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(tr8("历史记录保存位置"));
        name->setFont(appFont(11, QFont::DemiBold));

        m_recordDirectoryLabel = new QLabel(m_settings->recordDirectoryPath());
        m_recordDirectoryLabel->setWordWrap(true);
        m_recordDirectoryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_recordDirectoryLabel->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));

        labels->addWidget(name);
        labels->addWidget(m_recordDirectoryLabel);

        auto *choose = new QPushButton(tr8("更改位置"));
        choose->setMinimumSize(92, 34);
        choose->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(choose, &QPushButton::clicked, this, [this]() {
            const QString dir = QFileDialog::getExistingDirectory(
                this,
                tr8("选择历史记录保存位置"),
                m_settings->recordDirectoryPath(),
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
            );
            if (dir.trimmed().isEmpty()) {
                return;
            }
            m_settings->setRecordDirectoryPath(dir);
            refreshRecordDirectoryLabel();
            saveAndRefresh();
        });

        auto *open = new QPushButton(tr8("打开目录"));
        open->setMinimumSize(92, 34);
        open->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        open->setMenu(recordDirectoryOpenMenu(open, this, [this]() {
            return m_settings->recordDirectoryPath();
        }));

        auto *reset = new QPushButton(tr8("恢复默认"));
        reset->setMinimumSize(92, 34);
        reset->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(reset, &QPushButton::clicked, this, [this]() {
            m_settings->resetRecordDirectory();
            refreshRecordDirectoryLabel();
            saveAndRefresh();
        });

        layout->addLayout(labels, 1);
        layout->addWidget(choose);
        layout->addWidget(open);
        layout->addWidget(reset);
        return frame;
    }

    QWidget *historyLoadCountRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(tr8("历史记录加载数量"));
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *firstBox = new QSpinBox;
        firstBox->setRange(5, 200);
        firstBox->setSingleStep(5);
        firstBox->setSuffix(tr8(" 条"));
        firstBox->setValue(m_settings->historyInitialLoadCount());
        firstBox->setFixedWidth(110);
        firstBox->setFixedHeight(34);
        firstBox->setStyleSheet(QStringLiteral(
            "QSpinBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 4px 8px; }"
        ));

        auto *moreBox = new QSpinBox;
        moreBox->setRange(5, 200);
        moreBox->setSingleStep(5);
        moreBox->setSuffix(tr8(" 条"));
        moreBox->setValue(m_settings->historyLoadMoreCount());
        moreBox->setFixedWidth(110);
        moreBox->setFixedHeight(34);
        moreBox->setStyleSheet(firstBox->styleSheet());

        auto *controls = new QGridLayout;
        controls->setHorizontalSpacing(10);
        controls->setVerticalSpacing(8);
        auto *firstLabel = new QLabel(tr8("首次加载"));
        firstLabel->setStyleSheet(QStringLiteral("color: #111827;"));
        auto *moreLabel = new QLabel(tr8("加载更多"));
        moreLabel->setStyleSheet(QStringLiteral("color: #111827;"));
        controls->addWidget(firstLabel, 0, 0);
        controls->addWidget(firstBox, 0, 1);
        controls->addWidget(moreLabel, 1, 0);
        controls->addWidget(moreBox, 1, 1);

        connect(firstBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
            m_settings->setHistoryInitialLoadCount(value);
            saveAndRefresh();
        });
        connect(moreBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
            m_settings->setHistoryLoadMoreCount(value);
            saveAndRefresh();
        });

        layout->addLayout(labels, 1);
        layout->addLayout(controls);
        return frame;
    }

    void refreshRecordDirectoryLabel()
    {
        if (m_recordDirectoryLabel) {
            m_recordDirectoryLabel->setText(m_settings->recordDirectoryPath());
        }
    }

    void editHotkey(const HotkeyDef &def)
    {
        QDialog dialog(this);
        dialog.setWindowTitle(tr8("更改快捷键"));
        dialog.setModal(true);
        dialog.setMinimumWidth(420);

        auto *root = new QVBoxLayout(&dialog);
        root->setContentsMargins(18, 16, 18, 16);
        root->setSpacing(12);

        auto *title = new QLabel(def.title);
        title->setFont(appFont(14, QFont::DemiBold));
        root->addWidget(title);

        auto *editor = new QKeySequenceEdit(QKeySequence(m_settings->hotkey(def.id)));
        editor->setFixedHeight(42);
        editor->setStyleSheet(QStringLiteral(
            "QKeySequenceEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 8px;"
            "}"
        ));
        root->addWidget(editor);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *cancel = new QPushButton(tr8("取消"));
        cancel->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *ok = new QPushButton(tr8("保存"));
        ok->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        buttons->addWidget(cancel);
        buttons->addWidget(ok);
        root->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(ok, &QPushButton::clicked, &dialog, [&]() {
            const QString value = editor->keySequence().toString(QKeySequence::PortableText);
            if (value.trimmed().isEmpty()) {
                showAttentionWarning(&dialog, tr8("快捷键无效"), tr8("请选择一个有效的快捷键。"));
                return;
            }

            QString otherTitle;
            if (m_settings->conflictsWithOther(def.id, value, &otherTitle)) {
                showAttentionWarning(&dialog, tr8("快捷键冲突"), tr8("这个快捷键已经被“") + otherTitle + tr8("”使用。"));
                return;
            }

            m_settings->setHotkey(def.id, value);
            saveAndRefresh();
            dialog.accept();
        });

        dialog.exec();
    }

    void saveAndRefresh()
    {
        if (!m_settings->save()) {
            showAttentionWarning(this, tr8("保存失败"), tr8("无法写入 config/settings.json。"));
        }

        for (const HotkeyDef &def : hotkeyDefs()) {
            if (m_hotkeyLabels.contains(def.id)) {
                m_hotkeyLabels.value(def.id)->setText(displayShortcut(m_settings->hotkey(def.id)));
            }
        }
        refreshRecordDirectoryLabel();
        refreshSettingsPromptSelector();

        if (m_onChanged) {
            m_onChanged();
        }
    }

    QWidget *secretRow(const QString &title, const QString &hint)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QHBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);

        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        auto *state = new QLabel(hint);
        state->setStyleSheet(QStringLiteral("color: #667085;"));

        layout->addWidget(name);
        layout->addStretch();
        layout->addWidget(state);
        return frame;
    }
};

// 主界面窗口：承载首页、历史、提示词、自定义功能、测试工具、设置和常见问题页面。
class HubWindow : public QMainWindow
{
public:
    explicit HubWindow(AppSettings *settings, FloatingBar *floatingBar = nullptr, const std::function<void()> &onSettingsChanged = std::function<void()>(), QWidget *parent = nullptr)
        : QMainWindow(parent), m_settings(settings), m_floatingBar(floatingBar), m_onSettingsChanged(onSettingsChanged)
    {
        setWindowTitle(tr8("vocekit"));
        resize(1280, 820);
        setMinimumSize(1100, 720);
        setFont(appFont());

        auto *central = new QWidget;
        central->setObjectName(QStringLiteral("central"));
        central->setStyleSheet(QStringLiteral(
            "QWidget#central { background: #f6f7f9; color: #111827; }"
            "QLabel { background: transparent; }"
        ));
        setCentralWidget(central);

        auto *root = new QHBoxLayout(central);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        root->addWidget(sidebar());
        root->addWidget(pagesWidget(), 1);
        selectPage(QStringLiteral("home"));
    }

    void showSettingsPage(int initialTab = 0)
    {
        openSettingsDialog(initialTab);
    }

    void refreshShortcuts()
    {
        if (m_modeGridLayout) {
            populateModeGrid();
        }
        refreshStatusLabels();
        refreshPromptSelector();
        refreshCustomFunctionsPage();
        refreshRecordDirectoryViews();
    }

    void applySettingsChanged()
    {
        refreshShortcuts();
        if (m_historyTabs && m_historyCacheValid) {
            resetHistoryTabLoadedState();
            rebuildHistoryTabPlaceholders(qMax(0, m_historyTabs->currentIndex()), QString());
            populateHistoryTab(qMax(0, m_historyTabs->currentIndex()));
        }
    }

private:
    AppSettings *m_settings;
    FloatingBar *m_floatingBar = nullptr;
    std::function<void()> m_onSettingsChanged;
    QMap<QString, QLabel *> m_modeShortcutLabels;
    QMap<QString, QLabel *> m_statusValueLabels;
    QMap<QString, QPushButton *> m_navButtons;
    QGridLayout *m_modeGridLayout = nullptr;
    QStackedWidget *m_pages = nullptr;
    QTabWidget *m_historyTabs = nullptr;
    QTabWidget *m_vocabularyTabs = nullptr;
    SettingsPanel *m_settingsPanel = nullptr;
    QVBoxLayout *m_hubCustomListLayout = nullptr;
    QComboBox *m_promptSelector = nullptr;
    QTextEdit *m_promptEditor = nullptr;
    QPushButton *m_promptSaveButton = nullptr;
    QCheckBox *m_promptLock = nullptr;
    QLabel *m_hubRecordDirectoryLabel = nullptr;
    QLineEdit *m_historySearchEdit = nullptr;
    QPushButton *m_historyBatchToggleButton = nullptr;
    QPushButton *m_historySelectAllButton = nullptr;
    QPushButton *m_historyClearSelectionButton = nullptr;
    QPushButton *m_historyBatchDeleteButton = nullptr;
    QLabel *m_historySelectionLabel = nullptr;
    QLineEdit *m_vocabularySearchEdit = nullptr;
    QLineEdit *m_testSearchEdit = nullptr;
    QVBoxLayout *m_testItemsLayout = nullptr;
    QLabel *m_testEmptyLabel = nullptr;
    QPushButton *m_testFaqMatchButton = nullptr;
    QLineEdit *m_faqSearchEdit = nullptr;
    QVBoxLayout *m_faqItemsLayout = nullptr;
    QLabel *m_faqEmptyLabel = nullptr;
    QLabel *m_interfaceCheckResult = nullptr;
    QLabel *m_networkDiagnosticResult = nullptr;
    QLabel *m_microphoneTestResult = nullptr;
    QPushButton *m_interfaceCheckButton = nullptr;
    QPushButton *m_networkDiagnosticButton = nullptr;
    QPushButton *m_microphoneTestButton = nullptr;
    AudioRecorder m_microphoneTestRecorder;
    bool m_microphoneTesting = false;

    struct HistoryEntry
    {
        QString modeId;
        QString mode;
        QString time;
        QString input;
        QString output;
        QString error;
        QString audio;
        QString textFile;
        QString allAudioFile;
        QString allTextFile;
        QString allDetailFile;
        QString filePath;
        QString model;
        qint64 elapsedMs = -1;
        bool favorite = false;
        QString favoriteFolder;
        bool draft = false;
    };

    struct HistoryTabDef
    {
        QString id;
        QString title;
    };

    QVector<HistoryEntry> m_historyEntriesCache;
    bool m_historyCacheValid = false;
    bool m_historyLoadInProgress = false;
    int m_historyLoadGeneration = 0;
    QString m_historySearchText;
    QString m_vocabularySearchText;
    bool m_historyBatchMode = false;
    QSet<QString> m_selectedHistoryFiles;

    void configureScrollableHistoryTabs(QTabWidget *tabs)
    {
        if (!tabs || !tabs->tabBar()) {
            return;
        }
        tabs->setTabPosition(QTabWidget::North);
        tabs->setElideMode(Qt::ElideNone);
        tabs->tabBar()->setExpanding(false);
        tabs->tabBar()->setUsesScrollButtons(true);
        tabs->tabBar()->setElideMode(Qt::ElideNone);
        tabs->tabBar()->installEventFilter(new TabBarWheelFilter(tabs->tabBar()));
    }

    void updateHistoryBatchButtons()
    {
        if (m_historyBatchToggleButton) {
            m_historyBatchToggleButton->setText(m_historyBatchMode ? tr8("退出选择") : tr8("选择记录"));
        }
        if (m_historySelectAllButton) {
            m_historySelectAllButton->setVisible(m_historyBatchMode);
        }
        if (m_historyClearSelectionButton) {
            m_historyClearSelectionButton->setVisible(m_historyBatchMode);
            m_historyClearSelectionButton->setEnabled(!m_selectedHistoryFiles.isEmpty());
        }
        if (m_historyBatchDeleteButton) {
            m_historyBatchDeleteButton->setVisible(m_historyBatchMode);
            m_historyBatchDeleteButton->setEnabled(!m_selectedHistoryFiles.isEmpty());
            m_historyBatchDeleteButton->setText(m_selectedHistoryFiles.isEmpty()
                ? tr8("删除选中")
                : tr8("删除选中(") + QString::number(m_selectedHistoryFiles.size()) + tr8(")"));
        }
        if (m_historySelectionLabel) {
            m_historySelectionLabel->setVisible(m_historyBatchMode);
            m_historySelectionLabel->setText(m_selectedHistoryFiles.isEmpty()
                ? tr8("未选择")
                : tr8("已选择 ") + QString::number(m_selectedHistoryFiles.size()) + tr8(" 条"));
        }
    }

    void refreshHistoryViewsFromCache()
    {
        if (!m_historyTabs) {
            return;
        }
        if (!m_historyCacheValid) {
            refreshHistoryTabs();
            return;
        }
        const int previousIndex = qMax(0, m_historyTabs->currentIndex());
        resetHistoryTabLoadedState();
        rebuildHistoryTabPlaceholders(previousIndex, QString());
        populateHistoryTab(qMin(previousIndex, m_historyTabs->count() - 1));
    }

    void deleteSelectedHistoryEntries()
    {
        if (m_selectedHistoryFiles.isEmpty()) {
            showAttentionInformation(this, tr8("没有选中记录"), tr8("请先勾选要删除的历史记录。"));
            return;
        }
        const int count = m_selectedHistoryFiles.size();
        if (QMessageBox::question(
                this,
                tr8("批量删除"),
                tr8("确定删除选中的 ") + QString::number(count) + tr8(" 条历史记录和对应录音吗？")
            ) != QMessageBox::Yes) {
            return;
        }

        bool failed = false;
        const QStringList files = m_selectedHistoryFiles.toList();
        for (const QString &filePath : files) {
            const HistoryEntry entry = historyEntryFromFile(filePath);
            failed = !deleteHistoryEntryFiles(entry) || failed;
        }
        m_selectedHistoryFiles.clear();
        updateHistoryBatchButtons();
        if (failed) {
            showAttentionWarning(this, tr8("删除失败"), tr8("部分文件无法删除，请检查文件是否正在被占用。"));
        }
        refreshHistoryTabs(true);
    }

    void selectCurrentFilteredHistoryEntries()
    {
        const QVector<HistoryEntry> entries = currentFilteredHistoryEntries();
        if (entries.isEmpty()) {
            showAttentionInformation(this, tr8("没有可选择记录"), tr8("当前筛选条件下没有历史记录。"));
            return;
        }
        m_historyBatchMode = true;
        for (const HistoryEntry &entry : entries) {
            if (!entry.filePath.trimmed().isEmpty()) {
                m_selectedHistoryFiles.insert(entry.filePath);
            }
        }
        updateHistoryBatchButtons();
        refreshHistoryViewsFromCache();
    }

    void clearSelectedHistoryEntries()
    {
        m_selectedHistoryFiles.clear();
        updateHistoryBatchButtons();
        refreshHistoryViewsFromCache();
    }

    void applyTestSearch()
    {
        if (!m_testItemsLayout) {
            return;
        }
        const QString keyword = m_testSearchEdit ? m_testSearchEdit->text().trimmed() : QString();
        int visibleCount = 0;
        int faqMatches = 0;
        for (int i = 0; i < m_testItemsLayout->count(); ++i) {
            QLayoutItem *item = m_testItemsLayout->itemAt(i);
            QWidget *widget = item ? item->widget() : nullptr;
            if (!widget || widget == m_testEmptyLabel || widget == m_testFaqMatchButton) {
                continue;
            }
            const QString searchText = widget->property("testSearchText").toString();
            if (searchText.isEmpty()) {
                continue;
            }
            const bool matched = keyword.isEmpty() || searchText.contains(keyword, Qt::CaseInsensitive);
            widget->setVisible(matched);
            if (matched) {
                ++visibleCount;
            }
        }
        if (!keyword.isEmpty() && m_faqItemsLayout) {
            for (int i = 0; i < m_faqItemsLayout->count(); ++i) {
                QLayoutItem *item = m_faqItemsLayout->itemAt(i);
                QWidget *widget = item ? item->widget() : nullptr;
                if (!widget || widget == m_faqEmptyLabel) {
                    continue;
                }
                const QString searchText = widget->property("faqSearchText").toString();
                if (!searchText.isEmpty() && searchText.contains(keyword, Qt::CaseInsensitive)) {
                    ++faqMatches;
                }
            }
        }
        if (m_testFaqMatchButton) {
            m_testFaqMatchButton->setVisible(faqMatches > 0);
            m_testFaqMatchButton->setText(tr8("常见问题找到 ") + QString::number(faqMatches) + tr8(" 条匹配，点击查看"));
        }
        if (m_testEmptyLabel) {
            m_testEmptyLabel->setVisible(!keyword.isEmpty() && visibleCount == 0 && faqMatches == 0);
        }
    }

    void applyFaqSearch()
    {
        if (!m_faqItemsLayout) {
            return;
        }
        const QString keyword = m_faqSearchEdit ? m_faqSearchEdit->text().trimmed() : QString();
        int visibleCount = 0;
        for (int i = 0; i < m_faqItemsLayout->count(); ++i) {
            QLayoutItem *item = m_faqItemsLayout->itemAt(i);
            QWidget *widget = item ? item->widget() : nullptr;
            if (!widget || widget == m_faqEmptyLabel) {
                continue;
            }
            const QString searchText = widget->property("faqSearchText").toString();
            if (searchText.isEmpty()) {
                continue;
            }
            const bool matched = keyword.isEmpty() || searchText.contains(keyword, Qt::CaseInsensitive);
            widget->setVisible(matched);
            if (matched) {
                ++visibleCount;
            }
        }
        if (m_faqEmptyLabel) {
            m_faqEmptyLabel->setVisible(visibleCount == 0);
        }
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (m_settings && m_settings->trayResident()) {
            hide();
            event->ignore();
        } else {
            event->accept();
            qApp->quit();
        }
    }

private:

    QWidget *sidebar()
    {
        auto *panel = new QFrame;
        panel->setObjectName(QStringLiteral("sidebar"));
        panel->setFixedWidth(240);
        panel->setStyleSheet(QStringLiteral(
            "QFrame#sidebar { background: #111827; border: none; }"
            "QLabel { color: #f9fafb; }"
        ));

        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(12);

        auto *brand = new QLabel(tr8("语音助手"));
        brand->setFont(appFont(18, QFont::DemiBold));
        brand->setStyleSheet(QStringLiteral("color: #ffffff; background: transparent;"));
        layout->addWidget(brand);

        auto *caption = new QLabel(tr8("后台语音输入助手"));
        caption->setStyleSheet(QStringLiteral("color: #aeb7c5; background: transparent;"));
        layout->addWidget(caption);
        layout->addSpacing(18);

        layout->addWidget(navButton(QStringLiteral("home"), tr8("主页"), true));
        layout->addWidget(navButton(QStringLiteral("history"), tr8("历史记录"), false));
        layout->addWidget(navButton(QStringLiteral("vocabulary"), tr8("词库"), false));
        layout->addWidget(navButton(QStringLiteral("prompts"), tr8("提示词"), false));
        layout->addWidget(navButton(QStringLiteral("custom"), tr8("功能自定义"), false));
        layout->addWidget(navButton(QStringLiteral("diagnostics"), tr8("测试工具"), false));
        layout->addWidget(navButton(QStringLiteral("settings"), tr8("设置"), false));
        layout->addWidget(navButton(QStringLiteral("faq"), tr8("常见问题"), false));
        layout->addStretch();
        return panel;
    }

    QWidget *pagesWidget()
    {
        m_pages = new QStackedWidget;
        m_pages->addWidget(content());
        m_pages->addWidget(historyPage());
        m_pages->addWidget(vocabularyPage());
        m_pages->addWidget(promptsPage());
        m_pages->addWidget(customFunctionsPage());
        m_pages->addWidget(diagnosticsPage());
        m_pages->addWidget(settingsPage());
        m_pages->addWidget(faqPage());
        return m_pages;
    }

    QWidget *content()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(18);

        auto *top = new QHBoxLayout;
        auto *titleBox = new QVBoxLayout;
        auto *title = new QLabel(tr8("语音助手主界面"));
        title->setFont(appFont(24, QFont::DemiBold));
        titleBox->addWidget(title);

        top->addLayout(titleBox, 1);
        layout->addLayout(top);

        auto *modeScroll = new QScrollArea;
        modeScroll->setWidgetResizable(true);
        modeScroll->setFrameShape(QFrame::NoFrame);
        modeScroll->setMinimumHeight(320);
        modeScroll->setMaximumHeight(500);
        modeScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        modeScroll->setFocusPolicy(Qt::WheelFocus);
        modeScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        modeScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        modeScroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background: transparent; }"
        ));

        auto *modeHolder = new QWidget;
        m_modeGridLayout = new QGridLayout(modeHolder);
        m_modeGridLayout->setHorizontalSpacing(14);
        m_modeGridLayout->setVerticalSpacing(14);
        m_modeGridLayout->setContentsMargins(0, 0, 0, 0);
        populateModeGrid();
        modeScroll->setWidget(modeHolder);
        layout->addWidget(modeScroll, 1);

        auto *middle = new QHBoxLayout;
        middle->setSpacing(14);
        middle->addWidget(historyPanel(), 2);
        middle->addWidget(statusPanel(), 1);
        layout->addLayout(middle, 2);

        return page;
    }

    QPushButton *navButton(const QString &id, const QString &text, bool active)
    {
        auto *button = new QPushButton(text);
        button->setFixedHeight(38);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("navId", id);
        button->setStyleSheet(navButtonStyle(active));
        connect(button, &QPushButton::clicked, this, [this, id]() {
            selectPage(id);
        });
        m_navButtons.insert(id, button);
        return button;
    }

    QString navButtonStyle(bool active) const
    {
        return QStringLiteral(
            "QPushButton {"
            "  text-align: left;"
            "  background: %1;"
            "  color: %2;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding-left: 12px;"
            "}"
            "QPushButton:hover { background: #263244; color: #ffffff; }"
        ).arg(active ? QStringLiteral("#263244") : QStringLiteral("transparent"),
              active ? QStringLiteral("#ffffff") : QStringLiteral("#aeb7c5"));
    }

    void selectPage(const QString &id)
    {
        if (!m_pages) {
            return;
        }

        const QStringList order = QStringList()
            << QStringLiteral("home")
            << QStringLiteral("history")
            << QStringLiteral("vocabulary")
            << QStringLiteral("prompts")
            << QStringLiteral("custom")
            << QStringLiteral("diagnostics")
            << QStringLiteral("settings")
            << QStringLiteral("faq");
        const int index = order.indexOf(id);
        if (index < 0) {
            return;
        }

        const bool pageChanged = m_pages->currentIndex() != index;
        m_pages->setCurrentIndex(index);
        for (const QString &key : m_navButtons.keys()) {
            m_navButtons.value(key)->setStyleSheet(navButtonStyle(key == id));
        }

        if (id == QStringLiteral("history")) {
            QTimer::singleShot(0, this, [this]() {
                refreshHistoryTabs();
            });
        } else if (id == QStringLiteral("vocabulary")) {
            refreshVocabularyTabs();
        } else if (id == QStringLiteral("prompts")) {
            refreshPromptSelector();
        } else if (id == QStringLiteral("custom")) {
            refreshCustomFunctionsPage();
        } else if (id == QStringLiteral("settings") && pageChanged && m_settingsPanel) {
            m_settingsPanel->refreshFromSettings();
        }
    }

    QWidget *historyPage()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(16);

        auto *top = new QHBoxLayout;
        auto *titleBox = new QVBoxLayout;
        auto *title = new QLabel(tr8("历史记录"));
        title->setFont(appFont(24, QFont::DemiBold));
        titleBox->addWidget(title);

        auto *refresh = new QPushButton(tr8("刷新"));
        refresh->setFixedHeight(38);
        refresh->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(refresh, &QPushButton::clicked, this, [this]() { refreshHistoryTabs(true); });

        auto *openFolder = new QPushButton(tr8("打开目录"));
        openFolder->setFixedHeight(38);
        openFolder->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        openFolder->setMenu(recordDirectoryOpenMenu(openFolder, this, [this]() {
            return m_settings->recordDirectoryPath();
        }));

        auto *backup = new QPushButton(tr8("备份"));
        backup->setMinimumSize(72, 38);
        backup->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(backup, &QPushButton::clicked, this, [this]() {
            backupHistoryRecords();
        });

        auto *importButton = new QPushButton(tr8("导入"));
        importButton->setMinimumSize(72, 38);
        importButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(importButton, &QPushButton::clicked, this, [this]() {
            importHistoryRecords();
        });

        auto *exportButton = new QPushButton(tr8("导出"));
        exportButton->setMinimumSize(72, 38);
        exportButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *exportMenu = new QMenu(exportButton);
        exportMenu->addAction(tr8("文本导出"), this, [this]() { exportHistoryText(); });
        exportMenu->addAction(tr8("录音导出"), this, [this]() { exportHistoryAudio(); });
        exportMenu->addAction(tr8("详细记录导出"), this, [this]() { exportHistoryDetails(); });
        exportMenu->addAction(tr8("全部导出"), this, [this]() { exportHistoryAll(); });
        exportButton->setMenu(exportMenu);

        top->addLayout(titleBox, 1);
        top->addWidget(refresh);
        top->addWidget(openFolder);
        top->addWidget(backup);
        top->addWidget(importButton);
        top->addWidget(exportButton);
        layout->addLayout(top);

        auto *tools = new QHBoxLayout;
        tools->setSpacing(10);
        m_historySearchEdit = new QLineEdit;
        m_historySearchEdit->setMinimumHeight(40);
        m_historySearchEdit->setPlaceholderText(tr8("搜索历史记录、识别文本、输出结果或错误信息"));
        m_historySearchEdit->setClearButtonEnabled(true);
        m_historySearchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 8px;"
            "  padding: 0 12px;"
            "  color: #111827;"
            "}"
        ));
        connect(m_historySearchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
            m_historySearchText = text;
            refreshHistoryViewsFromCache();
        });

        m_historyBatchToggleButton = new QPushButton(tr8("选择记录"));
        m_historyBatchToggleButton->setFont(appFont(10, QFont::DemiBold));
        m_historyBatchToggleButton->setMinimumSize(112, 42);
        m_historyBatchToggleButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_historyBatchToggleButton->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(m_historyBatchToggleButton, &QPushButton::clicked, this, [this]() {
            m_historyBatchMode = !m_historyBatchMode;
            if (!m_historyBatchMode) {
                m_selectedHistoryFiles.clear();
            }
            updateHistoryBatchButtons();
            refreshHistoryViewsFromCache();
        });

        m_historySelectAllButton = new QPushButton(tr8("全选当前"));
        m_historySelectAllButton->setFont(appFont(10, QFont::DemiBold));
        m_historySelectAllButton->setMinimumSize(112, 42);
        m_historySelectAllButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_historySelectAllButton->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(m_historySelectAllButton, &QPushButton::clicked, this, [this]() {
            selectCurrentFilteredHistoryEntries();
        });

        m_historyClearSelectionButton = new QPushButton(tr8("清空选择"));
        m_historyClearSelectionButton->setFont(appFont(10, QFont::DemiBold));
        m_historyClearSelectionButton->setMinimumSize(112, 42);
        m_historyClearSelectionButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_historyClearSelectionButton->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(m_historyClearSelectionButton, &QPushButton::clicked, this, [this]() {
            clearSelectedHistoryEntries();
        });

        m_historyBatchDeleteButton = new QPushButton(tr8("删除选中"));
        m_historyBatchDeleteButton->setFont(appFont(10, QFont::DemiBold));
        m_historyBatchDeleteButton->setMinimumSize(126, 42);
        m_historyBatchDeleteButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_historyBatchDeleteButton->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));
        connect(m_historyBatchDeleteButton, &QPushButton::clicked, this, [this]() {
            deleteSelectedHistoryEntries();
        });

        m_historySelectionLabel = new QLabel(tr8("未选择"));
        m_historySelectionLabel->setMinimumHeight(42);
        m_historySelectionLabel->setMinimumWidth(92);
        m_historySelectionLabel->setAlignment(Qt::AlignCenter);
        m_historySelectionLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #007f5f; font-weight: 600; padding: 0 6px; }"
        ));
        tools->addWidget(m_historySearchEdit, 1);
        tools->addWidget(m_historyBatchToggleButton);
        tools->addWidget(m_historySelectAllButton);
        tools->addWidget(m_historyClearSelectionButton);
        tools->addWidget(m_historyBatchDeleteButton);
        tools->addWidget(m_historySelectionLabel);
        layout->addLayout(tools);
        updateHistoryBatchButtons();

        m_historyTabs = new QTabWidget;
        configureScrollableHistoryTabs(m_historyTabs);
        m_historyTabs->setStyleSheet(QStringLiteral(
            "QTabWidget::pane { border: 1px solid #dde2ea; background: #ffffff; border-radius: 8px; }"
            "QTabBar::tab { padding: 9px 16px; color: #667085; }"
            "QTabBar::tab:selected { color: #111827; font-weight: 600; }"
            "QTabBar QToolButton { width: 28px; background: #ffffff; border: 1px solid #d0d5dd; color: #111827; }"
            "QTabBar QToolButton:hover { background: #eef2ff; }"
        ));
        layout->addWidget(m_historyTabs, 1);
        refreshHistoryTabs();
        return page;
    }

    QWidget *vocabularyPage()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(16);

        auto *top = new QHBoxLayout;
        auto *titleBox = new QVBoxLayout;
        auto *title = new QLabel(tr8("词库"));
        title->setFont(appFont(24, QFont::DemiBold));
        titleBox->addWidget(title);

        auto *add = new QPushButton(tr8("新增词条"));
        add->setMinimumSize(96, 38);
        add->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(add, &QPushButton::clicked, this, [this]() {
            showVocabularyEntryDialog();
        });

        auto *importButton = new QPushButton(tr8("导入"));
        importButton->setMinimumSize(72, 38);
        importButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(importButton, &QPushButton::clicked, this, [this]() {
            showAttentionInformation(this, tr8("词库导入"), tr8("导入逻辑下一步接入；这里会支持从文本、CSV 或 JSON 导入词条。"));
        });

        auto *exportButton = new QPushButton(tr8("导出"));
        exportButton->setMinimumSize(72, 38);
        exportButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(exportButton, &QPushButton::clicked, this, [this]() {
            showAttentionInformation(this, tr8("词库导出"), tr8("导出逻辑下一步接入；这里会支持导出当前筛选词条或全部词条。"));
        });

        auto *openFolder = new QPushButton(tr8("打开目录"));
        openFolder->setMinimumSize(92, 38);
        openFolder->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(openFolder, &QPushButton::clicked, this, []() {
            const QString path = QDir(appBasePath()).filePath(QStringLiteral("config/lexicon"));
            QDir().mkpath(path);
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });

        top->addLayout(titleBox, 1);
        top->addWidget(add);
        top->addWidget(importButton);
        top->addWidget(exportButton);
        top->addWidget(openFolder);
        layout->addLayout(top);

        auto *tools = new QHBoxLayout;
        tools->setSpacing(10);
        m_vocabularySearchEdit = new QLineEdit;
        m_vocabularySearchEdit->setMinimumHeight(40);
        m_vocabularySearchEdit->setPlaceholderText(tr8("搜索原词、标准词、别名、备注或作用范围"));
        m_vocabularySearchEdit->setClearButtonEnabled(true);
        m_vocabularySearchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 8px;"
            "  padding: 0 12px;"
            "  color: #111827;"
            "}"
        ));
        connect(m_vocabularySearchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
            m_vocabularySearchText = text;
            refreshVocabularyTabs();
        });

        tools->addWidget(m_vocabularySearchEdit, 1);
        layout->addLayout(tools);

        m_vocabularyTabs = new QTabWidget;
        configureScrollableHistoryTabs(m_vocabularyTabs);
        m_vocabularyTabs->setStyleSheet(QStringLiteral(
            "QTabWidget::pane { border: 1px solid #dde2ea; background: #ffffff; border-radius: 8px; }"
            "QTabBar::tab { padding: 9px 16px; color: #667085; }"
            "QTabBar::tab:selected { color: #111827; font-weight: 600; }"
            "QTabBar QToolButton { width: 28px; background: #ffffff; border: 1px solid #d0d5dd; color: #111827; }"
            "QTabBar QToolButton:hover { background: #eef2ff; }"
        ));
        layout->addWidget(m_vocabularyTabs, 1);
        refreshVocabularyTabs();
        return page;
    }

    QVector<HistoryTabDef> vocabularyTabDefs() const
    {
        QVector<HistoryTabDef> tabs;
        tabs.append({QStringLiteral("__all"), tr8("全部")});
        tabs.append({QStringLiteral("__global"), tr8("全局")});
        tabs.append({QStringLiteral("dictate"), tr8("听写")});
        tabs.append({QStringLiteral("translate"), tr8("翻译")});
        tabs.append({QStringLiteral("ask"), tr8("问答")});
        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            const QString title = fn.name.trimmed().isEmpty() ? tr8("自定义") : fn.name.trimmed();
            tabs.append({fn.id, title});
        }
        return tabs;
    }

    void refreshVocabularyTabs()
    {
        if (!m_vocabularyTabs) {
            return;
        }
        const int previousIndex = qMax(0, m_vocabularyTabs->currentIndex());
        while (m_vocabularyTabs->count() > 0) {
            QWidget *page = m_vocabularyTabs->widget(0);
            m_vocabularyTabs->removeTab(0);
            if (page) {
                page->deleteLater();
            }
        }
        const QVector<HistoryTabDef> tabs = vocabularyTabDefs();
        for (const HistoryTabDef &tab : tabs) {
            m_vocabularyTabs->addTab(vocabularyTabContent(tab.id, tab.title), tab.title);
        }
        if (m_vocabularyTabs->count() > 0) {
            m_vocabularyTabs->setCurrentIndex(qMin(previousIndex, m_vocabularyTabs->count() - 1));
        }
    }

    QWidget *vocabularyTabContent(const QString &scopeId, const QString &scopeTitle)
    {
        Q_UNUSED(scopeId);
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(10);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        auto *holder = new QWidget;
        auto *items = new QVBoxLayout(holder);
        items->setContentsMargins(0, 0, 10, 0);
        items->setSpacing(10);

        items->addWidget(vocabularyEmptyCard(scopeTitle));
        items->addStretch();
        scroll->setWidget(holder);
        layout->addWidget(scroll, 1);
        return page;
    }

    QWidget *vocabularyEmptyCard(const QString &scopeTitle)
    {
        Q_UNUSED(scopeTitle);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(18, 16, 18, 16);
        layout->setSpacing(10);

        auto *title = new QLabel(m_vocabularySearchText.trimmed().isEmpty() ? tr8("暂无词条") : tr8("没有匹配词条"));
        title->setFont(appFont(13, QFont::DemiBold));

        auto *structure = new QLabel(tr8("词条将包含：原词/错词、标准写法、别名、作用范围、匹配方式、备注、启用状态。"));
        structure->setWordWrap(true);
        structure->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));

        layout->addWidget(title);
        layout->addWidget(structure);
        return frame;
    }

    void showVocabularyEntryDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(tr8("新增词条"));
        dialog.setMinimumSize(640, 520);
        dialog.setFont(appFont());
        dialog.setStyleSheet(QStringLiteral("QDialog { background: #f6f7f9; } QLabel { color: #111827; }"));

        auto *root = new QVBoxLayout(&dialog);
        root->setContentsMargins(22, 20, 22, 18);
        root->setSpacing(14);

        auto *title = new QLabel(tr8("新增词条"));
        title->setFont(appFont(18, QFont::DemiBold));
        root->addWidget(title);

        auto *formCard = new QFrame;
        formCard->setObjectName(QStringLiteral("card"));
        formCard->setStyleSheet(cardStyle());
        auto *form = new QGridLayout(formCard);
        form->setContentsMargins(16, 14, 16, 14);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(12);

        auto *source = new QLineEdit;
        source->setPlaceholderText(tr8("例如：deepseep、项目简称、容易识别错的词"));
        auto *target = new QLineEdit;
        target->setPlaceholderText(tr8("例如：DeepSeek、正式项目名、固定译名"));
        auto *aliases = new QLineEdit;
        aliases->setPlaceholderText(tr8("可选，多个别名用逗号分隔"));
        auto *scope = new QComboBox;
        scope->addItem(tr8("全局"), QStringLiteral("__global"));
        scope->addItem(tr8("听写"), QStringLiteral("dictate"));
        scope->addItem(tr8("翻译"), QStringLiteral("translate"));
        scope->addItem(tr8("问答"), QStringLiteral("ask"));
        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            scope->addItem(fn.name, fn.id);
        }
        auto *match = new QComboBox;
        match->addItem(tr8("精确匹配"));
        match->addItem(tr8("忽略大小写"));
        match->addItem(tr8("包含匹配"));
        match->addItem(tr8("正则匹配"));
        auto *enabled = new QCheckBox(tr8("启用"));
        enabled->setChecked(true);
        enabled->setFont(appFont(10, QFont::DemiBold));
        auto *note = new QTextEdit;
        note->setPlaceholderText(tr8("可选，记录这个词条适用场景或使用注意事项"));
        note->setMinimumHeight(100);

        form->addWidget(new QLabel(tr8("原词 / 错词")), 0, 0);
        form->addWidget(source, 0, 1);
        form->addWidget(new QLabel(tr8("标准写法")), 1, 0);
        form->addWidget(target, 1, 1);
        form->addWidget(new QLabel(tr8("别名")), 2, 0);
        form->addWidget(aliases, 2, 1);
        form->addWidget(new QLabel(tr8("作用范围")), 3, 0);
        form->addWidget(scope, 3, 1);
        form->addWidget(new QLabel(tr8("匹配方式")), 4, 0);
        form->addWidget(match, 4, 1);
        form->addWidget(new QLabel(tr8("状态")), 5, 0);
        form->addWidget(enabled, 5, 1);
        form->addWidget(new QLabel(tr8("备注")), 6, 0, Qt::AlignTop);
        form->addWidget(note, 6, 1);
        form->setColumnStretch(1, 1);
        root->addWidget(formCard, 1);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *cancel = new QPushButton(tr8("取消"));
        cancel->setFixedHeight(38);
        cancel->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *save = new QPushButton(tr8("保存词条"));
        save->setFixedHeight(38);
        save->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        buttons->addWidget(cancel);
        buttons->addWidget(save);
        root->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(save, &QPushButton::clicked, &dialog, [this, &dialog]() {
            showAttentionInformation(this, tr8("词库 UI 已完成"), tr8("保存词条的数据结构和生效逻辑将在下一步接入。"));
            dialog.accept();
        });

        dialog.exec();
    }

    QWidget *promptsPage()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(14);

        auto *title = new QLabel(tr8("提示词"));
        title->setFont(appFont(24, QFont::DemiBold));
        layout->addWidget(title);

        auto *tools = new QHBoxLayout;
        m_promptLock = new QCheckBox(tr8("锁定提示词"));
        m_promptLock->setChecked(m_settings->promptLocked());
        m_promptLock->setFont(appFont(10, QFont::DemiBold));
        connect(m_promptLock, &QCheckBox::toggled, this, [this](bool locked) {
            m_settings->setPromptLocked(locked);
            m_settings->save();
            updatePromptEditorLock();
            if (m_onSettingsChanged) {
                m_onSettingsChanged();
            }
        });

        m_promptSelector = new QComboBox;
        m_promptSelector->setMinimumHeight(38);
        m_promptSelector->setMinimumWidth(320);
        m_promptSelector->setStyleSheet(QStringLiteral(
            "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
        ));
        connect(m_promptSelector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]() {
            loadPromptEditor();
        });

        tools->addWidget(m_promptSelector);
        tools->addWidget(m_promptLock);
        tools->addStretch();
        layout->addLayout(tools);

        m_promptEditor = new QTextEdit;
        m_promptEditor->setStyleSheet(QStringLiteral(
            "QTextEdit { background: #ffffff; border: 1px solid #dde2ea; border-radius: 8px; padding: 12px; }"
        ));
        layout->addWidget(m_promptEditor, 1);

        auto *buttons = new QHBoxLayout;
        m_promptSaveButton = new QPushButton(tr8("保存提示词"));
        m_promptSaveButton->setFixedHeight(38);
        m_promptSaveButton->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(m_promptSaveButton, &QPushButton::clicked, this, [this]() { savePromptFromEditor(); });

        auto *reload = new QPushButton(tr8("重新读取"));
        reload->setFixedHeight(38);
        reload->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(reload, &QPushButton::clicked, this, [this]() { loadPromptEditor(); });

        auto *openFolder = new QPushButton(tr8("打开提示词目录"));
        openFolder->setFixedHeight(38);
        openFolder->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(openFolder, &QPushButton::clicked, this, []() {
            const QString path = QDir(appBasePath()).filePath(QStringLiteral("prompts"));
            QDir().mkpath(path);
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });

        buttons->addWidget(m_promptSaveButton);
        buttons->addWidget(reload);
        buttons->addWidget(openFolder);
        buttons->addStretch();
        layout->addLayout(buttons);

        refreshPromptSelector();
        return page;
    }

    QWidget *customFunctionsPage()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(16);

        auto *top = new QHBoxLayout;
        auto *titleBox = new QVBoxLayout;
        auto *title = new QLabel(tr8("功能自定义"));
        title->setFont(appFont(24, QFont::DemiBold));
        titleBox->addWidget(title);

        auto *add = new QPushButton(tr8("新增功能"));
        add->setFont(appFont(10, QFont::DemiBold));
        add->setFixedSize(112, 42);
        add->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));
        connect(add, &QPushButton::clicked, this, [this]() { addHubCustomFunction(); });

        top->addLayout(titleBox, 1);
        top->addWidget(add);
        layout->addLayout(top);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *holder = new QWidget;
        m_hubCustomListLayout = new QVBoxLayout(holder);
        m_hubCustomListLayout->setContentsMargins(0, 0, 0, 0);
        m_hubCustomListLayout->setSpacing(12);
        scroll->setWidget(holder);
        layout->addWidget(scroll, 1);

        refreshCustomFunctionsPage();
        return page;
    }

    QWidget *diagnosticsPage()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(14);

        auto *title = new QLabel(tr8("测试工具"));
        title->setFont(appFont(24, QFont::DemiBold));
        layout->addWidget(title);

        m_testSearchEdit = new QLineEdit;
        m_testSearchEdit->setMinimumHeight(42);
        m_testSearchEdit->setPlaceholderText(tr8("搜索测试项目或常见问题关键词"));
        m_testSearchEdit->setClearButtonEnabled(true);
        m_testSearchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 8px;"
            "  padding: 0 12px;"
            "  color: #111827;"
            "}"
        ));
        connect(m_testSearchEdit, &QLineEdit::textChanged, this, [this]() {
            applyTestSearch();
        });
        layout->addWidget(m_testSearchEdit);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background: transparent; }"
        ));

        auto *holder = new QWidget;
        auto *items = new QVBoxLayout(holder);
        m_testItemsLayout = items;
        items->setContentsMargins(0, 0, 10, 0);
        items->setSpacing(12);

        m_testFaqMatchButton = new QPushButton;
        m_testFaqMatchButton->setMinimumHeight(42);
        m_testFaqMatchButton->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        m_testFaqMatchButton->setVisible(false);
        connect(m_testFaqMatchButton, &QPushButton::clicked, this, [this]() {
            const QString keyword = m_testSearchEdit ? m_testSearchEdit->text().trimmed() : QString();
            selectPage(QStringLiteral("faq"));
            if (m_faqSearchEdit) {
                m_faqSearchEdit->setText(keyword);
            }
            applyFaqSearch();
        });
        items->addWidget(m_testFaqMatchButton);

        items->addWidget(diagnosticActionCard(
            tr8("接口自检"),
            tr8("测试已填写的百度、讯飞、DeepSeek、OpenAI 和 Claude 接口。未填写的接口会跳过。"),
            tr8("开始接口自检"),
            &m_interfaceCheckButton,
            &m_interfaceCheckResult,
            [this]() { runInterfaceSelfCheck(); }
        ));

        items->addWidget(diagnosticActionCard(
            tr8("网络诊断"),
            tr8("检查软件当前网络模式、系统代理、环境代理、DNS 解析和核心接口域名连通性。"),
            tr8("开始网络诊断"),
            &m_networkDiagnosticButton,
            &m_networkDiagnosticResult,
            [this]() { runNetworkDiagnostics(); }
        ));

        items->addWidget(diagnosticActionCard(
            tr8("麦克风测试"),
            tr8("录制约 3 秒钟测试音频，判断默认麦克风是否能录到声音。测试文件会自动清理。"),
            tr8("开始麦克风测试"),
            &m_microphoneTestButton,
            &m_microphoneTestResult,
            [this]() { runMicrophoneTest(); }
        ));

        items->addWidget(diagnosticActionCard(
            tr8("浮动条测试"),
            tr8("显示一次真实浮动条，用来确认浮动条没有被关闭、遮挡或移动到屏幕外。"),
            tr8("显示浮动条"),
            nullptr,
            nullptr,
            [this]() { runFloatingBarTest(); }
        ));

        items->addWidget(diagnosticActionCard(
            tr8("结果小框测试"),
            tr8("弹出一次真实结果小框，用来确认复制、写入、替换选中和关闭按钮的显示效果。"),
            tr8("显示结果小框"),
            nullptr,
            nullptr,
            [this]() { runResultPopupTest(); }
        ));

        m_testEmptyLabel = new QLabel(tr8("没有找到匹配的测试项目或常见问题。"));
        m_testEmptyLabel->setWordWrap(true);
        m_testEmptyLabel->setAlignment(Qt::AlignCenter);
        m_testEmptyLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background: #f2f4f7;"
            "  color: #667085;"
            "  border-radius: 8px;"
            "  padding: 16px;"
            "}"
        ));
        m_testEmptyLabel->setVisible(false);
        items->addWidget(m_testEmptyLabel);

        items->addStretch();
        scroll->setWidget(holder);
        layout->addWidget(scroll, 1);
        applyTestSearch();
        return page;
    }

    QWidget *diagnosticActionCard(
        const QString &title,
        const QString &hint,
        const QString &buttonText,
        QPushButton **buttonOut,
        QLabel **resultOut,
        const std::function<void()> &onClick
    )
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setProperty("testSearchText", (QStringList() << title << hint << buttonText).join(QStringLiteral("\n")));
        frame->setStyleSheet(cardStyle());

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);

        auto *top = new QHBoxLayout;
        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(13, QFont::DemiBold));
        labels->addWidget(name);

        auto *button = new QPushButton(buttonText);
        button->setFixedHeight(36);
        button->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(button, &QPushButton::clicked, this, [onClick]() {
            if (onClick) {
                onClick();
            }
        });
        if (buttonOut) {
            *buttonOut = button;
        }

        top->addLayout(labels, 1);
        top->addWidget(button);
        layout->addLayout(top);

        if (resultOut) {
            auto *result = new QLabel(tr8("尚未测试。"));
            result->setWordWrap(true);
            result->setTextInteractionFlags(Qt::TextSelectableByMouse);
            result->setStyleSheet(QStringLiteral(
                "QLabel {"
                "  background: #f2f4f7;"
                "  color: #344054;"
                "  border-radius: 6px;"
                "  padding: 10px;"
                "}"
            ));
            *resultOut = result;
            layout->addWidget(result);
        }

        return frame;
    }

    void runInterfaceSelfCheck()
    {
        if (!m_interfaceCheckResult || !m_interfaceCheckButton) {
            return;
        }
        m_interfaceCheckButton->setEnabled(false);
        m_interfaceCheckResult->setText(tr8("正在自检接口，请稍等..."));

        const bool useSystemProxy = m_settings->useSystemProxy();
        auto *watcher = new QFutureWatcher<QStringList>(this);
        connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher]() {
            const QStringList lines = watcher->result();
            if (m_interfaceCheckResult) {
                m_interfaceCheckResult->setText(lines.join(QStringLiteral("\n\n")));
            }
            if (m_interfaceCheckButton) {
                m_interfaceCheckButton->setEnabled(true);
            }
            watcher->deleteLater();
        });

        watcher->setFuture(QtConcurrent::run([useSystemProxy]() {
            QStringList lines;
            ApiClient api;
            api.setUseSystemProxy(useSystemProxy);
            api.reloadSecrets();

            QString error;
            if (api.hasBaidu()) {
                error.clear();
                const QString result = api.testBaiduCredential(&error);
                lines << diagnosticStatusLine(tr8("百度语音识别"), result.isEmpty() ? tr8("失败") : tr8("通过"), result.isEmpty() ? compactDiagnosticError(error) : result);
            } else {
                lines << diagnosticStatusLine(tr8("百度语音识别"), tr8("未填写，跳过"));
            }

            if (api.hasXfyun()) {
                error.clear();
                const QString result = api.testXfyunCredential(&error);
                lines << diagnosticStatusLine(tr8("讯飞语音听写"), result.isEmpty() ? tr8("失败") : tr8("通过"), result.isEmpty() ? compactDiagnosticError(error) : result);
            } else {
                lines << diagnosticStatusLine(tr8("讯飞语音听写"), tr8("未填写，跳过"));
            }

            if (api.hasDeepSeek()) {
                error.clear();
                const QString result = api.testModelProvider(QStringLiteral("deepseek"), &error);
                lines << diagnosticStatusLine(tr8("DeepSeek"), result.isEmpty() ? tr8("失败") : tr8("通过"), result.isEmpty() ? compactDiagnosticError(error) : result);
            } else {
                lines << diagnosticStatusLine(tr8("DeepSeek"), tr8("未填写，跳过"));
            }

            if (api.hasOpenAI()) {
                error.clear();
                const QString result = api.testModelProvider(QStringLiteral("openai"), &error);
                lines << diagnosticStatusLine(tr8("OpenAI"), result.isEmpty() ? tr8("失败") : tr8("通过"), result.isEmpty() ? compactDiagnosticError(error) : result);
            } else {
                lines << diagnosticStatusLine(tr8("OpenAI"), tr8("未填写，跳过"));
            }

            if (api.hasAnthropic()) {
                error.clear();
                const QString result = api.testModelProvider(QStringLiteral("claude"), &error);
                lines << diagnosticStatusLine(tr8("Claude"), result.isEmpty() ? tr8("失败") : tr8("通过"), result.isEmpty() ? compactDiagnosticError(error) : result);
            } else {
                lines << diagnosticStatusLine(tr8("Claude"), tr8("未填写，跳过"));
            }

            return lines;
        }));
    }

    void runNetworkDiagnostics()
    {
        if (!m_networkDiagnosticResult || !m_networkDiagnosticButton) {
            return;
        }
        m_networkDiagnosticButton->setEnabled(false);
        m_networkDiagnosticResult->setText(tr8("正在诊断网络，请稍等..."));

        const bool useSystemProxy = m_settings->useSystemProxy();
        auto *watcher = new QFutureWatcher<QStringList>(this);
        connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher]() {
            const QStringList lines = watcher->result();
            if (m_networkDiagnosticResult) {
                m_networkDiagnosticResult->setText(lines.join(QStringLiteral("\n\n")));
            }
            if (m_networkDiagnosticButton) {
                m_networkDiagnosticButton->setEnabled(true);
            }
            watcher->deleteLater();
        });

        watcher->setFuture(QtConcurrent::run([useSystemProxy]() {
            QStringList lines;
            lines << diagnosticStatusLine(tr8("软件网络模式"), useSystemProxy ? tr8("使用系统代理") : tr8("直连"));

            const QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl(QStringLiteral("https://api.deepseek.com"))));
            QStringList proxyDetails;
            for (const QNetworkProxy &proxy : proxies) {
                if (proxy.type() != QNetworkProxy::NoProxy && proxy.type() != QNetworkProxy::DefaultProxy) {
                    proxyDetails << proxy.hostName() + QStringLiteral(":") + QString::number(proxy.port());
                }
            }
            lines << diagnosticStatusLine(
                tr8("Windows 系统代理"),
                proxyDetails.isEmpty() ? tr8("未检测到明确代理") : tr8("已检测到代理"),
                proxyDetails.join(QStringLiteral("，"))
            );

            QStringList envProxies;
            const QList<QByteArray> envNames = QList<QByteArray>() << "HTTP_PROXY" << "HTTPS_PROXY" << "ALL_PROXY";
            for (const QByteArray &name : envNames) {
                const QByteArray value = qgetenv(name.constData());
                if (!value.trimmed().isEmpty()) {
                    envProxies << QString::fromLatin1(name) + QStringLiteral("=") + QString::fromLocal8Bit(value);
                }
            }
            lines << diagnosticStatusLine(
                tr8("环境变量代理"),
                envProxies.isEmpty() ? tr8("未检测到") : tr8("已检测到"),
                envProxies.join(QStringLiteral("\n  "))
            );

            const QStringList hosts = QStringList()
                << QStringLiteral("api.deepseek.com")
                << QStringLiteral("aip.baidubce.com")
                << QStringLiteral("vop.baidu.com")
                << QStringLiteral("iat-api.xfyun.cn")
                << QStringLiteral("api.openai.com")
                << QStringLiteral("api.anthropic.com");
            for (const QString &host : hosts) {
                const QHostInfo info = QHostInfo::fromName(host);
                lines << diagnosticStatusLine(
                    tr8("DNS ") + host,
                    info.error() == QHostInfo::NoError ? tr8("通过") : tr8("失败"),
                    info.error() == QHostInfo::NoError ? info.addresses().value(0).toString() : info.errorString()
                );
            }

            lines << networkProbeLine(tr8("DeepSeek 域名连通"), QUrl(QStringLiteral("https://api.deepseek.com")), useSystemProxy);
            lines << networkProbeLine(tr8("百度令牌域名连通"), QUrl(QStringLiteral("https://aip.baidubce.com")), useSystemProxy);
            lines << networkProbeLine(tr8("讯飞听写域名连通"), QUrl(QStringLiteral("https://iat-api.xfyun.cn/v2/iat")), useSystemProxy);
            lines << networkProbeLine(tr8("OpenAI 域名连通"), QUrl(QStringLiteral("https://api.openai.com")), useSystemProxy);
            lines << networkProbeLine(tr8("Claude 域名连通"), QUrl(QStringLiteral("https://api.anthropic.com")), useSystemProxy);

            lines << diagnosticStatusLine(
                tr8("TUN / 透明代理提示"),
                tr8("需要人工确认"),
                tr8("软件只能检测系统代理和连通性，无法百分百判断 TUN 或虚拟网卡。若开启 v2rayN TUN、Clash TUN、sing-box TUN，请把百度、讯飞等国内接口域名按需要设置为直连。")
            );
            return lines;
        }));
    }

    void runMicrophoneTest()
    {
        if (!m_microphoneTestResult || !m_microphoneTestButton || m_microphoneTesting) {
            return;
        }

        QString error;
        if (!m_microphoneTestRecorder.start(tr8("麦克风测试"), QDir::tempPath(), &error)) {
            m_microphoneTestResult->setText(diagnosticStatusLine(tr8("麦克风测试"), tr8("失败"), compactDiagnosticError(error)));
            showAttentionWarning(this, tr8("麦克风测试失败"), error.isEmpty() ? tr8("无法启动默认麦克风。") : error);
            return;
        }

        m_microphoneTesting = true;
        m_microphoneTestButton->setEnabled(false);
        m_microphoneTestResult->setText(tr8("正在录音 3 秒钟，请对着麦克风说一句话..."));

        QTimer::singleShot(3000, this, [this]() {
            const QByteArray pcm = m_microphoneTestRecorder.stop();
            QFile::remove(m_microphoneTestRecorder.lastWavPath());
            m_microphoneTesting = false;
            if (m_microphoneTestButton) {
                m_microphoneTestButton->setEnabled(true);
            }
            if (!m_microphoneTestResult) {
                return;
            }

            const int peak = pcm16PeakLevel(pcm);
            const double seconds = pcm.size() / 2.0 / 16000.0;
            QString detail = tr8("录音时长约 %1 秒，峰值音量 %2。").arg(QString::number(seconds, 'f', 1)).arg(peak);
            if (pcm.isEmpty() || peak < 200) {
                m_microphoneTestResult->setText(diagnosticStatusLine(tr8("麦克风测试"), tr8("失败"), detail + tr8("没有检测到有效声音。")));
                showAttentionWarning(this, tr8("麦克风测试失败"), tr8("没有检测到有效声音。请检查默认麦克风、系统录音权限和输入音量。"));
            } else if (peak < 1200) {
                m_microphoneTestResult->setText(diagnosticStatusLine(tr8("麦克风测试"), tr8("声音偏低"), detail + tr8("可以录到声音，但输入音量偏低。")));
            } else {
                m_microphoneTestResult->setText(diagnosticStatusLine(tr8("麦克风测试"), tr8("通过"), detail));
            }
        });
    }

    void runFloatingBarTest()
    {
        if (!m_settings->floatingBarEnabled()) {
            showAttentionInformation(this, tr8("浮动条已关闭"), tr8("请在设置的“常用设置”页勾选“启用浮动条”。"));
            return;
        }
        if (!m_floatingBar) {
            showAttentionWarning(this, tr8("浮动条测试失败"), tr8("没有连接到浮动条对象，请重启软件后再试。"));
            return;
        }
        m_floatingBar->setSuppressed(false);
        m_floatingBar->setStatus(tr8("浮动条测试"), tr8("如果能看到这条提示，浮动条显示正常。"));
        m_floatingBar->hideLater(5000);
    }

    void runResultPopupTest()
    {
        NativeWindowHandle targetWindow = nullptr;
#ifdef Q_OS_WIN
        targetWindow = reinterpret_cast<HWND>(winId());
#endif
        auto *popup = new ResultChoicePopup(
            m_settings,
            tr8("结果小框测试"),
            tr8("这是测试结果。你可以查看复制、写入、替换选中和关闭按钮是否完整显示。测试小框不会调用大模型。"),
            targetWindow,
            false,
            10000
        );
        popup->showNearBottom();
    }

    QWidget *settingsPage()
    {
        m_settingsPanel = new SettingsPanel(m_settings, [this]() {
            applySettingsChanged();
            if (m_onSettingsChanged) {
                m_onSettingsChanged();
            }
        }, this);
        return m_settingsPanel;
    }

    QWidget *faqPage()
    {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(14);

        auto *title = new QLabel(tr8("常见问题"));
        title->setFont(appFont(24, QFont::DemiBold));
        layout->addWidget(title);

        m_faqSearchEdit = new QLineEdit;
        m_faqSearchEdit->setMinimumHeight(42);
        m_faqSearchEdit->setPlaceholderText(tr8("搜索弹窗原文、原因或处理办法"));
        m_faqSearchEdit->setClearButtonEnabled(true);
        m_faqSearchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 8px;"
            "  padding: 0 12px;"
            "  color: #111827;"
            "}"
        ));
        connect(m_faqSearchEdit, &QLineEdit::textChanged, this, [this]() {
            applyFaqSearch();
        });
        layout->addWidget(m_faqSearchEdit);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background: transparent; }"
        ));

        auto *holder = new QWidget;
        auto *items = new QVBoxLayout(holder);
        m_faqItemsLayout = items;
        items->setContentsMargins(0, 0, 10, 0);
        items->setSpacing(12);

        addLatestFeatureFaqItems(items);

        items->addWidget(faqCard(
            tr8("测试工具：接口自检失败"),
            tr8("接口自检会测试已填写的百度、讯飞、DeepSeek、OpenAI 和 Claude。未填写的接口会跳过；失败通常是密钥错误、模型名不可用、网络代理分流错误或接口权限未开通。"),
            QStringList()
                << tr8("先打开“设置 -> 接口”，确认对应接口密钥已经填写并保存。")
                << tr8("再打开“设置 -> 模型”，确认功能选择的模型名称和接口服务匹配。")
                << tr8("如果只有某一个服务失败，优先检查该服务控制台的应用权限、密钥状态和接口域名分流规则。")
        ));

        items->addWidget(faqCard(
            tr8("测试工具：网络诊断显示域名不可达"),
            tr8("网络诊断会检查 DNS 和核心接口域名连通性。域名不可达通常和 DNS、系统代理、TUN、透明代理、防火墙或安全软件有关。"),
            QStringList()
                << tr8("如果 DNS 失败，先更换 DNS 或检查代理软件的 DNS 设置。")
                << tr8("如果连通失败，分别测试“使用系统代理”开启和关闭两种状态。")
                << tr8("如果开启了 TUN 或透明代理，请把百度、讯飞等国内接口域名按需要设置为直连。")
        ));

        items->addWidget(faqCard(
            tr8("麦克风测试失败 / 没有检测到有效声音"),
            tr8("麦克风测试会录制约 3 秒钟。如果弹出“麦克风测试失败”或结果显示没有有效声音，通常是默认输入设备错误、录音权限被拦截、音量太低或设备被其它程序占用。"),
            QStringList()
                << tr8("在 Windows 声音设置里确认默认输入设备正确，并观察输入音量条是否变化。")
                << tr8("检查系统隐私设置和安全软件，允许本程序访问麦克风。")
                << tr8("关闭正在独占麦克风的软件后重新测试。")
        ));

        items->addWidget(faqCard(
            tr8("浮动条测试失败 / 结果小框测试看不到"),
            tr8("浮动条测试依赖常用设置里的“浮动条”开关；结果小框是普通独立小窗，可能被其它全屏程序、置顶窗口或安全软件遮挡。"),
            QStringList()
                << tr8("浮动条测试提示已关闭时，打开“设置 -> 常用设置”，勾选“启用浮动条”。")
                << tr8("如果结果小框看不到，先切回桌面或主界面，避免被全屏窗口遮住。")
                << tr8("如果安全软件拦截弹窗或置前行为，请把 vocekit.exe 加入信任。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：缺少语音识别密钥"),
            tr8("弹窗可能写为：缺少讯飞语音听写密钥；缺少百度语音识别密钥。一般是接口页没有填写对应语音服务的密钥，或者当前语音识别服务选错了。"),
            QStringList()
                << tr8("打开“设置 -> 接口”，先确认“当前语音识别服务”是你要用的百度或讯飞。")
                << tr8("使用讯飞时填写讯飞 AppID、API Key、API Secret；使用百度时填写百度 API Key、Secret Key。")
                << tr8("填写后点击接口页顶部固定位置的“保存接口配置”，再重新触发功能。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：讯飞连接被远端主机关闭"),
            tr8("弹窗可能写为：讯飞语音听写连接被远端主机关闭；讯飞语音听写网络请求失败：远端主机关闭了这个连接。常见原因是 TUN 模式、透明代理或虚拟网卡在底层接管了 WebSocket/TLS 连接。"),
            QStringList()
                << tr8("如果正在使用 v2rayN、Clash、sing-box 等工具，先检查是否开启了 TUN、透明代理、增强模式或虚拟网卡。")
                << tr8("必须开启这些模式时，把 iat-api.xfyun.cn 和 *.xfyun.cn 加入直连规则。")
                << tr8("仍然失败时，在“设置 -> 常用设置”里关闭“使用系统代理”，并重启软件后再试。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：讯飞语音听写网络请求超时"),
            tr8("讯飞 WebSocket 在限定时间内没有完成连接、鉴权或返回识别结果。网络代理、DNS、TUN 分流和本机防火墙都可能导致超时。"),
            QStringList()
                << tr8("优先关闭系统代理测试一次；如果必须走代理，把讯飞域名设置为直连。")
                << tr8("检查电脑网络、防火墙和安全软件是否拦截 vocekit.exe。")
                << tr8("确认讯飞控制台的应用处于启用状态，并且接口权限已经开通。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：百度令牌获取失败 / 百度识别失败"),
            tr8("百度语音识别需要先用 API Key 和 Secret Key 换取访问令牌，再上传录音。密钥错误、接口未开通、网络失败或录音格式异常都会触发这类弹窗。"),
            QStringList()
                << tr8("打开“设置 -> 接口”，确认百度 API Key 和 Secret Key 没有多余空格。")
                << tr8("确认百度智能云语音识别服务已开通，密钥属于同一个应用。")
                << tr8("如果使用代理或 TUN，把百度语音相关域名设置为直连后重试。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：缺少大模型密钥"),
            tr8("弹窗可能写为：缺少 DeepSeek 密钥；缺少 OpenAI 密钥；缺少 Claude 密钥。说明当前功能选择了对应模型服务，但接口页没有保存该服务的密钥。"),
            QStringList()
                << tr8("打开“设置 -> 模型”，确认当前功能选择的是你已经有密钥的模型服务。")
                << tr8("打开“设置 -> 接口”，在“大模型接口”分区填写对应 API Key。")
                << tr8("如果只是测试普通听写，可以先把听写模型切回 DeepSeek 或关闭听写整理。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：网络请求超时"),
            tr8("大模型或语音接口在超时时间内没有返回。它通常不是软件卡死，而是网络链路、代理分流、接口服务器或模型排队过慢。"),
            QStringList()
                << tr8("先在“设置 -> 常用设置”切换“使用系统代理”，分别测试直连和代理。")
                << tr8("在代理软件里把 DeepSeek、OpenAI、Claude、百度、讯飞域名分别按需要设置为代理或直连。")
                << tr8("把当前功能换成响应更快的模型，或缩短选中文本后再试。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：网络请求失败"),
            tr8("弹窗可能写为：网络请求失败：Connection closed、Host requires authentication 或其它原始错误。说明请求到接口时连接被关闭、代理需要认证、DNS 失败或服务端拒绝连接。"),
            QStringList()
                << tr8("如果看到 Host requires authentication，检查代理软件是否要求用户名密码，或关闭软件的“使用系统代理”。")
                << tr8("如果看到 Connection closed，优先检查 TUN/透明代理分流和接口域名直连规则。")
                << tr8("如果只在某一个接口失败，回到“设置 -> 接口”检查该接口密钥和服务是否可用。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：SSL 运行库缺失或版本不匹配"),
            tr8("弹窗会提示程序目录中缺少 libeay32.dll 和 ssleay32.dll，或出现 Error creating SSL context。这通常是 Qt 5.9 需要的 OpenSSL 运行库没有随程序一起部署。"),
            QStringList()
                << tr8("优先使用打包脚本生成的测试包运行，不要只复制 exe。")
                << tr8("确认程序目录里有 Qt 运行库、OpenSSL 相关 DLL 和平台插件目录。")
                << tr8("如果是开发目录运行，重新执行 deploy 脚本后再打开软件。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：接口认证失败"),
            tr8("弹窗会提示检查百度、讯飞、DeepSeek、OpenAI 或 Claude 密钥。一般是 API Key 填错、复制时多了空格、密钥已失效、模型无权限或模型名称不可用。"),
            QStringList()
                << tr8("重新复制密钥，避免前后空格和换行。")
                << tr8("确认接口页保存的是当前模型服务对应的密钥。")
                << tr8("到对应服务控制台检查接口权限和模型是否可用。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：录音为空 / 没有识别到语音"),
            tr8("弹窗可能写为：录音为空；没有识别到语音；讯飞没有识别到语音。通常是麦克风没有输入、录音时间太短、系统默认麦克风错误或录音权限被拦截。"),
            QStringList()
                << tr8("在 Windows 声音设置里确认默认输入设备正确，并能看到音量条变化。")
                << tr8("录音时说话时间稍长一点，避免按下快捷键后立刻停止。")
                << tr8("检查安全软件和系统隐私设置是否禁止程序访问麦克风。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：模型没有返回结果"),
            tr8("语音识别已经完成，但大模型返回为空或接口响应没有有效内容。可能是提示词过强、选中文本过长、模型服务异常或模型名称不可用。"),
            QStringList()
                << tr8("换一个模型重新测试，或减少选中的文字长度。")
                << tr8("打开“提示词”页面，检查对应功能的提示词是否要求过于严格。")
                << tr8("查看历史记录里的原始识别文本和错误信息，确认是哪一步为空。")
        ));

        items->addWidget(faqCard(
            tr8("没有选中文字"),
            tr8("弹窗会提示：请先用鼠标拖选要处理的文字，再按快捷键。默认会先用普通方式读取选中文字；如果开启“强力选中”，普通读取失败后会临时模拟复制来兜底。"),
            QStringList()
                << tr8("用鼠标左键拖动选中网页、文档或文本框里的文字，再按对应快捷键。")
                << tr8("某些网页、PDF 或特殊控件不暴露文本选择内容，可以到“设置 -> 常用设置”开启“强力选中”再试。")
                << tr8("强力选中会临时模拟复制，可能被部分安全软件提示为高危行为；不需要时建议保持关闭。")
                << tr8("为避免浏览器或聊天软件的私有剪贴板格式导致崩溃，强力选中只会恢复文本、HTML、图片和链接等常用内容。")
                << tr8("如果不想读取选中文字，去左侧“功能自定义”关闭该功能的“读取鼠标选中的文字”。")
        ));

        items->addWidget(faqCard(
            tr8("需要输入方式 / 没有启用任何输入方式"),
            tr8("弹窗可能写为：至少需要启用读取鼠标选中的文字或使用语音输入中的一种；这个功能没有启用任何输入方式。说明当前功能没有输入来源。"),
            QStringList()
                << tr8("打开左侧“功能自定义”，给听写、翻译、问答或自定义功能启用至少一种输入。")
                << tr8("自定义功能也要在“设置 -> 自定义功能”里单独设置输入方式。")
                << tr8("翻译如果只想翻译选中文字，就启用“读取鼠标选中的文字”，关闭“使用语音输入”。")
        ));

        items->addWidget(faqCard(
            tr8("提示词已锁定 / 无法保存提示词"),
            tr8("提示词页面和设置里的提示词使用同一份内容。锁定后不能编辑；保存失败通常是 prompts 目录或提示词文件没有写入权限。"),
            QStringList()
                << tr8("如果弹出“提示词已锁定”，先取消“锁定提示词”再编辑。")
                << tr8("如果弹出“无法保存提示词”，确认软件目录不是只读目录，提示词文件没有被其它程序占用。")
                << tr8("保存成功会弹出“已保存”，这是正常提示，不是错误。")
        ));

        items->addWidget(faqCard(
            tr8("保存失败：无法写入配置文件"),
            tr8("弹窗可能写为：无法写入 config/secrets.json；无法写入 config/settings.json；接口密钥已保存，但无法写入语音识别服务选择。"),
            QStringList()
                << tr8("把软件放到桌面、文档或普通磁盘目录，不要放在 Program Files 这类需要管理员权限的位置。")
                << tr8("确认 config 文件夹和里面的 json 文件没有只读属性。")
                << tr8("关闭多个同时运行的软件实例，只保留一个窗口后重新保存。")
        ));

        items->addWidget(faqCard(
            tr8("自定义功能：名称、快捷键或输入方式错误"),
            tr8("弹窗可能写为：名称不能为空；快捷键不能为空；快捷键无效；快捷键冲突；自定义功能至少需要启用选中文字或语音输入中的一种。"),
            QStringList()
                << tr8("每个自定义功能都要填写名称，并设置一个有效快捷键。")
                << tr8("快捷键不能和听写、翻译、问答、设置或其它自定义功能重复。")
                << tr8("每个自定义功能的输入方式、模型、展现方式和显示时间都是独立设置的。")
        ));

        items->addWidget(faqCard(
            tr8("历史记录：无法播放 / 删除失败 / 无法更新收藏状态"),
            tr8("历史记录操作依赖本地 records 目录。录音文件被删除、正在被播放器占用、目录无权限或历史 json 无法写入时，会弹出这些提示。"),
            QStringList()
                << tr8("无法播放时，确认这条记录右侧对应的录音文件仍然存在。")
                << tr8("删除失败时，先停止播放并关闭占用录音文件的程序。")
                << tr8("无法收藏时，确认历史记录保存目录可写，并且没有被同步盘或安全软件锁定。")
        ));

        items->addWidget(faqCard(
            tr8("历史记录：备份、导入或导出失败"),
            tr8("弹窗可能写为：备份失败、导入失败、导出失败、没有选中记录、没有可导出记录或没有录音可导出。通常是没有先选择记录、目录选错、目标目录不可写、录音文件已经被删除，或安全软件/同步盘锁定了文件。"),
            QStringList()
                << tr8("导出前先点“选择记录”勾选单条或多条记录；需要导出当前筛选结果时，点“全选当前”。")
                << tr8("备份会自动保存到当前历史记录保存位置里的“备份文件”，不要手动把备份目录再复制进自己里面。")
                << tr8("导入时选择单独的备份目录；如果选择当前软件目录或 records 的上级目录，软件会拒绝导入，避免把目录复制到自己里面。")
                << tr8("导出录音时只会导出当前筛选结果里仍然存在的录音文件；如果录音被删掉，只能导出文本或详细记录。")
                << tr8("“全部导出”会同时导出文本记录、录音文件和详细 JSON，但仍然只处理你选中的记录。")
                << tr8("如果写入失败，把导出位置换到桌面或文档目录，并暂时避开同步盘、压缩包内部目录和只读目录。")
        ));

        items->addWidget(faqCard(
            tr8("浮动条已关闭"),
            tr8("从托盘或快捷键触发浮动条时，如果常用设置里关闭了浮动条，会弹出“请在设置的常用设置页勾选启用浮动条”。"),
            QStringList()
                << tr8("打开“设置 -> 常用设置”，勾选“启用浮动条”。")
                << tr8("浮动条只在语音输入、识别和处理时临时显示，结束后会按设置的显示时间自动关闭。")
        ));

        items->addWidget(faqCard(
            tr8("删除历史记录 / 删除自定义功能"),
            tr8("这类弹窗是确认提示，不是错误。它会在真正删除本地记录、录音文件或自定义功能前询问一次。"),
            QStringList()
                << tr8("确定删除后，本地记录和对应录音可能无法从软件里恢复。")
                << tr8("不确定时点取消，再先复制或收藏需要保留的内容。")
        ));

        items->addWidget(faqCard(
            tr8("这里没有列出的弹窗"),
            tr8("如果测试人员遇到这里没有出现的弹窗，说明需要继续补充常见问题或修复新的异常路径。"),
            QStringList()
                << tr8("先完整记录弹窗标题和正文。")
                << tr8("再记录当时使用的功能、模型、语音识别服务、是否开启 TUN 或代理。")
                << tr8("把这些信息发给开发者，方便复现和补充说明。")
        ));

        m_faqEmptyLabel = new QLabel(tr8("没有找到匹配的常见问题。"));
        m_faqEmptyLabel->setWordWrap(true);
        m_faqEmptyLabel->setAlignment(Qt::AlignCenter);
        m_faqEmptyLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background: #f2f4f7;"
            "  color: #667085;"
            "  border-radius: 8px;"
            "  padding: 16px;"
            "}"
        ));
        m_faqEmptyLabel->setVisible(false);
        items->addWidget(m_faqEmptyLabel);

        items->addStretch();
        scroll->setWidget(holder);
        layout->addWidget(scroll, 1);
        applyFaqSearch();
        return page;
    }

    void addLatestFeatureFaqItems(QVBoxLayout *items)
    {
        items->addWidget(faqCard(
            tr8("结果小框：重新生成、换模型或继续追问失败"),
            tr8("这些按钮会重新调用当前功能对应的大模型。失败通常和模型密钥、模型名称、网络代理、选中文本过长或当前服务不可用有关。"),
            QStringList()
                << tr8("先到“设置 -> 接口”确认对应大模型密钥已经保存，再到“设置 -> 模型”确认当前功能选择的模型可以使用。")
                << tr8("如果只在换模型后失败，说明新模型所属服务没有填密钥、模型名不可用，或当前账号没有该模型权限。")
                << tr8("继续追问时尽量补一句明确要求；如果选中文本很长，先缩短文本再重试。")
        ));

        items->addWidget(faqCard(
            tr8("结果小框：流式显示中断或一直没有文字"),
            tr8("流式显示会保持一条长连接，边接收边显示。TUN、透明代理、系统代理、防火墙、安全软件或服务端排队都可能导致中断、停顿或超时。"),
            QStringList()
                << tr8("先在“测试工具”里运行网络诊断和接口自检，确认当前网络和模型服务可用。")
                << tr8("在“设置 -> 常用设置”切换“使用系统代理”，分别测试直连和代理；开着 TUN 时请给国内语音接口和所用大模型域名设置正确分流。")
                << tr8("如果频繁中断，换一个响应更快的模型，或减少选中文本长度。")
        ));

        items->addWidget(faqCard(
            tr8("主页功能卡片排序没有保存"),
            tr8("拖动主页功能卡片调整顺序后，会写入 config/settings.json。保存失败通常是配置目录只读、文件被安全软件锁定，或同时打开了多个程序实例。"),
            QStringList()
                << tr8("确认软件放在普通可写目录，不要放在需要管理员权限的系统目录。")
                << tr8("关闭重复启动的 vocekit.exe，只保留一个实例后再调整顺序。")
                << tr8("如果调整后仍恢复原顺序，删除损坏的 config/settings.json 后重新打开软件，让它重新生成默认配置。")
        ));

        items->addWidget(faqCard(
            tr8("录音倒计时或提示音没有出现"),
            tr8("倒计时和提示音是可选设置，只在真正需要语音输入的功能触发前出现。只处理选中文字的功能不会进入录音准备阶段。"),
            QStringList()
                << tr8("打开“设置 -> 常用设置”，确认“启用录音倒计时”或“启用录音提示音”已经勾选。")
                << tr8("打开左侧“功能自定义”，确认当前功能启用了“使用语音输入”，并检查该功能里的倒计时秒数和提示音开关。")
                << tr8("如果提示音没有声音，检查 Windows 系统提示音、默认播放设备和静音状态。")
        ));

        items->addWidget(faqCard(
            tr8("浮动条波形没有变化"),
            tr8("波形只在录音过程中显示，用来提示麦克风是否有输入。没有变化通常是默认麦克风错误、录音权限被拦截、输入音量过低或设备被其它软件占用。"),
            QStringList()
                << tr8("先到“测试工具”运行麦克风测试，确认能检测到有效声音。")
                << tr8("在 Windows 声音设置里确认默认输入设备正确，并观察系统输入音量条是否变化。")
                << tr8("关闭可能独占麦克风的软件，然后重新触发听写或需要语音输入的自定义功能。")
        ));
    }

    QWidget *faqCard(const QString &title, const QString &cause, const QStringList &solutions)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setProperty("faqSearchText", (QStringList() << title << cause << solutions).join(QStringLiteral("\n")));
        frame->setStyleSheet(cardStyle());

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(8);

        auto *titleLabel = new QLabel(title);
        titleLabel->setFont(appFont(12, QFont::DemiBold));
        titleLabel->setWordWrap(true);

        auto *causeLabel = new QLabel(cause);
        causeLabel->setWordWrap(true);
        causeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        causeLabel->setStyleSheet(QStringLiteral("color: #475467;"));

        QString solutionText = tr8("处理办法：");
        for (int i = 0; i < solutions.size(); ++i) {
            solutionText += QStringLiteral("\n%1. %2").arg(i + 1).arg(solutions.at(i));
        }

        auto *solutionLabel = new QLabel(solutionText);
        solutionLabel->setWordWrap(true);
        solutionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        solutionLabel->setStyleSheet(QStringLiteral("color: #111827;"));

        layout->addWidget(titleLabel);
        layout->addWidget(causeLabel);
        layout->addWidget(solutionLabel);
        return frame;
    }

    QWidget *hubToggleRow(const QString &title, const QString &hint, bool checked, const std::function<void(bool)> &onChanged)
    {
        Q_UNUSED(hint);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(10);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *controls = new QHBoxLayout;
        controls->setContentsMargins(0, 0, 0, 0);
        auto *check = new QCheckBox(tr8("启用"));
        check->setChecked(checked);
        check->setFont(appFont(10, QFont::DemiBold));
        connect(check, &QCheckBox::toggled, this, [onChanged](bool enabled) {
            if (onChanged) {
                onChanged(enabled);
            }
        });
        controls->addWidget(check);
        controls->addStretch();

        layout->addLayout(labels);
        layout->addLayout(controls);
        return frame;
    }

    QWidget *hubRecordDirectoryRow()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(10);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(tr8("历史记录保存位置"));
        name->setFont(appFont(11, QFont::DemiBold));
        m_hubRecordDirectoryLabel = new QLabel(m_settings->recordDirectoryPath());
        m_hubRecordDirectoryLabel->setWordWrap(true);
        m_hubRecordDirectoryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_hubRecordDirectoryLabel->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));

        labels->addWidget(name);
        labels->addWidget(m_hubRecordDirectoryLabel);

        auto *choose = new QPushButton(tr8("更改位置"));
        choose->setMinimumSize(92, 34);
        choose->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(choose, &QPushButton::clicked, this, [this]() { chooseRecordDirectoryFromHub(); });

        auto *open = new QPushButton(tr8("打开目录"));
        open->setMinimumSize(92, 34);
        open->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        open->setMenu(recordDirectoryOpenMenu(open, this, [this]() {
            return m_settings->recordDirectoryPath();
        }));

        auto *reset = new QPushButton(tr8("恢复默认"));
        reset->setMinimumSize(92, 34);
        reset->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        connect(reset, &QPushButton::clicked, this, [this]() {
            m_settings->resetRecordDirectory();
            saveHubSettings();
            refreshRecordDirectoryViews();
            refreshHistoryTabs(true);
        });

        auto *buttons = new QHBoxLayout;
        buttons->setContentsMargins(0, 0, 0, 0);
        buttons->setSpacing(8);
        buttons->addWidget(choose);
        buttons->addWidget(open);
        buttons->addWidget(reset);
        buttons->addStretch();

        layout->addLayout(labels);
        layout->addLayout(buttons);
        return frame;
    }

    void chooseRecordDirectoryFromHub()
    {
        const QString dir = QFileDialog::getExistingDirectory(
            this,
            tr8("选择历史记录保存位置"),
            m_settings->recordDirectoryPath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (dir.trimmed().isEmpty()) {
            return;
        }
        m_settings->setRecordDirectoryPath(dir);
        saveHubSettings();
        refreshRecordDirectoryViews();
        refreshHistoryTabs(true);
    }

    void refreshRecordDirectoryViews()
    {
        if (m_hubRecordDirectoryLabel) {
            m_hubRecordDirectoryLabel->setText(m_settings->recordDirectoryPath());
        }
    }

    QWidget *settingsLinkRow(const QString &title, const QString &hint, int tabIndex)
    {
        Q_UNUSED(hint);
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(10);

        auto *labels = new QVBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(11, QFont::DemiBold));
        labels->addWidget(name);

        auto *open = new QPushButton(tr8("打开"));
        open->setFixedHeight(34);
        open->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        connect(open, &QPushButton::clicked, this, [this, tabIndex]() { openSettingsDialog(tabIndex); });

        auto *controls = new QHBoxLayout;
        controls->setContentsMargins(0, 0, 0, 0);
        controls->addWidget(open);
        controls->addStretch();

        layout->addLayout(labels);
        layout->addLayout(controls);
        return frame;
    }

    void openSettingsDialog(int initialTab = 0)
    {
        if (m_settingsPanel) {
            m_settingsPanel->refreshFromSettings();
            m_settingsPanel->setCurrentTab(initialTab);
        }
        selectPage(QStringLiteral("settings"));
        showNormal();
        raise();
        activateWindow();
    }

    void saveHubSettings()
    {
        if (!m_settings->save()) {
            showAttentionWarning(this, tr8("保存失败"), tr8("无法写入 config/settings.json。"));
        }
        refreshStatusLabels();
        refreshRecordDirectoryViews();
        if (m_onSettingsChanged) {
            m_onSettingsChanged();
        }
    }

    QVector<HistoryEntry> loadHistoryEntries() const
    {
        return loadHistoryEntriesFromPath(m_settings->recordDirectoryPath());
    }

    static HistoryEntry historyEntryFromJsonObject(const QJsonObject &item, const QString &filePath)
    {
        HistoryEntry entry;
        entry.modeId = item.value(QStringLiteral("modeId")).toString();
        entry.mode = item.value(QStringLiteral("mode")).toString();
        entry.time = item.value(QStringLiteral("time")).toString();
        entry.input = item.value(QStringLiteral("input")).toString();
        entry.output = item.value(QStringLiteral("output")).toString();
        entry.error = item.value(QStringLiteral("error")).toString();
        entry.audio = item.value(QStringLiteral("audio")).toString();
        entry.textFile = item.value(QStringLiteral("textFile")).toString();
        entry.allAudioFile = item.value(QStringLiteral("allAudioFile")).toString();
        entry.allTextFile = item.value(QStringLiteral("allTextFile")).toString();
        entry.allDetailFile = item.value(QStringLiteral("allDetailFile")).toString();
        entry.model = item.value(QStringLiteral("model")).toString();
        entry.elapsedMs = static_cast<qint64>(item.value(QStringLiteral("elapsedMs")).toDouble(-1));
        entry.favorite = item.value(QStringLiteral("favorite")).toBool(false);
        entry.favoriteFolder = item.value(QStringLiteral("favoriteFolder")).toString();
        entry.draft = item.value(QStringLiteral("draft")).toBool(false);
        entry.filePath = filePath;
        return entry;
    }

    static QVector<HistoryEntry> loadHistoryEntriesFromPath(const QString &recordsPath)
    {
        QVector<HistoryEntry> entries;
        QDir root(historyRootPath(recordsPath));
        if (!root.exists()) {
            return entries;
        }

        const QFileInfoList modeDirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &modeDirInfo : modeDirs) {
            if (isReservedHistoryRootFolder(modeDirInfo.fileName()) || isHistoryDateFolderName(modeDirInfo.fileName())) {
                continue;
            }
            QDir modeDir(modeDirInfo.absoluteFilePath());
            const QFileInfoList dayDirs = modeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
            for (const QFileInfo &dayInfo : dayDirs) {
                if (!isHistoryDateFolderName(dayInfo.fileName())) {
                    continue;
                }
                QDir detailDir(QDir(dayInfo.absoluteFilePath()).filePath(historyDetailSubFolderName()));
                const QFileInfoList files = detailDir.entryInfoList(QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Time);
                for (const QFileInfo &fileInfo : files) {
                    QFile file(fileInfo.absoluteFilePath());
                    if (!file.open(QIODevice::ReadOnly)) {
                        continue;
                    }
                    const QJsonObject item = QJsonDocument::fromJson(file.readAll()).object();
                    entries.append(historyEntryFromJsonObject(item, fileInfo.absoluteFilePath()));
                }
            }
        }

        std::sort(entries.begin(), entries.end(), [](const HistoryEntry &a, const HistoryEntry &b) {
            return a.time > b.time;
        });
        return entries;
    }

    HistoryEntry historyEntryFromFile(const QString &filePath) const
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return HistoryEntry();
        }
        const QJsonObject item = QJsonDocument::fromJson(file.readAll()).object();
        return historyEntryFromJsonObject(item, filePath);
    }

    QString historyTimeText(const QString &iso) const
    {
        const QDateTime time = QDateTime::fromString(iso, Qt::ISODate);
        if (time.isValid()) {
            return time.toString(tr8("yyyy-MM-dd HH:mm:ss"));
        }
        return QString(iso).replace(QStringLiteral("T"), QStringLiteral(" "));
    }

    QString historyElapsedText(qint64 elapsedMs) const
    {
        if (elapsedMs < 0) {
            return tr8("未记录");
        }
        if (elapsedMs < 1000) {
            return QString::number(elapsedMs) + tr8(" 毫秒");
        }
        if (elapsedMs < 60000) {
            return QString::number(elapsedMs / 1000.0, 'f', elapsedMs < 10000 ? 1 : 0) + tr8(" 秒");
        }
        const qint64 minutes = elapsedMs / 60000;
        const qint64 seconds = (elapsedMs % 60000) / 1000;
        return QString::number(minutes) + tr8(" 分 ") + QString::number(seconds) + tr8(" 秒");
    }

    QString historyModelText(const HistoryEntry &entry) const
    {
        const QString model = entry.model.trimmed();
        if (model.isEmpty()) {
            return tr8("未调用大模型");
        }
        for (const ModelOption &option : modelOptions()) {
            if (option.id == model) {
                if (option.title == model) {
                    return option.title;
                }
                return option.title + tr8("（") + model + tr8("）");
            }
        }
        return model;
    }

    QString historyRecognizedText(const HistoryEntry &entry) const
    {
        const QString input = entry.input.trimmed();
        const QString marker = tr8("语音输入：\n");
        const int index = input.indexOf(marker);
        if (index >= 0) {
            return input.mid(index + marker.size()).trimmed();
        }
        if (!entry.audio.trimmed().isEmpty()) {
            return input;
        }
        return QString();
    }

    QString historyDetailPlainText(const HistoryEntry &entry) const
    {
        QStringList parts;
        parts << tr8("功能：") + entry.mode;
        parts << tr8("时间：") + historyTimeText(entry.time);
        parts << tr8("耗时：") + historyElapsedText(entry.elapsedMs);
        parts << tr8("使用模型：") + historyModelText(entry);
        parts << tr8("状态：") + (entry.draft ? tr8("草稿") : tr8("正式记录"));
        parts << tr8("录音：") + (entry.audio.trimmed().isEmpty() ? tr8("本次没有录音") : entry.audio);
        parts << tr8("识别文本：\n") + (historyRecognizedText(entry).trimmed().isEmpty() ? tr8("无") : historyRecognizedText(entry));
        parts << tr8("输入内容：\n") + (entry.input.trimmed().isEmpty() ? tr8("无") : entry.input);
        parts << tr8("模型输出：\n") + (entry.output.trimmed().isEmpty() ? tr8("无") : entry.output);
        parts << tr8("错误：\n") + (entry.error.trimmed().isEmpty() ? tr8("无") : entry.error);
        return parts.join(QStringLiteral("\n\n"));
    }

    QString historyPreview(const HistoryEntry &entry) const
    {
        QString text = entry.output.trimmed();
        if (text.isEmpty()) {
            text = entry.input.trimmed();
        }
        if (text.isEmpty()) {
            text = entry.error.trimmed();
        }
        if (text.isEmpty()) {
            text = tr8("无文本内容");
        }
        text.replace(QRegExp(QStringLiteral("\\s+")), QStringLiteral(" "));
        if (text.size() > 160) {
            text = text.left(160) + QStringLiteral("...");
        }
        return text;
    }

    QString historyTitleText(const HistoryEntry &entry) const
    {
        const QString errorMark = entry.error.trimmed().isEmpty() ? QString() : tr8(" · 有错误");
        const QString folder = entry.favoriteFolder.trimmed();
        const QString favoriteMark = entry.favorite
            ? (folder.isEmpty() ? tr8(" · 已收藏") : tr8(" · 已收藏：") + folder)
            : QString();
        const QString draftMark = entry.draft ? tr8(" · 草稿") : QString();
        return entry.mode + tr8(" · ") + historyTimeText(entry.time) + favoriteMark + draftMark + errorMark;
    }

    bool historyMatchesSearch(const HistoryEntry &entry) const
    {
        const QString keyword = m_historySearchText.trimmed();
        if (keyword.isEmpty()) {
            return true;
        }
        const QString text = (QStringList()
            << entry.mode
            << historyTimeText(entry.time)
            << entry.input
            << entry.output
            << entry.error
            << entry.model
            << entry.favoriteFolder
            << entry.filePath
            << entry.audio
            << entry.textFile
            << entry.allAudioFile
            << entry.allTextFile
            << entry.allDetailFile)
            .join(QStringLiteral("\n"));
        return text.contains(keyword, Qt::CaseInsensitive);
    }

    int historyTextUnits(const QString &text) const
    {
        int units = 0;
        for (int i = 0; i < text.size(); ++i) {
            units += text.at(i).unicode() < 0x80 ? 1 : 2;
        }
        return units;
    }

    int historyRowHeight(const HistoryEntry &entry, QListWidget *list) const
    {
        const int width = list && list->viewport() ? list->viewport()->width() : 0;
        const int titleWidth = qMax(240, width - 180);
        const int previewWidth = qMax(280, width - 90);
        const int titleUnitsPerLine = qBound(34, titleWidth / 7, 94);
        const int previewUnitsPerLine = qBound(42, previewWidth / 7, 110);
        const int titleLines = qBound(1, (historyTextUnits(historyTitleText(entry)) + titleUnitsPerLine - 1) / titleUnitsPerLine, 2);
        const int previewLines = qBound(3, (historyTextUnits(historyPreview(entry)) + previewUnitsPerLine - 1) / previewUnitsPerLine + 1, 7);
        return qBound(150, 46 + titleLines * 24 + previewLines * 24, 300);
    }

    QString builtinHistoryModeId(const QString &modeTitle) const
    {
        if (modeTitle == tr8("听写")) return QStringLiteral("dictate");
        if (modeTitle == tr8("翻译")) return QStringLiteral("translate");
        if (modeTitle == tr8("问答")) return QStringLiteral("ask");
        return QString();
    }

    QString historyEntryEffectiveModeId(const HistoryEntry &entry) const
    {
        if (!entry.modeId.trimmed().isEmpty()) {
            return entry.modeId;
        }
        const QString builtinId = builtinHistoryModeId(entry.mode);
        if (!builtinId.isEmpty()) {
            return builtinId;
        }
        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            if (entry.mode == fn.name) {
                return fn.id;
            }
        }
        return QString();
    }

    bool historyMatchesMode(const HistoryEntry &entry, const QString &modeId) const
    {
        if (modeId == QStringLiteral("__all")) {
            return true;
        }
        if (modeId == QStringLiteral("__favorite")) {
            return entry.favorite;
        }
        const QString favoriteFolderPrefix = QStringLiteral("__favorite_folder:");
        if (modeId.startsWith(favoriteFolderPrefix)) {
            return entry.favorite && entry.favoriteFolder.trimmed() == modeId.mid(favoriteFolderPrefix.size());
        }
        return historyEntryEffectiveModeId(entry) == modeId;
    }

    // 历史记录工具：按当前标签页和搜索条件筛选记录，并负责备份、导入和三种导出。
    QString currentHistoryModeId() const
    {
        if (!m_historyTabs) {
            return QStringLiteral("__all");
        }
        QWidget *page = m_historyTabs->currentWidget();
        const QString mode = page ? page->property("historyMode").toString() : QString();
        return mode.trimmed().isEmpty() ? QStringLiteral("__all") : mode;
    }

    QVector<HistoryEntry> currentFilteredHistoryEntries() const
    {
        const QVector<HistoryEntry> source = loadHistoryEntries();
        const QString mode = currentHistoryModeId();
        QVector<HistoryEntry> filtered;
        filtered.reserve(source.size());
        for (const HistoryEntry &entry : source) {
            if (!historyMatchesMode(entry, mode)) {
                continue;
            }
            if (!historyMatchesSearch(entry)) {
                continue;
            }
            filtered.append(entry);
        }
        return filtered;
    }

    QString exportTimestamp() const
    {
        return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    }

    QJsonObject historyEntryToJson(const HistoryEntry &entry) const
    {
        QJsonObject item;
        item.insert(QStringLiteral("modeId"), entry.modeId);
        item.insert(QStringLiteral("mode"), entry.mode);
        item.insert(QStringLiteral("time"), entry.time);
        item.insert(QStringLiteral("timeText"), historyTimeText(entry.time));
        item.insert(QStringLiteral("elapsedMs"), static_cast<double>(entry.elapsedMs));
        item.insert(QStringLiteral("elapsedText"), historyElapsedText(entry.elapsedMs));
        item.insert(QStringLiteral("model"), entry.model);
        item.insert(QStringLiteral("modelText"), historyModelText(entry));
        item.insert(QStringLiteral("recognizedText"), historyRecognizedText(entry));
        item.insert(QStringLiteral("input"), entry.input);
        item.insert(QStringLiteral("output"), entry.output);
        item.insert(QStringLiteral("error"), entry.error);
        item.insert(QStringLiteral("audio"), entry.audio);
        item.insert(QStringLiteral("textFile"), entry.textFile);
        item.insert(QStringLiteral("allAudioFile"), entry.allAudioFile);
        item.insert(QStringLiteral("allTextFile"), entry.allTextFile);
        item.insert(QStringLiteral("allDetailFile"), entry.allDetailFile);
        item.insert(QStringLiteral("audioExists"), !entry.audio.trimmed().isEmpty() && QFileInfo::exists(entry.audio));
        item.insert(QStringLiteral("favorite"), entry.favorite);
        item.insert(QStringLiteral("favoriteFolder"), entry.favoriteFolder);
        item.insert(QStringLiteral("draft"), entry.draft);
        item.insert(QStringLiteral("sourceFile"), entry.filePath);
        return item;
    }

    void ensureHistoryCacheLoaded()
    {
        if (!m_historyCacheValid) {
            m_historyEntriesCache = loadHistoryEntries();
            m_historyCacheValid = true;
        }
    }

    void backupHistoryRecords()
    {
        const QString sourcePath = historyRootPath(m_settings->recordDirectoryPath());
        if (!QDir(sourcePath).exists()) {
            QDir().mkpath(sourcePath);
        }
        ensureHistoryRootStructure(sourcePath);

        const QString targetPath = QDir(historyBackupDirectory(sourcePath)).filePath(QStringLiteral("vocekit-records-backup_") + exportTimestamp());
        QString error;
        int fileCount = 0;
        if (!copyDirectoryContentsRecursiveExcept(sourcePath, targetPath, QSet<QString>() << historyBackupFolderName(), false, &error, &fileCount)) {
            showAttentionWarning(this, tr8("备份失败"), error.isEmpty() ? tr8("无法复制历史记录文件。") : error);
            return;
        }
        showAttentionInformation(this, tr8("备份完成"), tr8("已备份 ") + QString::number(fileCount) + tr8(" 个文件到：\n") + targetPath);
    }

    void importHistoryRecords()
    {
        const QString selectedPath = QFileDialog::getExistingDirectory(
            this,
            tr8("选择要导入的历史记录目录"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (selectedPath.isEmpty()) {
            return;
        }

        const QString sourcePath = QDir(selectedPath).absolutePath();
        const QString targetPath = historyRootPath(m_settings->recordDirectoryPath());
        if (pathIsSameOrInside(targetPath, sourcePath)) {
            showAttentionWarning(this, tr8("导入失败"), tr8("不能从当前历史记录保存目录或它的上级目录导入，请选择单独的备份目录。"));
            return;
        }
        ensureHistoryRootStructure(targetPath);

        QString error;
        int fileCount = 0;
        if (!copyDirectoryContentsRecursive(sourcePath, targetPath, true, &error, &fileCount)) {
            showAttentionWarning(this, tr8("导入失败"), error.isEmpty() ? tr8("无法导入历史记录文件。") : error);
            return;
        }
        if (fileCount == 0) {
            showAttentionWarning(this, tr8("导入失败"), tr8("选择的目录里没有可导入的文件。"));
            return;
        }

        m_historyCacheValid = false;
        refreshHistoryTabs(true);
        showAttentionInformation(this, tr8("导入完成"), tr8("已导入 ") + QString::number(fileCount) + tr8(" 个文件。"));
    }

    QVector<HistoryEntry> selectedCurrentHistoryEntries() const
    {
        const QVector<HistoryEntry> filtered = currentFilteredHistoryEntries();
        QVector<HistoryEntry> selected;
        selected.reserve(qMin(filtered.size(), m_selectedHistoryFiles.size()));
        for (const HistoryEntry &entry : filtered) {
            if (m_selectedHistoryFiles.contains(entry.filePath)) {
                selected.append(entry);
            }
        }
        return selected;
    }

    bool selectedHistoryEntriesForExport(QVector<HistoryEntry> *entries)
    {
        if (!entries) {
            return false;
        }

        const QVector<HistoryEntry> filtered = currentFilteredHistoryEntries();
        if (filtered.isEmpty()) {
            showAttentionInformation(this, tr8("没有可导出记录"), tr8("当前筛选条件下没有历史记录。"));
            entries->clear();
            return false;
        }

        QVector<HistoryEntry> selected;
        selected.reserve(qMin(filtered.size(), m_selectedHistoryFiles.size()));
        for (const HistoryEntry &entry : filtered) {
            if (m_selectedHistoryFiles.contains(entry.filePath)) {
                selected.append(entry);
            }
        }

        if (selected.isEmpty()) {
            showAttentionInformation(
                this,
                tr8("没有选中记录"),
                tr8("请先点“选择记录”，勾选要导出的历史记录；也可以点“全选当前”导出当前筛选结果。")
            );
            entries->clear();
            return false;
        }

        *entries = selected;
        return true;
    }

    bool writeHistoryTextExport(const QVector<HistoryEntry> &entries, const QString &path) const
    {
        QStringList blocks;
        for (int i = 0; i < entries.size(); ++i) {
            blocks << (tr8("第 ") + QString::number(i + 1) + tr8(" 条\n") + historyDetailPlainText(entries.at(i)));
        }
        return writeTextFile(path, blocks.join(QStringLiteral("\n\n==============================\n\n")));
    }

    bool writeHistoryDetailsExport(const QVector<HistoryEntry> &entries, const QString &path) const
    {
        QJsonArray records;
        for (const HistoryEntry &entry : entries) {
            records.append(historyEntryToJson(entry));
        }
        QJsonObject root;
        root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
        root.insert(QStringLiteral("recordCount"), entries.size());
        root.insert(QStringLiteral("filterMode"), currentHistoryModeId());
        root.insert(QStringLiteral("searchText"), m_historySearchText);
        root.insert(QStringLiteral("selectionOnly"), true);
        root.insert(QStringLiteral("records"), records);

        QFile file(path);
        QFileInfo info(path);
        if (!info.dir().exists()) {
            info.dir().mkpath(QStringLiteral("."));
        }
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        return true;
    }

    bool exportHistoryAudioFiles(const QVector<HistoryEntry> &entries, const QString &targetPath, int *exported, QString *error) const
    {
        if (exported) {
            *exported = 0;
        }
        if (error) {
            error->clear();
        }

        QDir target(targetPath);
        if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
            if (error) {
                *error = tr8("无法创建录音导出目录。");
            }
            return false;
        }

        int count = 0;
        QString copyError;
        for (int i = 0; i < entries.size(); ++i) {
            const HistoryEntry entry = entries.at(i);
            QString sourceAudio = entry.audio.trimmed();
            if ((sourceAudio.isEmpty() || !QFileInfo::exists(sourceAudio)) && QFileInfo::exists(entry.allAudioFile)) {
                sourceAudio = entry.allAudioFile;
            }
            if (sourceAudio.isEmpty() || !QFileInfo::exists(sourceAudio)) {
                continue;
            }
            const QFileInfo audioInfo(sourceAudio);
            const QString timePart = historyTimeText(entry.time).remove(QRegExp(QStringLiteral("[^0-9]")));
            const QString fileName = safeFileNamePart(timePart + QStringLiteral("_") + entry.mode + QStringLiteral("_") + QString::number(i + 1), QStringLiteral("audio"))
                + QStringLiteral(".") + (audioInfo.suffix().isEmpty() ? QStringLiteral("wav") : audioInfo.suffix());
            if (!copyFileToPath(sourceAudio, target.filePath(fileName), true, &copyError, &count)) {
                if (error) {
                    *error = copyError.isEmpty() ? tr8("无法复制录音文件。") : copyError;
                }
                return false;
            }
        }

        if (exported) {
            *exported = count;
        }
        return true;
    }

    void exportHistoryText()
    {
        QVector<HistoryEntry> entries;
        if (!selectedHistoryEntriesForExport(&entries)) {
            return;
        }

        const QString path = QFileDialog::getSaveFileName(
            this,
            tr8("导出文本记录"),
            QDir::home().filePath(QStringLiteral("vocekit-history-text_") + exportTimestamp() + QStringLiteral(".txt")),
            tr8("文本文件 (*.txt)")
        );
        if (path.isEmpty()) {
            return;
        }

        if (!writeHistoryTextExport(entries, path)) {
            showAttentionWarning(this, tr8("导出失败"), tr8("无法写入文本导出文件。"));
            return;
        }
        showAttentionInformation(this, tr8("导出完成"), tr8("已导出 ") + QString::number(entries.size()) + tr8(" 条文本记录。"));
    }

    void exportHistoryDetails()
    {
        QVector<HistoryEntry> entries;
        if (!selectedHistoryEntriesForExport(&entries)) {
            return;
        }

        const QString path = QFileDialog::getSaveFileName(
            this,
            tr8("导出详细记录"),
            QDir::home().filePath(QStringLiteral("vocekit-history-details_") + exportTimestamp() + QStringLiteral(".json")),
            tr8("JSON 文件 (*.json)")
        );
        if (path.isEmpty()) {
            return;
        }

        if (!writeHistoryDetailsExport(entries, path)) {
            showAttentionWarning(this, tr8("导出失败"), tr8("无法写入详细记录文件。"));
            return;
        }
        showAttentionInformation(this, tr8("导出完成"), tr8("已导出 ") + QString::number(entries.size()) + tr8(" 条详细记录。"));
    }

    void exportHistoryAudio()
    {
        QVector<HistoryEntry> entries;
        if (!selectedHistoryEntriesForExport(&entries)) {
            return;
        }

        const QString parentPath = QFileDialog::getExistingDirectory(
            this,
            tr8("选择录音导出位置"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (parentPath.isEmpty()) {
            return;
        }

        const QString targetPath = QDir(parentPath).filePath(QStringLiteral("vocekit-audio-export_") + exportTimestamp());
        int exported = 0;
        QString error;
        if (!exportHistoryAudioFiles(entries, targetPath, &exported, &error)) {
            showAttentionWarning(this, tr8("导出失败"), error.isEmpty() ? tr8("无法复制录音文件。") : error);
            return;
        }

        if (exported == 0) {
            showAttentionInformation(this, tr8("没有录音可导出"), tr8("当前筛选结果里没有存在于本地的录音文件。"));
            return;
        }
        showAttentionInformation(this, tr8("导出完成"), tr8("已导出 ") + QString::number(exported) + tr8(" 个录音文件到：\n") + targetPath);
    }

    void exportHistoryAll()
    {
        QVector<HistoryEntry> entries;
        if (!selectedHistoryEntriesForExport(&entries)) {
            return;
        }

        const QString parentPath = QFileDialog::getExistingDirectory(
            this,
            tr8("选择全部导出位置"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );
        if (parentPath.isEmpty()) {
            return;
        }

        const QString targetPath = QDir(parentPath).filePath(QStringLiteral("vocekit-history-export_") + exportTimestamp());
        QDir target(targetPath);
        if (!target.exists() && !target.mkpath(QStringLiteral("."))) {
            showAttentionWarning(this, tr8("导出失败"), tr8("无法创建全部导出目录。"));
            return;
        }

        const QString textPath = target.filePath(tr8("文本记录.txt"));
        const QString detailsPath = target.filePath(tr8("详细记录.json"));
        const QString audioPath = target.filePath(tr8("录音"));
        if (!writeHistoryTextExport(entries, textPath)) {
            showAttentionWarning(this, tr8("导出失败"), tr8("无法写入文本导出文件。"));
            return;
        }
        if (!writeHistoryDetailsExport(entries, detailsPath)) {
            showAttentionWarning(this, tr8("导出失败"), tr8("无法写入详细记录文件。"));
            return;
        }

        int exportedAudio = 0;
        QString error;
        if (!exportHistoryAudioFiles(entries, audioPath, &exportedAudio, &error)) {
            showAttentionWarning(this, tr8("导出失败"), error.isEmpty() ? tr8("无法复制录音文件。") : error);
            return;
        }

        showAttentionInformation(
            this,
            tr8("导出完成"),
            tr8("已导出 ") + QString::number(entries.size()) + tr8(" 条记录，录音 ") + QString::number(exportedAudio) + tr8(" 个。\n") + targetPath
        );
    }

    QPushButton *historyActionButton(const QString &text, const QString &background = QStringLiteral("#ffffff"), const QString &foreground = QStringLiteral("#111827"))
    {
        auto *button = new QPushButton(text);
        button->setFixedSize(70, 36);
        button->setFont(appFont(9, QFont::DemiBold));
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: 1px solid #e4e7ec;"
            "  border-radius: 6px;"
            "  padding: 0;"
            "  margin: 0;"
            "  min-height: 36px;"
            "  max-height: 36px;"
            "}"
            "QPushButton:hover { background: %3; }"
        ).arg(background, foreground, background == QStringLiteral("#ffffff") ? QStringLiteral("#f2f4f7") : QStringLiteral("#1f2937")));
        return button;
    }

    QPushButton *historyMenuButton(const HistoryEntry &entry)
    {
        auto *button = new QPushButton(tr8("操作"));
        button->setFixedSize(68, 34);
        button->setFont(appFont(9, QFont::DemiBold));
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: #ffffff;"
            "  color: #111827;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 0;"
            "}"
            "QPushButton:hover { background: #f2f4f7; }"
            "QPushButton::menu-indicator { width: 0; image: none; }"
        ));

        auto *menu = new QMenu(button);
        menu->setStyleSheet(QStringLiteral(
            "QMenu { background: #ffffff; border: 1px solid #d0d5dd; padding: 6px; }"
            "QMenu::item { padding: 8px 28px 8px 12px; color: #111827; }"
            "QMenu::item:selected { background: #eef2ff; }"
        ));

        QAction *detail = menu->addAction(tr8("查看详情"));
        QAction *play = menu->addAction(tr8("播放录音"));
        QAction *copy = menu->addAction(tr8("复制内容"));
        QAction *favorite = menu->addAction(entry.favorite ? tr8("取消收藏") : tr8("收藏"));
        QMenu *folderMenu = menu->addMenu(tr8("加入收藏夹"));
        QAction *newFolder = folderMenu->addAction(tr8("新建收藏夹..."));
        const QStringList folders = m_settings->favoriteFolders();
        if (!folders.isEmpty()) {
            folderMenu->addSeparator();
        }
        for (const QString &folder : folders) {
            QAction *folderAction = folderMenu->addAction(folder);
            connect(folderAction, &QAction::triggered, this, [this, entry, folder]() {
                setHistoryFavoriteFolder(entry.filePath, folder);
            });
        }
        QAction *remove = menu->addAction(tr8("删除"));

        connect(detail, &QAction::triggered, this, [this, entry]() {
            showHistoryDetail(historyEntryFromFile(entry.filePath));
        });
        connect(play, &QAction::triggered, this, [this, entry]() {
            playHistoryAudio(entry);
        });
        connect(copy, &QAction::triggered, this, [entry]() {
            QApplication::clipboard()->setText(entry.output.trimmed().isEmpty() ? entry.input : entry.output);
        });
        connect(favorite, &QAction::triggered, this, [this, entry]() {
            toggleHistoryFavorite(entry.filePath);
        });
        connect(newFolder, &QAction::triggered, this, [this, entry]() {
            const QString folder = createFavoriteFolderDialog();
            if (!folder.isEmpty()) {
                setHistoryFavoriteFolder(entry.filePath, folder);
            }
        });
        connect(remove, &QAction::triggered, this, [this, entry]() {
            deleteHistoryEntry(entry);
        });

        button->setMenu(menu);
        return button;
    }

    QWidget *historyRowWidget(const HistoryEntry &entry, QListWidget *list)
    {
        auto *row = new HistoryRowFrame;
        row->setObjectName(QStringLiteral("historyRow"));
        row->setClickCallback([this, entry]() {
            if (!m_historyBatchMode) {
                showHistoryDetail(historyEntryFromFile(entry.filePath));
            }
        });
        const int rowHeight = historyRowHeight(entry, list);
        row->setMinimumHeight(rowHeight);
        auto *layout = new QVBoxLayout(row);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(10);

        auto *title = new QLabel(historyTitleText(entry));
        title->setFont(appFont(10, QFont::DemiBold));
        title->setStyleSheet(QStringLiteral("color: #111827;"));
        title->setWordWrap(true);
        title->setMinimumHeight(30);
        title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto *preview = new QLabel(historyPreview(entry));
        preview->setFont(appFont(10));
        preview->setStyleSheet(QStringLiteral("color: #667085;"));
        preview->setWordWrap(true);
        preview->setMinimumHeight(qMax(78, rowHeight - 70));
        preview->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        preview->setAttribute(Qt::WA_TransparentForMouseEvents);

        auto *header = new QHBoxLayout;
        header->setContentsMargins(0, 0, 0, 0);
        header->setSpacing(10);
        if (m_historyBatchMode) {
            auto *select = new QCheckBox;
            select->setFixedSize(28, 30);
            select->setChecked(m_selectedHistoryFiles.contains(entry.filePath));
            select->setToolTip(tr8("选择这条记录"));
            connect(select, &QCheckBox::toggled, this, [this, entry](bool checked) {
                if (checked) {
                    m_selectedHistoryFiles.insert(entry.filePath);
                } else {
                    m_selectedHistoryFiles.remove(entry.filePath);
                }
                updateHistoryBatchButtons();
            });
            header->addWidget(select, 0, Qt::AlignTop);
        }
        header->addWidget(title, 1);
        header->addWidget(historyMenuButton(entry), 0, Qt::AlignTop);

        layout->addLayout(header);
        layout->addWidget(preview, 1);

        row->setStyleSheet(QStringLiteral(
            "QWidget#historyRow { background: #f9fafb; border: 1px solid #eef0f4; border-radius: 6px; }"
            "QLabel { border: none; background: transparent; }"
        ));

        Q_UNUSED(list);
        return row;
    }

    void appendHistoryRows(QListWidget *list, const QVector<HistoryEntry> &entries, int start, int count)
    {
        if (!list || start < 0 || count <= 0) {
            return;
        }
        const int end = qMin(start + count, entries.size());
        for (int i = start; i < end; ++i) {
            const HistoryEntry entry = entries.at(i);
            auto *item = new QListWidgetItem;
            item->setData(Qt::UserRole, entry.filePath);
            item->setSizeHint(QSize(0, historyRowHeight(entry, list)));
            list->addItem(item);
            list->setItemWidget(item, historyRowWidget(entry, list));
        }
    }

    void appendHistoryRowsFromIndexes(QListWidget *list, const QVector<HistoryEntry> &entries, const QVector<int> &indexes, int start, int count)
    {
        if (!list || start < 0 || count <= 0) {
            return;
        }
        const int end = qMin(start + count, indexes.size());
        for (int i = start; i < end; ++i) {
            const int entryIndex = indexes.at(i);
            if (entryIndex < 0 || entryIndex >= entries.size()) {
                continue;
            }
            const HistoryEntry entry = entries.at(entryIndex);
            auto *item = new QListWidgetItem;
            item->setData(Qt::UserRole, entry.filePath);
            item->setSizeHint(QSize(0, historyRowHeight(entry, list)));
            list->addItem(item);
            list->setItemWidget(item, historyRowWidget(entry, list));
        }
    }

    void addHistoryLoadMoreItem(QListWidget *list, const QSharedPointer<QVector<HistoryEntry>> &entries, int nextStart, int batchSize)
    {
        if (!list || !entries || nextStart >= entries->size()) {
            return;
        }

        auto *item = new QListWidgetItem;
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setSizeHint(QSize(0, 54));
        list->addItem(item);

        auto *button = new QPushButton(tr8("加载更多"));
        button->setFixedHeight(38);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        list->setItemWidget(item, button);

        connect(button, &QPushButton::clicked, this, [this, list, item, entries, nextStart, batchSize]() {
            const int row = list->row(item);
            if (row >= 0) {
                delete list->takeItem(row);
            }
            appendHistoryRows(list, *entries, nextStart, batchSize);
            addHistoryLoadMoreItem(list, entries, nextStart + batchSize, batchSize);
        });
    }

    void addHistoryLoadMoreIndexItem(QListWidget *list, const QSharedPointer<QVector<int>> &indexes, int nextStart, int batchSize)
    {
        if (!list || !indexes || nextStart >= indexes->size()) {
            return;
        }

        auto *item = new QListWidgetItem;
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setSizeHint(QSize(0, 54));
        list->addItem(item);

        auto *button = new QPushButton(tr8("加载更多"));
        button->setFixedHeight(38);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        list->setItemWidget(item, button);

        connect(button, &QPushButton::clicked, this, [this, list, item, indexes, nextStart, batchSize]() {
            const int row = list->row(item);
            if (row >= 0) {
                delete list->takeItem(row);
            }
            appendHistoryRowsFromIndexes(list, m_historyEntriesCache, *indexes, nextStart, batchSize);
            addHistoryLoadMoreIndexItem(list, indexes, nextStart + batchSize, batchSize);
        });
    }

    QListWidget *historyListForMode(const QString &modeId, const QVector<HistoryEntry> &entries, int maxRows = 0)
    {
        auto *list = new QListWidget;
        list->setFrameShape(QFrame::NoFrame);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setTextElideMode(Qt::ElideNone);
        list->setStyleSheet(QStringLiteral(
            "QListWidget { background: transparent; padding: 10px; }"
            "QListWidget::item {"
            "  background: transparent;"
            "  border: none;"
            "  margin: 6px 0;"
            "}"
            "QListWidget::item:selected { background: transparent; }"
        ));

        if (modeId == QStringLiteral("__all")) {
            QSharedPointer<QVector<int>> indexes(new QVector<int>);
            indexes->reserve(entries.size());
            for (int i = 0; i < entries.size(); ++i) {
                if (!historyMatchesSearch(entries.at(i))) {
                    continue;
                }
                indexes->append(i);
            }

            if (indexes->isEmpty()) {
                auto *empty = new QListWidgetItem(tr8("暂无记录"));
                empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
                list->addItem(empty);
            } else {
                const int firstBatchSize = m_settings->historyInitialLoadCount();
                const int moreBatchSize = m_settings->historyLoadMoreCount();
                const bool pagedAll = maxRows <= 0 && indexes->size() > firstBatchSize;
                const int initialRows = maxRows > 0 ? qMin(maxRows, indexes->size()) : (pagedAll ? firstBatchSize : indexes->size());
                appendHistoryRowsFromIndexes(list, entries, *indexes, 0, initialRows);

                if (maxRows > 0 && indexes->size() > maxRows) {
                    auto *more = new QListWidgetItem(tr8("还有更多记录，请打开左侧“历史记录”查看全部。"));
                    more->setFlags(more->flags() & ~Qt::ItemIsSelectable);
                    list->addItem(more);
                } else if (pagedAll) {
                    addHistoryLoadMoreIndexItem(list, indexes, initialRows, moreBatchSize);
                }
            }

            return list;
        }

        QSharedPointer<QVector<HistoryEntry>> filtered(new QVector<HistoryEntry>);
        for (const HistoryEntry &entry : entries) {
            if (!historyMatchesMode(entry, modeId)) {
                continue;
            }
            if (!historyMatchesSearch(entry)) {
                continue;
            }
            filtered->append(entry);
        }

        if (filtered->isEmpty()) {
            auto *empty = new QListWidgetItem(tr8("暂无记录"));
            empty->setFlags(empty->flags() & ~Qt::ItemIsSelectable);
            list->addItem(empty);
        } else {
            const int firstBatchSize = m_settings->historyInitialLoadCount();
            const int moreBatchSize = m_settings->historyLoadMoreCount();
            const bool paged = maxRows <= 0 && filtered->size() > firstBatchSize;
            const int initialRows = maxRows > 0 ? qMin(maxRows, filtered->size()) : (paged ? firstBatchSize : filtered->size());
            appendHistoryRows(list, *filtered, 0, initialRows);

            if (maxRows > 0 && filtered->size() > maxRows) {
                auto *more = new QListWidgetItem(tr8("还有更多记录，请打开左侧“历史记录”查看全部。"));
                more->setFlags(more->flags() & ~Qt::ItemIsSelectable);
                list->addItem(more);
            } else if (paged) {
                addHistoryLoadMoreItem(list, filtered, initialRows, moreBatchSize);
            }
        }

        return list;
    }

    QVector<HistoryTabDef> historyTabModes() const
    {
        QVector<HistoryTabDef> tabs;
        tabs.append({QStringLiteral("__all"), tr8("全部")});
        tabs.append({QStringLiteral("__favorite"), tr8("收藏")});
        tabs.append({QStringLiteral("dictate"), tr8("听写")});
        tabs.append({QStringLiteral("translate"), tr8("翻译")});
        tabs.append({QStringLiteral("ask"), tr8("问答")});
        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            const QString title = fn.name.trimmed().isEmpty() ? tr8("自定义功能") : fn.name.trimmed();
            tabs.append({fn.id, title});
        }
        return tabs;
    }

    QWidget *historyTabPlaceholder(const QString &modeId, const QString &message = QString())
    {
        auto *page = new QWidget;
        page->setProperty("historyMode", modeId);
        page->setProperty("historyLoaded", false);
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        if (!message.trimmed().isEmpty()) {
            auto *label = new QLabel(message);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet(QStringLiteral("color: #667085; padding: 40px;"));
            layout->addWidget(label, 1);
        }
        return page;
    }

    QWidget *historyViewForMode(const QString &modeId, const QVector<HistoryEntry> &entries, int maxRows = 0)
    {
        if (modeId != QStringLiteral("__favorite")) {
            return historyListForMode(modeId, entries, maxRows);
        }

        auto *view = new QWidget;
        auto *layout = new QVBoxLayout(view);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        auto *tools = new QHBoxLayout;
        tools->setSpacing(10);
        auto *label = new QLabel(tr8("收藏夹"));
        label->setFont(appFont(10, QFont::DemiBold));
        auto *folderBox = new QComboBox;
        folderBox->setMinimumHeight(40);
        folderBox->setMinimumWidth(220);
        folderBox->addItem(tr8("全部收藏"), QStringLiteral("__favorite"));
        for (const QString &folder : m_settings->favoriteFolders()) {
            folderBox->addItem(folder, QStringLiteral("__favorite_folder:") + folder);
        }

        auto *create = new QPushButton(tr8("新建收藏夹"));
        create->setFont(appFont(10, QFont::DemiBold));
        create->setFixedSize(126, 42);
        create->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));

        auto *listHost = new QWidget;
        auto *listLayout = new QVBoxLayout(listHost);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(0);

        auto rebuildList = [this, folderBox, listLayout, entries, maxRows]() {
            clearLayout(listLayout);
            listLayout->addWidget(historyListForMode(folderBox->currentData().toString(), entries, maxRows));
        };

        connect(folderBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [rebuildList](int) {
            rebuildList();
        });
        connect(create, &QPushButton::clicked, this, [this]() {
            const QString folder = createFavoriteFolderDialog();
            if (!folder.isEmpty()) {
                refreshHistoryTabs(true);
            }
        });

        tools->addWidget(label);
        tools->addWidget(folderBox);
        tools->addWidget(create);
        tools->addStretch();
        layout->addLayout(tools);
        layout->addWidget(listHost, 1);
        rebuildList();
        return view;
    }

    void populateHistoryTab(int index)
    {
        if (!m_historyTabs || index < 0 || index >= m_historyTabs->count()) {
            return;
        }
        QWidget *page = m_historyTabs->widget(index);
        if (!page || page->property("historyLoaded").toBool()) {
            return;
        }

        const QString mode = page->property("historyMode").toString();
        auto *layout = qobject_cast<QVBoxLayout *>(page->layout());
        if (!layout) {
            return;
        }
        clearLayout(layout);
        layout->addWidget(historyViewForMode(mode, m_historyEntriesCache));
        page->setProperty("historyLoaded", true);
    }

    QString createFavoriteFolderDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(tr8("新建收藏夹"));
        dialog.setModal(true);
        dialog.setMinimumSize(420, 190);
        dialog.setStyleSheet(QStringLiteral("QDialog { background: #f6f7f9; } QLabel { color: #111827; }"));

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(20, 18, 20, 18);
        layout->setSpacing(12);

        auto *title = new QLabel(tr8("收藏夹名称"));
        title->setFont(appFont(13, QFont::DemiBold));
        auto *input = new QLineEdit;
        input->setMinimumHeight(40);
        input->setPlaceholderText(tr8("例如：常用翻译、重要问答"));
        input->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 8px; padding: 0 12px; }"
        ));

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *cancel = new QPushButton(tr8("取消"));
        cancel->setFont(appFont(10, QFont::DemiBold));
        cancel->setFixedSize(86, 42);
        cancel->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *create = new QPushButton(tr8("创建"));
        create->setFont(appFont(10, QFont::DemiBold));
        create->setFixedSize(86, 42);
        create->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));
        buttons->addWidget(cancel);
        buttons->addWidget(create);

        layout->addWidget(title);
        layout->addWidget(input);
        layout->addStretch();
        layout->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(create, &QPushButton::clicked, &dialog, [&dialog]() {
            dialog.accept();
        });

        if (dialog.exec() != QDialog::Accepted) {
            return QString();
        }
        const QString name = input->text().trimmed();
        if (name.isEmpty()) {
            showAttentionWarning(this, tr8("名称不能为空"), tr8("收藏夹必须填写名称。"));
            return QString();
        }
        if (!m_settings->addFavoriteFolder(name)) {
            showAttentionWarning(this, tr8("无法创建"), tr8("收藏夹名称为空或已经存在。"));
            return QString();
        }
        if (!m_settings->save()) {
            showAttentionWarning(this, tr8("保存失败"), tr8("无法写入 config/settings.json。"));
            return QString();
        }
        return name;
    }

    void playHistoryAudio(const HistoryEntry &entry)
    {
        QString audioPath = entry.audio.trimmed();
        if ((audioPath.isEmpty() || !QFileInfo::exists(audioPath)) && QFileInfo::exists(entry.allAudioFile)) {
            audioPath = entry.allAudioFile;
        }
        if (audioPath.isEmpty() || !QFileInfo::exists(audioPath)) {
            showAttentionWarning(this, tr8("无法播放"), tr8("这条记录没有可播放的录音文件。"));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(audioPath));
    }

    bool writeHistoryFavorite(const QString &filePath, bool favorite, const QString &folder = QString())
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        QJsonObject item = QJsonDocument::fromJson(file.readAll()).object();
        file.close();

        item.insert(QStringLiteral("favorite"), favorite);
        if (favorite && !folder.trimmed().isEmpty()) {
            item.insert(QStringLiteral("favoriteFolder"), folder.trimmed());
        } else if (!favorite) {
            item.remove(QStringLiteral("favoriteFolder"));
        }
        const QByteArray json = QJsonDocument(item).toJson(QJsonDocument::Indented);
        const QStringList paths = QStringList() << filePath << item.value(QStringLiteral("allDetailFile")).toString();
        bool wroteMain = false;
        for (const QString &path : paths) {
            if (path.trimmed().isEmpty() || (path != filePath && !QFileInfo::exists(path))) {
                continue;
            }
            QFile target(path);
            if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                if (path == filePath) {
                    return false;
                }
                continue;
            }
            target.write(json);
            if (path == filePath) {
                wroteMain = true;
            }
        }
        return wroteMain;
    }

    void toggleHistoryFavorite(const QString &filePath)
    {
        if (filePath.trimmed().isEmpty()) {
            return;
        }
        const HistoryEntry entry = historyEntryFromFile(filePath);
        if (!writeHistoryFavorite(filePath, !entry.favorite)) {
            showAttentionWarning(this, tr8("保存失败"), tr8("无法更新收藏状态。"));
            return;
        }
        refreshHistoryTabs(true);
    }

    void setHistoryFavoriteFolder(const QString &filePath, const QString &folder)
    {
        const QString trimmed = folder.trimmed();
        if (filePath.trimmed().isEmpty() || trimmed.isEmpty()) {
            return;
        }
        if (!m_settings->favoriteFolders().contains(trimmed)) {
            m_settings->addFavoriteFolder(trimmed);
            m_settings->save();
        }
        if (!writeHistoryFavorite(filePath, true, trimmed)) {
            showAttentionWarning(this, tr8("保存失败"), tr8("无法更新收藏夹。"));
            return;
        }
        refreshHistoryTabs(true);
    }

    void deleteHistoryEntry(const HistoryEntry &entry)
    {
        if (entry.filePath.trimmed().isEmpty()) {
            return;
        }
        if (QMessageBox::question(this, tr8("删除历史记录"), tr8("确定删除这条历史记录和对应录音吗？")) != QMessageBox::Yes) {
            return;
        }

        if (!deleteHistoryEntryFiles(entry)) {
            showAttentionWarning(this, tr8("删除失败"), tr8("部分文件无法删除，请检查文件是否正在被占用。"));
        }
        refreshHistoryTabs(true);
    }

    bool deleteHistoryEntryFiles(const HistoryEntry &entry)
    {
        bool ok = true;
        const QStringList relatedFiles = QStringList()
            << entry.audio
            << entry.allAudioFile
            << entry.textFile
            << entry.allTextFile
            << entry.filePath
            << entry.allDetailFile;
        QSet<QString> removed;
        for (const QString &path : relatedFiles) {
            const QString trimmed = path.trimmed();
            if (trimmed.isEmpty()) {
                continue;
            }
            const QString normalized = normalizedPathForCompare(trimmed);
            if (removed.contains(normalized)) {
                continue;
            }
            removed.insert(normalized);
            if (QFileInfo::exists(trimmed)) {
                ok = QFile::remove(trimmed) && ok;
            }
        }
        return ok;
    }

    void resetHistoryTabLoadedState()
    {
        if (!m_historyTabs) {
            return;
        }
        for (int i = 0; i < m_historyTabs->count(); ++i) {
            QWidget *page = m_historyTabs->widget(i);
            if (page) {
                page->setProperty("historyLoaded", false);
            }
        }
    }

    void rebuildHistoryTabPlaceholders(int previousIndex, const QString &message)
    {
        if (!m_historyTabs) {
            return;
        }
        QObject::disconnect(m_historyTabs, SIGNAL(currentChanged(int)), this, nullptr);
        while (m_historyTabs->count() > 0) {
            QWidget *page = m_historyTabs->widget(0);
            m_historyTabs->removeTab(0);
            delete page;
        }

        const QVector<HistoryTabDef> modes = historyTabModes();
        for (const HistoryTabDef &mode : modes) {
            m_historyTabs->addTab(historyTabPlaceholder(mode.id, message), mode.title);
        }

        connect(m_historyTabs, &QTabWidget::currentChanged, this, [this](int index) {
            populateHistoryTab(index);
        });

        if (m_historyTabs->count() > 0) {
            const int index = qBound(0, previousIndex, m_historyTabs->count() - 1);
            m_historyTabs->setCurrentIndex(index);
        }
    }

    void refreshHistoryTabs(bool forceReload = false)
    {
        if (!m_historyTabs) {
            return;
        }
        const int previousIndex = qMax(0, m_historyTabs->currentIndex());

        if (m_historyCacheValid && !forceReload) {
            if (m_historyTabs->count() == 0) {
                rebuildHistoryTabPlaceholders(previousIndex, QString());
            }
            if (m_historyTabs->count() > 0) {
                populateHistoryTab(qMin(previousIndex, m_historyTabs->count() - 1));
            }
            return;
        }

        if (m_historyLoadInProgress && !forceReload) {
            if (m_historyTabs->count() == 0) {
                rebuildHistoryTabPlaceholders(previousIndex, tr8("正在加载历史记录..."));
            }
            return;
        }

        rebuildHistoryTabPlaceholders(previousIndex, tr8("正在加载历史记录..."));
        startHistoryLoad(qMin(previousIndex, m_historyTabs->count() - 1));
    }

    void startHistoryLoad(int targetIndex)
    {
        const int generation = ++m_historyLoadGeneration;
        m_historyLoadInProgress = true;
        const QString recordsPath = m_settings->recordDirectoryPath();
        auto *watcher = new QFutureWatcher<QVector<HistoryEntry>>(this);
        connect(watcher, &QFutureWatcher<QVector<HistoryEntry>>::finished, this, [this, watcher, generation, targetIndex]() {
            if (generation == m_historyLoadGeneration) {
                m_historyLoadInProgress = false;
                m_historyEntriesCache = watcher->result();
                m_historyCacheValid = true;
                resetHistoryTabLoadedState();
                populateHistoryTab(qMin(targetIndex, m_historyTabs ? m_historyTabs->count() - 1 : 0));
            }
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([recordsPath]() {
            return HubWindow::loadHistoryEntriesFromPath(recordsPath);
        }));
    }

    void showHistoryDetail(const HistoryEntry &entry)
    {
        QDialog dialog(this);
        dialog.setWindowTitle(tr8("记录详情"));
        dialog.setMinimumSize(820, 620);
        dialog.setStyleSheet(QStringLiteral(
            "QDialog { background: #f6f7f9; }"
            "QLabel { color: #111827; }"
            "QTextEdit { background: #ffffff; border: 1px solid #dde2ea; border-radius: 8px; padding: 10px; }"
        ));

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(12);

        auto *title = new QLabel(entry.mode + tr8("记录详情"));
        title->setFont(appFont(18, QFont::DemiBold));
        title->setMinimumHeight(34);
        layout->addWidget(title);

        auto *metaCard = new QFrame;
        metaCard->setObjectName(QStringLiteral("card"));
        metaCard->setStyleSheet(cardStyle());
        auto *meta = new QGridLayout(metaCard);
        meta->setContentsMargins(14, 12, 14, 12);
        meta->setHorizontalSpacing(18);
        meta->setVerticalSpacing(10);

        auto addMeta = [this, meta](int row, int column, const QString &name, const QString &value) {
            auto *nameLabel = new QLabel(name);
            nameLabel->setFont(appFont(9, QFont::DemiBold));
            nameLabel->setStyleSheet(QStringLiteral("color: #667085;"));
            auto *valueLabel = new QLabel(value.trimmed().isEmpty() ? tr8("无") : value);
            valueLabel->setWordWrap(true);
            valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            valueLabel->setFont(appFont(10, QFont::DemiBold));
            valueLabel->setMinimumHeight(26);
            meta->addWidget(nameLabel, row, column * 2);
            meta->addWidget(valueLabel, row, column * 2 + 1);
        };

        const bool audioExists = !entry.audio.trimmed().isEmpty() && QFileInfo::exists(entry.audio);
        const QString audioState = entry.audio.trimmed().isEmpty()
            ? tr8("本次没有录音")
            : (audioExists ? tr8("文件存在") : tr8("文件不存在或已被删除"));
        addMeta(0, 0, tr8("功能"), entry.mode);
        addMeta(0, 1, tr8("时间"), historyTimeText(entry.time));
        addMeta(1, 0, tr8("耗时"), historyElapsedText(entry.elapsedMs));
        addMeta(1, 1, tr8("使用模型"), historyModelText(entry));
        addMeta(2, 0, tr8("状态"), entry.draft ? tr8("草稿") : (entry.error.trimmed().isEmpty() ? tr8("完成") : tr8("有错误")));
        addMeta(2, 1, tr8("录音"), audioState);
        layout->addWidget(metaCard);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background: transparent; }"
        ));

        auto *content = new QWidget;
        auto *contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(0, 0, 10, 0);
        contentLayout->setSpacing(12);

        auto addTextSection = [this, contentLayout](const QString &sectionTitle, const QString &text, int minimumHeight) {
            auto *frame = new QFrame;
            frame->setObjectName(QStringLiteral("card"));
            frame->setStyleSheet(cardStyle());
            auto *frameLayout = new QVBoxLayout(frame);
            frameLayout->setContentsMargins(14, 12, 14, 14);
            frameLayout->setSpacing(8);

            auto *label = new QLabel(sectionTitle);
            label->setFont(appFont(12, QFont::DemiBold));
            label->setMinimumHeight(28);
            frameLayout->addWidget(label);

            auto *editor = new QTextEdit;
            editor->setReadOnly(true);
            editor->setLineWrapMode(QTextEdit::WidgetWidth);
            editor->setPlainText(text.trimmed().isEmpty() ? tr8("无") : text);
            editor->setMinimumHeight(minimumHeight);
            frameLayout->addWidget(editor);
            contentLayout->addWidget(frame);
        };

        addTextSection(tr8("识别文本"), historyRecognizedText(entry), 110);
        addTextSection(tr8("输入内容"), entry.input, 130);
        addTextSection(tr8("模型输出"), entry.output, 170);
        addTextSection(tr8("错误"), entry.error.trimmed().isEmpty() ? tr8("无") : entry.error, 90);
        addTextSection(tr8("录音文件"), entry.audio.trimmed().isEmpty() ? tr8("本次没有录音") : entry.audio, 80);
        contentLayout->addStretch();

        scroll->setWidget(content);
        layout->addWidget(scroll, 1);

        auto *buttons = new QHBoxLayout;
        auto *copyAll = new QPushButton(tr8("复制详情"));
        copyAll->setMinimumSize(92, 38);
        copyAll->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *copy = new QPushButton(tr8("复制输出"));
        copy->setMinimumSize(92, 38);
        copy->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        auto *openAudio = new QPushButton(tr8("播放录音"));
        openAudio->setMinimumSize(92, 38);
        openAudio->setEnabled(audioExists);
        openAudio->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *close = new QPushButton(tr8("关闭"));
        close->setMinimumSize(78, 38);
        close->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        buttons->addStretch();
        buttons->addWidget(copyAll);
        buttons->addWidget(copy);
        buttons->addWidget(openAudio);
        buttons->addWidget(close);
        layout->addLayout(buttons);

        connect(copyAll, &QPushButton::clicked, &dialog, [this, entry]() {
            QApplication::clipboard()->setText(historyDetailPlainText(entry));
        });
        connect(copy, &QPushButton::clicked, &dialog, [entry]() {
            QApplication::clipboard()->setText(entry.output.isEmpty() ? entry.input : entry.output);
        });
        connect(openAudio, &QPushButton::clicked, &dialog, [entry]() {
            if (!entry.audio.trimmed().isEmpty() && QFileInfo::exists(entry.audio)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(entry.audio));
            }
        });
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        dialog.exec();
    }

    QVector<PromptTargetInfo> promptTargets() const
    {
        return sharedPromptTargets(m_settings);
    }

    PromptTargetInfo promptTargetForId(const QString &id) const
    {
        return sharedPromptTargetForId(m_settings, id);
    }

    void refreshPromptSelector()
    {
        if (!m_promptSelector) {
            return;
        }
        const QString previous = m_promptSelector->currentData().toString();
        m_promptSelector->blockSignals(true);
        m_promptSelector->clear();
        for (const PromptTargetInfo &target : promptTargets()) {
            m_promptSelector->addItem(target.title, target.id);
        }
        const int index = m_promptSelector->findData(previous);
        if (index >= 0) {
            m_promptSelector->setCurrentIndex(index);
        }
        m_promptSelector->blockSignals(false);
        loadPromptEditor();
    }

    void loadPromptEditor()
    {
        if (!m_promptSelector || !m_promptEditor) {
            return;
        }
        const PromptTargetInfo target = promptTargetForId(m_promptSelector->currentData().toString());
        m_promptEditor->setPlainText(sharedPromptText(m_settings, target));
        updatePromptEditorLock();
    }

    void updatePromptEditorLock()
    {
        const bool locked = m_settings->promptLocked();
        if (m_promptLock && m_promptLock->isChecked() != locked) {
            m_promptLock->blockSignals(true);
            m_promptLock->setChecked(locked);
            m_promptLock->blockSignals(false);
        }
        if (m_promptEditor) {
            m_promptEditor->setDisabled(locked);
        }
        if (m_promptSaveButton) {
            m_promptSaveButton->setDisabled(locked);
        }
    }

    void savePromptFromEditor()
    {
        if (!m_promptSelector || !m_promptEditor) {
            return;
        }
        if (m_settings->promptLocked()) {
            showAttentionInformation(this, tr8("提示词已锁定"), tr8("请先取消锁定后再修改提示词。"));
            return;
        }

        const PromptTargetInfo target = promptTargetForId(m_promptSelector->currentData().toString());
        QString error;
        if (!saveSharedPromptText(m_settings, target, m_promptEditor->toPlainText(), &error)) {
            showAttentionWarning(this, tr8("保存失败"), error.isEmpty() ? tr8("无法保存提示词。") : error);
            return;
        }

        if (m_onSettingsChanged) {
            m_onSettingsChanged();
        }
        showAttentionInformation(this, tr8("已保存"), tr8("提示词已保存。"));
    }

    void addHubCustomFunction()
    {
        CustomFunctionDef fn;
        fn.id = m_settings->nextCustomFunctionId();
        fn.name = tr8("自定义功能 ") + fn.id.mid(7);
        fn.shortcut = m_settings->suggestedCustomShortcut();
        fn.model = QStringLiteral("deepseek-v4-flash");
        fn.outputMode = outputModePopup();
        fn.useSelection = true;
        fn.useVoice = true;
        fn.floatingBarSeconds = defaultFloatingBarSeconds();
        fn.resultPopupSeconds = defaultResultPopupSeconds();
        fn.countdownSeconds = defaultCountdownSeconds();
        fn.recordingBeepEnabled = true;
        fn.recordingBeepPath.clear();
        fn.prompt = tr8("请根据选中文本和我的语音要求完成任务，输出可以直接使用的结果。");
        m_settings->addCustomFunction(fn);
        saveHubSettings();
        refreshCustomFunctionsPage();
        populateModeGrid();
        refreshPromptSelector();
        if (!showFunctionEditorDialog(fn.id, fn.name, true, fn)) {
            m_settings->removeCustomFunction(fn.id);
            saveHubSettings();
            refreshCustomFunctionsPage();
            populateModeGrid();
            refreshPromptSelector();
        }
    }

    QComboBox *hubModelCombo(const QString &currentModel)
    {
        auto *box = new QComboBox;
        box->setFixedHeight(36);
        for (const ModelOption &option : modelOptions()) {
            box->addItem(option.title, option.id);
        }
        const int modelIndex = box->findData(normalizeModelId(currentModel, QStringLiteral("deepseek-v4-flash")));
        if (modelIndex >= 0) {
            box->setCurrentIndex(modelIndex);
        }
        box->setStyleSheet(QStringLiteral(
            "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
        ));
        return box;
    }

    QComboBox *hubOutputModeCombo(const QString &currentMode)
    {
        auto *box = new QComboBox;
        box->setFixedHeight(36);
        box->addItem(outputModeTitle(outputModeAutoWrite()), outputModeAutoWrite());
        box->addItem(outputModeTitle(outputModePopup()), outputModePopup());
        const int modeIndex = box->findData(normalizeOutputMode(currentMode, outputModePopup()));
        if (modeIndex >= 0) {
            box->setCurrentIndex(modeIndex);
        }
        box->setStyleSheet(QStringLiteral(
            "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
        ));
        return box;
    }

    QSpinBox *hubDisplayTimeSpinBox(int seconds, bool allowManualClose, const QString &zeroText = QString())
    {
        auto *box = new QSpinBox;
        const bool allowZero = allowManualClose || !zeroText.trimmed().isEmpty();
        box->setRange(allowZero ? 0 : 1, allowManualClose ? 600 : 60);
        box->setSuffix(tr8(" 秒"));
        if (allowManualClose) {
            box->setSpecialValueText(tr8("手动关闭"));
        } else if (!zeroText.trimmed().isEmpty()) {
            box->setSpecialValueText(zeroText.trimmed());
        }
        box->setValue(seconds);
        box->setFixedSize(130, 36);
        box->setStyleSheet(QStringLiteral(
            "QSpinBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 4px 8px; }"
        ));
        return box;
    }

    QString functionSummaryText(const QString &id, const QString &shortcut) const
    {
        const QString floatingTime = m_settings->floatingBarSecondsFor(id) == 0
            ? tr8("不调用")
            : QString::number(m_settings->floatingBarSecondsFor(id)) + tr8(" 秒");
        const QString resultTime = m_settings->resultPopupSecondsFor(id) == 0
            ? tr8("手动关闭")
            : QString::number(m_settings->resultPopupSecondsFor(id)) + tr8(" 秒");
        const QString countdownTime = m_settings->countdownSecondsFor(id) == 0
            ? tr8("不调用")
            : QString::number(m_settings->countdownSecondsFor(id)) + tr8(" 秒");
        return displayShortcut(shortcut)
            + tr8(" · 模型：") + modelTitle(m_settings->modelFor(id))
            + tr8(" · 输入：") + inputModeSummary(id)
            + tr8(" · 展现：") + outputModeTitle(m_settings->outputModeFor(id))
            + tr8(" · 浮动条：") + floatingTime
            + tr8(" · 结果小框：") + resultTime
            + tr8(" · 倒计时：") + countdownTime
            + tr8(" · 提示音：") + (m_settings->recordingBeepEnabledFor(id) ? tr8("开启") : tr8("关闭"));
    }

    QWidget *dialogSection(const QString &title, QVBoxLayout **contentOut = nullptr)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(12);

        auto *name = new QLabel(title);
        name->setFont(appFont(13, QFont::DemiBold));
        layout->addWidget(name);

        auto *content = new QVBoxLayout;
        content->setContentsMargins(0, 0, 0, 0);
        content->setSpacing(10);
        layout->addLayout(content);
        if (contentOut) {
            *contentOut = content;
        }
        return frame;
    }

    bool showFunctionEditorDialog(const QString &id, const QString &title, bool custom, const CustomFunctionDef &fn)
    {
        QDialog dialog(this);
        dialog.setWindowTitle(custom ? tr8("编辑自定义功能") : tr8("编辑内置功能"));
        dialog.setMinimumSize(820, 680);
        dialog.setFont(appFont());
        dialog.setStyleSheet(QStringLiteral(
            "QDialog { background: #f6f7f9; }"
            "QLabel { color: #111827; }"
        ));

        auto *root = new QVBoxLayout(&dialog);
        root->setContentsMargins(22, 20, 22, 18);
        root->setSpacing(14);

        auto *header = new QHBoxLayout;
        auto *titleBox = new QVBoxLayout;
        auto *dialogTitle = new QLabel(title);
        dialogTitle->setFont(appFont(20, QFont::DemiBold));
        auto *summary = new QLabel(functionSummaryText(id, custom ? fn.shortcut : m_settings->hotkey(id)));
        summary->setWordWrap(true);
        summary->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));
        titleBox->addWidget(dialogTitle);
        titleBox->addWidget(summary);

        auto *type = new QLabel(custom ? tr8("自定义") : tr8("内置"));
        type->setAlignment(Qt::AlignCenter);
        type->setMinimumSize(72, 32);
        type->setStyleSheet(QStringLiteral(
            "QLabel { background: #eef2ff; color: #1d4ed8; border-radius: 6px; font-weight: 600; }"
        ));
        header->addLayout(titleBox, 1);
        header->addWidget(type, 0, Qt::AlignTop);
        root->addLayout(header);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background: transparent; }"
        ));
        auto *holder = new QWidget;
        auto *content = new QVBoxLayout(holder);
        content->setContentsMargins(0, 0, 10, 0);
        content->setSpacing(12);

        QLineEdit *nameEdit = nullptr;
        QVBoxLayout *basicContent = nullptr;
        auto *basicSection = dialogSection(tr8("基础信息"), &basicContent);
        auto *basicForm = new QGridLayout;
        basicForm->setHorizontalSpacing(12);
        basicForm->setVerticalSpacing(10);
        int basicRow = 0;
        if (custom) {
            nameEdit = new QLineEdit(fn.name);
            nameEdit->setMinimumHeight(36);
            basicForm->addWidget(new QLabel(tr8("名称")), basicRow, 0);
            basicForm->addWidget(nameEdit, basicRow, 1);
            ++basicRow;
        } else {
            auto *fixedName = new QLabel(title);
            fixedName->setMinimumHeight(36);
            fixedName->setStyleSheet(QStringLiteral(
                "QLabel { background: #f9fafb; border: 1px solid #eef0f4; border-radius: 6px; padding: 7px 10px; }"
            ));
            basicForm->addWidget(new QLabel(tr8("名称")), basicRow, 0);
            basicForm->addWidget(fixedName, basicRow, 1);
            ++basicRow;
        }

        auto *shortcutEdit = new QKeySequenceEdit(QKeySequence(custom ? fn.shortcut : m_settings->hotkey(id)));
        shortcutEdit->setMinimumHeight(36);
        basicForm->addWidget(new QLabel(tr8("快捷键")), basicRow, 0);
        basicForm->addWidget(shortcutEdit, basicRow, 1);
        basicForm->setColumnStretch(1, 1);
        basicContent->addLayout(basicForm);
        content->addWidget(basicSection);

        QVBoxLayout *modeContent = nullptr;
        auto *modeSection = dialogSection(tr8("模型和输入"), &modeContent);
        auto *modeForm = new QGridLayout;
        modeForm->setHorizontalSpacing(12);
        modeForm->setVerticalSpacing(10);
        auto *modelBox = hubModelCombo(m_settings->modelFor(id));
        auto *outputBox = hubOutputModeCombo(m_settings->outputModeFor(id));
        auto *useSelection = new QCheckBox(tr8("读取鼠标选中的文字"));
        useSelection->setChecked(m_settings->useSelectionFor(id));
        useSelection->setFont(appFont(10, QFont::DemiBold));
        auto *useVoice = new QCheckBox(tr8("使用语音输入"));
        useVoice->setChecked(m_settings->useVoiceFor(id));
        useVoice->setFont(appFont(10, QFont::DemiBold));
        auto *inputBox = new QWidget;
        auto *inputLayout = new QHBoxLayout(inputBox);
        inputLayout->setContentsMargins(0, 0, 0, 0);
        inputLayout->setSpacing(14);
        inputLayout->addWidget(useSelection);
        inputLayout->addWidget(useVoice);
        inputLayout->addStretch();
        modeForm->addWidget(new QLabel(tr8("模型")), 0, 0);
        modeForm->addWidget(modelBox, 0, 1);
        modeForm->addWidget(new QLabel(tr8("展现方式")), 0, 2);
        modeForm->addWidget(outputBox, 0, 3);
        modeForm->addWidget(new QLabel(tr8("输入方式")), 1, 0);
        modeForm->addWidget(inputBox, 1, 1, 1, 3);
        modeForm->setColumnStretch(1, 1);
        modeForm->setColumnStretch(3, 1);
        modeContent->addLayout(modeForm);
        content->addWidget(modeSection);

        QVBoxLayout *timeContent = nullptr;
        auto *timeSection = dialogSection(tr8("显示时间"), &timeContent);
        auto *timeForm = new QGridLayout;
        timeForm->setHorizontalSpacing(12);
        timeForm->setVerticalSpacing(10);
        auto *floatingTime = hubDisplayTimeSpinBox(m_settings->floatingBarSecondsFor(id), false, tr8("不调用"));
        auto *popupTime = hubDisplayTimeSpinBox(m_settings->resultPopupSecondsFor(id), true);
        auto *countdownTime = hubDisplayTimeSpinBox(m_settings->countdownSecondsFor(id), false, tr8("不调用"));
        auto *beepEnabled = new QCheckBox;
        beepEnabled->setChecked(m_settings->recordingBeepEnabledFor(id));
        beepEnabled->setFixedWidth(28);
        auto *beepPathEdit = new QLineEdit(m_settings->recordingBeepPathFor(id));
        beepPathEdit->setReadOnly(true);
        beepPathEdit->setMinimumHeight(36);
        beepPathEdit->setPlaceholderText(tr8("系统提示音"));
        beepPathEdit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 6px; padding: 6px 10px; }"
        ));
        auto *chooseBeep = new QPushButton(tr8("选择声音"));
        chooseBeep->setFixedHeight(36);
        chooseBeep->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *clearBeep = new QPushButton(tr8("清除"));
        clearBeep->setFixedHeight(36);
        clearBeep->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *beepBox = new QWidget;
        auto *beepLayout = new QHBoxLayout(beepBox);
        beepLayout->setContentsMargins(0, 0, 0, 0);
        beepLayout->setSpacing(8);
        beepLayout->addWidget(beepEnabled);
        beepLayout->addWidget(beepPathEdit, 1);
        beepLayout->addWidget(chooseBeep);
        beepLayout->addWidget(clearBeep);
        const auto refreshBeepControls = [beepEnabled, beepPathEdit, chooseBeep, clearBeep](bool) {
            const bool enabled = beepEnabled->isChecked();
            beepPathEdit->setEnabled(enabled);
            chooseBeep->setEnabled(enabled);
            clearBeep->setEnabled(enabled);
        };
        refreshBeepControls(beepEnabled->isChecked());
        connect(beepEnabled, &QCheckBox::toggled, &dialog, refreshBeepControls);
        connect(chooseBeep, &QPushButton::clicked, &dialog, [beepPathEdit]() {
            const QString path = QFileDialog::getOpenFileName(
                nullptr,
                tr8("选择录音提示音"),
                QString(),
                tr8("声音文件 (*.wav);;所有文件 (*.*)")
            );
            if (!path.trimmed().isEmpty()) {
                beepPathEdit->setText(QDir::cleanPath(path));
            }
        });
        connect(clearBeep, &QPushButton::clicked, &dialog, [beepPathEdit]() {
            beepPathEdit->clear();
        });
        timeForm->addWidget(new QLabel(tr8("浮动条")), 0, 0);
        timeForm->addWidget(floatingTime, 0, 1);
        timeForm->addWidget(new QLabel(tr8("结果小框")), 0, 2);
        timeForm->addWidget(popupTime, 0, 3);
        timeForm->addWidget(new QLabel(tr8("录音倒计时")), 1, 0);
        timeForm->addWidget(countdownTime, 1, 1);
        timeForm->addWidget(new QLabel(tr8("录音提示音")), 1, 2);
        timeForm->addWidget(beepBox, 1, 3);
        timeForm->setColumnStretch(1, 1);
        timeForm->setColumnStretch(3, 1);
        timeContent->addLayout(timeForm);
        content->addWidget(timeSection);

        QVBoxLayout *promptContent = nullptr;
        auto *promptSection = dialogSection(tr8("提示词"), &promptContent);
        auto *promptTools = new QHBoxLayout;
        promptTools->setContentsMargins(0, 0, 0, 0);
        promptTools->setSpacing(10);
        auto *promptBox = new QComboBox;
        promptBox->setMinimumHeight(36);
        promptBox->setMinimumWidth(320);
        for (const PromptTargetInfo &target : promptTargets()) {
            promptBox->addItem(target.title, target.id);
        }
        const int promptIndex = promptBox->findData(id);
        if (promptIndex >= 0) {
            promptBox->setCurrentIndex(promptIndex);
        }
        promptBox->setStyleSheet(QStringLiteral(
            "QComboBox {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 6px;"
            "  padding: 6px 10px;"
            "  color: #111827;"
            "}"
            "QComboBox QAbstractItemView {"
            "  background: #ffffff;"
            "  border: 1px solid #9ca3af;"
            "  selection-background-color: #2563eb;"
            "  selection-color: #ffffff;"
            "  outline: 0;"
            "}"
        ));
        promptTools->addWidget(promptBox);
        promptTools->addStretch();
        promptContent->addLayout(promptTools);

        auto *promptEditor = new QTextEdit;
        promptEditor->setMinimumHeight(220);
        promptEditor->setPlainText(sharedPromptText(m_settings, promptTargetForId(promptBox->currentData().toString())));
        promptEditor->setDisabled(m_settings->promptLocked());
        promptEditor->setStyleSheet(QStringLiteral(
            "QTextEdit { background: #ffffff; border: 1px solid #dde2ea; border-radius: 8px; padding: 10px; }"
        ));
        promptContent->addWidget(promptEditor);
        connect(promptBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), &dialog, [this, promptBox, promptEditor]() {
            promptEditor->setPlainText(sharedPromptText(m_settings, promptTargetForId(promptBox->currentData().toString())));
        });
        content->addWidget(promptSection);

        content->addStretch();
        scroll->setWidget(holder);
        root->addWidget(scroll, 1);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *cancel = new QPushButton(tr8("取消"));
        cancel->setFixedHeight(38);
        cancel->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
        auto *save = new QPushButton(tr8("保存"));
        save->setFixedHeight(38);
        save->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
        buttons->addWidget(cancel);
        buttons->addWidget(save);
        root->addLayout(buttons);

        connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);

        connect(save, &QPushButton::clicked, &dialog, [this, &dialog, id, title, custom, fn, nameEdit, shortcutEdit, modelBox, outputBox, useSelection, useVoice, floatingTime, popupTime, countdownTime, beepEnabled, beepPathEdit, promptBox, promptEditor]() {
            const QString shortcut = shortcutEdit->keySequence().toString(QKeySequence::PortableText).trimmed();
            const QString name = custom && nameEdit ? nameEdit->text().trimmed() : title;
            const QString selectedPromptId = promptBox->currentData().toString();
            if (name.isEmpty()) {
                showAttentionWarning(this, tr8("名称不能为空"), tr8("请填写功能名称。"));
                return;
            }
            if (shortcut.isEmpty()) {
                showAttentionWarning(this, tr8("快捷键不能为空"), tr8("请设置快捷键。"));
                return;
            }
            QString otherTitle;
            if (m_settings->conflictsWithOther(id, shortcut, &otherTitle)) {
                showAttentionWarning(this, tr8("快捷键冲突"), tr8("这个快捷键已经被“") + otherTitle + tr8("”使用。"));
                return;
            }
            if (!useSelection->isChecked() && !useVoice->isChecked()) {
                showAttentionInformation(this, tr8("需要输入方式"), tr8("至少需要启用“读取鼠标选中的文字”或“使用语音输入”中的一种。"));
                return;
            }

            if (custom) {
                CustomFunctionDef updated = fn;
                updated.name = name;
                updated.shortcut = shortcut;
                updated.model = modelBox->currentData().toString();
                updated.outputMode = outputBox->currentData().toString();
                updated.useSelection = useSelection->isChecked();
                updated.useVoice = useVoice->isChecked();
                updated.floatingBarSeconds = floatingTime->value();
                updated.resultPopupSeconds = popupTime->value();
                updated.countdownSeconds = countdownTime->value();
                updated.recordingBeepEnabled = beepEnabled->isChecked();
                updated.recordingBeepPath = beepPathEdit->text().trimmed();
                if (!m_settings->promptLocked() && selectedPromptId == id) {
                    updated.prompt = promptEditor->toPlainText();
                }
                m_settings->updateCustomFunction(updated);
            } else {
                m_settings->setHotkey(id, shortcut);
                m_settings->setModelFor(id, modelBox->currentData().toString());
                m_settings->setOutputModeFor(id, outputBox->currentData().toString());
                m_settings->setUseSelectionFor(id, useSelection->isChecked());
                m_settings->setUseVoiceFor(id, useVoice->isChecked());
                m_settings->setFloatingBarSecondsFor(id, floatingTime->value());
                m_settings->setResultPopupSecondsFor(id, popupTime->value());
                m_settings->setCountdownSecondsFor(id, countdownTime->value());
                m_settings->setRecordingBeepEnabledFor(id, beepEnabled->isChecked());
                m_settings->setRecordingBeepPathFor(id, beepPathEdit->text().trimmed());
            }

            if (!m_settings->promptLocked() && !(custom && selectedPromptId == id)) {
                QString error;
                if (!saveSharedPromptText(m_settings, promptTargetForId(selectedPromptId), promptEditor->toPlainText(), &error)) {
                    showAttentionWarning(this, tr8("保存失败"), error.isEmpty() ? tr8("无法保存提示词。") : error);
                    return;
                }
            }

            saveHubSettings();
            if (m_settingsPanel) {
                m_settingsPanel->refreshFromSettings();
            }
            refreshCustomFunctionsPage();
            populateModeGrid();
            refreshPromptSelector();
            showAttentionInformation(this, tr8("已保存"), tr8("功能配置已保存。"));
            dialog.accept();
        });

        return dialog.exec() == QDialog::Accepted;
    }

    QWidget *functionSummaryCard(const QString &id, const QString &title, const QString &shortcut, bool custom, const CustomFunctionDef &fn)
    {
        auto *frame = new HistoryRowFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        frame->setClickCallback([this, id, title, custom, fn]() {
            showFunctionEditorDialog(id, title, custom, fn);
        });
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);

        auto *top = new QHBoxLayout;
        auto *name = new QLabel(title);
        name->setFont(appFont(13, QFont::DemiBold));
        auto *edit = new QPushButton(tr8("编辑"));
        edit->setFont(appFont(10, QFont::DemiBold));
        edit->setFixedSize(92, 42);
        edit->setStyleSheet(compactButtonStyle(QStringLiteral("#111827")));
        top->addWidget(name, 1);
        top->addWidget(edit, 0, Qt::AlignTop);
        QPushButton *remove = nullptr;
        if (custom) {
            remove = new QPushButton(tr8("删除"));
            remove->setFont(appFont(10, QFont::DemiBold));
            remove->setFixedSize(92, 42);
            remove->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
            top->addWidget(remove, 0, Qt::AlignTop);
        }
        layout->addLayout(top);

        auto *meta = new QLabel(functionSummaryText(id, shortcut));
        meta->setWordWrap(true);
        meta->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));
        layout->addWidget(meta);

        connect(edit, &QPushButton::clicked, this, [this, id, title, custom, fn]() {
            showFunctionEditorDialog(id, title, custom, fn);
        });
        if (remove) {
            connect(remove, &QPushButton::clicked, this, [this, fn]() {
                if (QMessageBox::question(this, tr8("删除自定义功能"), tr8("确定删除“") + fn.name + tr8("”？")) != QMessageBox::Yes) {
                    return;
                }
                m_settings->removeCustomFunction(fn.id);
                saveHubSettings();
                refreshCustomFunctionsPage();
                populateModeGrid();
                refreshPromptSelector();
            });
        }
        return frame;
    }

    void refreshCustomFunctionsPage()
    {
        if (!m_hubCustomListLayout) {
            return;
        }
        clearLayout(m_hubCustomListLayout);
        for (const HotkeyDef &def : coreFunctionDefs()) {
            m_hubCustomListLayout->addWidget(functionSummaryCard(def.id, def.title, m_settings->hotkey(def.id), false, CustomFunctionDef()));
        }
        const QVector<CustomFunctionDef> functions = m_settings->customFunctions();
        for (const CustomFunctionDef &fn : functions) {
            m_hubCustomListLayout->addWidget(functionSummaryCard(fn.id, fn.name, fn.shortcut, true, fn));
        }
        m_hubCustomListLayout->addStretch();
    }

    void clearLayout(QLayout *layout)
    {
        while (QLayoutItem *item = layout->takeAt(0)) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            if (item->layout()) {
                clearLayout(item->layout());
            }
            delete item;
        }
    }

    void populateModeGrid()
    {
        clearLayout(m_modeGridLayout);
        m_modeShortcutLabels.clear();

        struct CardData
        {
            QString id;
            QString title;
            QString shortcut;
            QString model;
            QString outputMode;
            QString description;
            QString accent;
        };

        QVector<CardData> cards;
        cards.append({QStringLiteral("dictate"), tr8("听写"), m_settings->hotkey(QStringLiteral("dictate")), m_settings->modelFor(QStringLiteral("dictate")), m_settings->outputModeFor(QStringLiteral("dictate")), tr8("按当前输入方式设置整理内容，并按设置展现结果。"), QStringLiteral("#2563eb")});
        cards.append({QStringLiteral("translate"), tr8("翻译"), m_settings->hotkey(QStringLiteral("translate")), m_settings->modelFor(QStringLiteral("translate")), m_settings->outputModeFor(QStringLiteral("translate")), tr8("按当前输入方式设置读取内容并翻译。"), QStringLiteral("#059669")});
        cards.append({QStringLiteral("ask"), tr8("问答"), m_settings->hotkey(QStringLiteral("ask")), m_settings->modelFor(QStringLiteral("ask")), m_settings->outputModeFor(QStringLiteral("ask")), tr8("按当前输入方式设置读取上下文和问题。"), QStringLiteral("#7c3aed")});

        const QStringList accents = QStringList() << QStringLiteral("#db2777") << QStringLiteral("#ea580c") << QStringLiteral("#0891b2") << QStringLiteral("#4f46e5");
        int accentIndex = 0;
        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            cards.append({fn.id, fn.name, fn.shortcut, fn.model, m_settings->outputModeFor(fn.id), tr8("使用独立提示词和当前输入方式设置处理内容。"), accents.at(accentIndex % accents.size())});
            ++accentIndex;
        }

        QVector<CardData> orderedCards;
        const QStringList order = m_settings->functionOrderIds();
        for (const QString &id : order) {
            for (const CardData &card : cards) {
                if (card.id == id) {
                    orderedCards.append(card);
                    break;
                }
            }
        }
        for (const CardData &card : cards) {
            bool exists = false;
            for (const CardData &ordered : orderedCards) {
                if (ordered.id == card.id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                orderedCards.append(card);
            }
        }
        cards = orderedCards;

        for (int i = 0; i < cards.size(); ++i) {
            const CardData card = cards.at(i);
            m_modeGridLayout->addWidget(modeCard(card.id, card.title, card.shortcut, card.model, card.outputMode, card.description, card.accent), i / 3, i % 3);
        }
    }

    QString inputModeSummary(const QString &id) const
    {
        QStringList inputs;
        if (m_settings->useSelectionFor(id)) {
            inputs.append(tr8("选中文字"));
        }
        if (m_settings->useVoiceFor(id)) {
            inputs.append(tr8("语音"));
        }
        return inputs.isEmpty() ? tr8("未启用") : inputs.join(tr8(" + "));
    }

    QWidget *modeCard(const QString &id, const QString &title, const QString &shortcut, const QString &model, const QString &outputMode, const QString &description, const QString &accent)
    {
        Q_UNUSED(description);
        auto *frame = new ModeCardFrame(id);
        frame->setObjectName(QStringLiteral("card"));
        frame->setFixedHeight(210);
        frame->setStyleSheet(cardStyle());
        frame->setDropCallback([this](const QString &sourceId, const QString &targetId, bool dropAfter) {
            reorderModeCard(sourceId, targetId, dropAfter);
        });
        frame->setDoubleClickCallback([this, id, title]() {
            bool custom = false;
            CustomFunctionDef customFunction;
            for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
                if (fn.id == id) {
                    custom = true;
                    customFunction = fn;
                    break;
                }
            }
            showFunctionEditorDialog(id, title, custom, customFunction);
        });

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(6);

        auto *dot = new QLabel;
        dot->setFixedSize(26, 5);
        dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 2px;").arg(accent));

        auto *topRow = new QHBoxLayout;
        topRow->setContentsMargins(0, 0, 0, 0);
        topRow->setSpacing(6);
        topRow->addWidget(dot);
        topRow->addStretch();

        auto *name = new QLabel(title);
        name->setFont(appFont(13, QFont::DemiBold));
        name->setStyleSheet(QStringLiteral("background: transparent; color: #111827;"));
        name->setWordWrap(false);
        name->setMinimumHeight(32);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        auto *key = new QLabel(displayShortcut(shortcut));
        key->setStyleSheet(QStringLiteral("background: transparent; color: #344054; font-weight: 600;"));
        key->setMinimumHeight(24);
        key->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_modeShortcutLabels.insert(id, key);

        auto *modelLabel = new QLabel(tr8("模型：") + modelTitle(model));
        modelLabel->setWordWrap(false);
        modelLabel->setFont(appFont(9, QFont::DemiBold));
        modelLabel->setStyleSheet(QStringLiteral("background: transparent; color: #047857; font-weight: 600;"));
        modelLabel->setMinimumHeight(22);
        modelLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto *outputLabel = new QLabel(tr8("展现：") + outputModeTitle(outputMode));
        outputLabel->setWordWrap(false);
        outputLabel->setFont(appFont(9, QFont::DemiBold));
        outputLabel->setStyleSheet(QStringLiteral("background: transparent; color: #047857; font-weight: 600;"));
        outputLabel->setMinimumHeight(22);
        outputLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        auto *inputLabel = new QLabel(tr8("输入：") + inputModeSummary(id));
        inputLabel->setWordWrap(false);
        inputLabel->setFont(appFont(9, QFont::DemiBold));
        inputLabel->setStyleSheet(QStringLiteral("background: transparent; color: #047857; font-weight: 600;"));
        inputLabel->setMinimumHeight(22);
        inputLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        layout->addLayout(topRow);
        layout->addWidget(name);
        layout->addWidget(key);
        layout->addWidget(modelLabel);
        layout->addWidget(inputLabel);
        layout->addWidget(outputLabel);
        layout->addStretch();
        for (QWidget *child : frame->findChildren<QWidget *>()) {
            child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }
        return frame;
    }

    void reorderModeCard(const QString &sourceId, const QString &targetId, bool dropAfter)
    {
        QStringList ids = m_settings->functionOrderIds();
        const int sourceIndex = ids.indexOf(sourceId);
        int targetIndex = ids.indexOf(targetId);
        if (sourceIndex < 0 || targetIndex < 0 || sourceIndex == targetIndex) {
            return;
        }

        ids.removeAt(sourceIndex);
        if (sourceIndex < targetIndex) {
            --targetIndex;
        }
        int insertIndex = dropAfter ? targetIndex + 1 : targetIndex;
        insertIndex = qBound(0, insertIndex, ids.size());
        ids.insert(insertIndex, sourceId);

        if (!m_settings->setFunctionOrderIds(ids)) {
            return;
        }
        if (!m_settings->save()) {
            showAttentionWarning(this, tr8("保存失败"), tr8("无法写入 config/settings.json。"));
        }
        populateModeGrid();
        if (m_onSettingsChanged) {
            m_onSettingsChanged();
        }
    }

    QWidget *historyPanel()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(18, 16, 18, 16);
        layout->setSpacing(10);

        auto *title = new QLabel(tr8("最近记录"));
        title->setFont(appFont(15, QFont::DemiBold));
        title->setStyleSheet(QStringLiteral("background: transparent; color: #111827;"));
        layout->addWidget(title);

        auto *tabs = new QTabWidget;
        configureScrollableHistoryTabs(tabs);
        tabs->setStyleSheet(QStringLiteral(
            "QTabWidget::pane { border: none; }"
            "QTabBar::tab { padding: 8px 12px; color: #667085; }"
            "QTabBar::tab:selected { color: #111827; font-weight: 600; }"
            "QTabBar QToolButton { width: 26px; background: #ffffff; border: 1px solid #d0d5dd; color: #111827; }"
            "QTabBar QToolButton:hover { background: #eef2ff; }"
        ));
        const QVector<HistoryEntry> entries = loadHistoryEntries();
        const QVector<HistoryTabDef> modes = historyTabModes();
        for (const HistoryTabDef &mode : modes) {
            tabs->addTab(historyListForMode(mode.id, entries, 8), mode.title);
        }
        layout->addWidget(tabs, 1);
        return frame;
    }

    QWidget *historyList(const QString &mode)
    {
        auto *list = new QListWidget;
        list->setFrameShape(QFrame::NoFrame);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setTextElideMode(Qt::ElideRight);
        list->setStyleSheet(QStringLiteral(
            "QListWidget { background: transparent; }"
            "QListWidget::item {"
            "  background: #f9fafb;"
            "  border: 1px solid #eef0f4;"
            "  border-radius: 6px;"
            "  padding: 10px;"
            "  margin: 4px 0;"
            "}"
        ));
        list->addItem(mode + tr8(" · 今天 18:50 · 已写入当前窗口"));
        list->addItem(mode + tr8(" · 今天 18:43 · 可复制 / 重试 / 删除"));
        list->addItem(mode + tr8(" · 昨天 21:08 · 本地保存录音和结果"));
        return list;
    }

    QWidget *statusPanel()
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        frame->setStyleSheet(cardStyle());
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(18, 16, 18, 16);
        layout->setSpacing(12);

        auto *title = new QLabel(tr8("当前状态"));
        title->setFont(appFont(15, QFont::DemiBold));
        title->setStyleSheet(QStringLiteral("background: transparent; color: #111827;"));
        layout->addWidget(title);

        layout->addWidget(statusRow(QStringLiteral("trayResident"), tr8("托盘常驻"), m_settings->trayResident() ? tr8("已开启") : tr8("已关闭")));
        layout->addWidget(statusRow(QStringLiteral("autoStart"), tr8("开机自启动"), m_settings->autoStartEnabled() ? tr8("已开启") : tr8("已关闭")));
        layout->addWidget(statusRow(QStringLiteral("strongSelection"), tr8("强力选中"), m_settings->strongSelectionEnabled() ? tr8("已开启") : tr8("已关闭")));
        layout->addWidget(statusRow(QStringLiteral("dictatePolish"), tr8("听写整理"), m_settings->dictatePolishEnabled() ? tr8("已开启") : tr8("已关闭")));
        layout->addWidget(statusRow(QStringLiteral("speechProvider"), tr8("语音识别"), speechProviderTitle(m_settings->speechProvider())));
        layout->addWidget(statusRow(QStringLiteral("networkProxy"), tr8("网络代理"), m_settings->useSystemProxy() ? tr8("系统代理") : tr8("直连")));
        layout->addWidget(statusRow(QStringLiteral("floatingBar"), tr8("浮动条"), m_settings->floatingBarEnabled() ? tr8("语音时显示") : tr8("已关闭")));
        layout->addWidget(statusRow(QStringLiteral("recordDirectory"), tr8("历史保存"), m_settings->usesDefaultRecordDirectory() ? tr8("默认位置") : tr8("自定义位置")));
        layout->addStretch();
        return frame;
    }

    QWidget *statusRow(const QString &name, const QString &value)
    {
        return statusRow(QString(), name, value);
    }

    QWidget *statusRow(const QString &id, const QString &name, const QString &value)
    {
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *left = new QLabel(name);
        auto *right = new QLabel(value);
        right->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));
        if (!id.isEmpty()) {
            m_statusValueLabels.insert(id, right);
        }
        layout->addWidget(left);
        layout->addStretch();
        layout->addWidget(right);
        return row;
    }

    void refreshStatusLabels()
    {
        if (m_statusValueLabels.contains(QStringLiteral("trayResident"))) {
            m_statusValueLabels.value(QStringLiteral("trayResident"))->setText(m_settings->trayResident() ? tr8("已开启") : tr8("已关闭"));
        }
        if (m_statusValueLabels.contains(QStringLiteral("autoStart"))) {
            m_statusValueLabels.value(QStringLiteral("autoStart"))->setText(m_settings->autoStartEnabled() ? tr8("已开启") : tr8("已关闭"));
        }
        if (m_statusValueLabels.contains(QStringLiteral("strongSelection"))) {
            m_statusValueLabels.value(QStringLiteral("strongSelection"))->setText(m_settings->strongSelectionEnabled() ? tr8("已开启") : tr8("已关闭"));
        }
        if (m_statusValueLabels.contains(QStringLiteral("floatingBar"))) {
            m_statusValueLabels.value(QStringLiteral("floatingBar"))->setText(m_settings->floatingBarEnabled() ? tr8("语音时显示") : tr8("已关闭"));
        }
        if (m_statusValueLabels.contains(QStringLiteral("dictatePolish"))) {
            m_statusValueLabels.value(QStringLiteral("dictatePolish"))->setText(m_settings->dictatePolishEnabled() ? tr8("已开启") : tr8("已关闭"));
        }
        if (m_statusValueLabels.contains(QStringLiteral("networkProxy"))) {
            m_statusValueLabels.value(QStringLiteral("networkProxy"))->setText(m_settings->useSystemProxy() ? tr8("系统代理") : tr8("直连"));
        }
        if (m_statusValueLabels.contains(QStringLiteral("speechProvider"))) {
            m_statusValueLabels.value(QStringLiteral("speechProvider"))->setText(speechProviderTitle(m_settings->speechProvider()));
        }
        if (m_statusValueLabels.contains(QStringLiteral("recordDirectory"))) {
            m_statusValueLabels.value(QStringLiteral("recordDirectory"))->setText(m_settings->usesDefaultRecordDirectory() ? tr8("默认位置") : tr8("自定义位置"));
        }
    }
};

// 语音业务控制器：串联快捷键触发、选中文本读取、录音、语音识别、大模型处理和结果展示。
class VoiceController : public QObject
{
public:
    VoiceController(AppSettings *settings, FloatingBar *bar, HubWindow *hub, QObject *parent = nullptr)
        : QObject(parent), m_settings(settings), m_bar(bar), m_hub(hub)
    {
        m_waveformTimer.setInterval(120);
        connect(&m_waveformTimer, &QTimer::timeout, this, [this]() {
            if (m_recording) {
                m_bar->setWaveformLevel(m_recorder.takePeakLevel());
            }
        });
        reload();
    }

    void reload()
    {
        m_api.reloadSecrets();
        m_api.setUseSystemProxy(m_settings->useSystemProxy());
    }

    void handleHotkey(const QString &id)
    {
        if (id == QStringLiteral("hub")) {
            m_hub->showNormal();
            m_hub->raise();
            m_hub->activateWindow();
            return;
        }

        if (m_countdownActive) {
            if (id == m_modeId) {
                ++m_countdownGeneration;
                m_countdownActive = false;
                m_bar->setStatus(tr8("已取消"), tr8("录音准备已取消"));
                m_bar->hideLater();
            } else {
                m_bar->setStatus(tr8("正在准备录音"), tr8("请等待倒计时结束，或再次按当前快捷键取消。"));
            }
            return;
        }

        if (m_recording) {
            if (id == m_modeId) {
                stopAndProcess();
            } else {
                m_bar->setStatus(tr8("正在录音"), tr8("请先结束当前录音。"));
            }
            return;
        }

        m_actionTimer.restart();
        rememberTargetWindow();
        m_modeId = id;
        const int floatingBarSeconds = m_settings->floatingBarSecondsFor(id);
        m_bar->setEnabledVisible(m_settings->floatingBarEnabled());
        m_bar->setSuppressed(floatingBarSeconds <= 0);
        m_bar->setAutoHideMsec(floatingBarSeconds * 1000);
        m_selectedText.clear();
        m_lastActionHadRecording = false;

        const bool useSelection = m_settings->useSelectionFor(id);
        const bool useVoice = m_settings->useVoiceFor(id);
        if (!useSelection && !useVoice) {
            showError(tr8("这个功能没有启用任何输入方式，请在左侧“功能自定义”中启用选中文字或语音输入。"));
            return;
        }

        if (useSelection) {
            m_bar->setStatus(
                tr8("正在读取选中文字"),
                m_settings->strongSelectionEnabled()
                    ? tr8("普通读取失败时会使用强力选中")
                    : tr8("不会复制，也不会读取剪贴板")
            );
            m_selectedText = ClipboardBridge::selectedText(m_settings->strongSelectionEnabled(), m_targetWindow).trimmed();
        }

        if (!useVoice) {
            if (m_selectedText.isEmpty()) {
                const QString message = tr8("请先用鼠标拖选要处理的文字，再按快捷键。");
                m_bar->setStatus(tr8("没有选中文字"), message);
                m_bar->hideLater();
                showAttentionInformation(m_hub, tr8("没有选中文字"), message);
                return;
            }
            m_bar->setStatus(tr8("正在处理"), tr8("正在调用模型处理选中文字"));
            processTextOnly(id, m_selectedText);
            return;
        }

        m_api.reloadSecrets();
        const QString speechConfigurationError = m_api.speechProviderConfigurationError(m_settings->speechProvider());
        if (!speechConfigurationError.isEmpty()) {
            showError(speechConfigurationError);
            return;
        }

        beginRecordingWithPreparation(id);
    }

private:
    struct LastRunContext
    {
        QString modeId;
        QString selectedText;
        QString voiceText;
        QString textOnlyInput;
        bool textOnly = false;
    };

    void beginRecordingWithPreparation(const QString &id)
    {
        const int countdownSeconds = m_settings->preRecordCountdownEnabled()
            ? m_settings->countdownSecondsFor(id)
            : 0;
        const bool playBeep = shouldPlayRecordingBeep(id);

        if (countdownSeconds > 0) {
            m_countdownActive = true;
            const int generation = ++m_countdownGeneration;
            for (int i = countdownSeconds; i >= 1; --i) {
                const int delay = (countdownSeconds - i) * 1000;
                QTimer::singleShot(delay, this, [this, id, generation, i]() {
                    if (!m_countdownActive || generation != m_countdownGeneration || id != m_modeId) {
                        return;
                    }
                    m_bar->setStatus(tr8("准备录音"), tr8("%1 秒后开始").arg(i));
                });
            }
            QTimer::singleShot(countdownSeconds * 1000, this, [this, id, generation, playBeep]() {
                if (!m_countdownActive || generation != m_countdownGeneration || id != m_modeId) {
                    return;
                }
                if (playBeep) {
                    m_bar->setStatus(tr8("准备录音"), tr8("提示音后开始"));
                    playRecordingBeep(id);
                    QTimer::singleShot(250, this, [this, id, generation]() {
                        startRecordingNow(id, generation);
                    });
                    return;
                }
                startRecordingNow(id, generation);
            });
            return;
        }

        if (playBeep) {
            m_countdownActive = true;
            const int generation = ++m_countdownGeneration;
            m_bar->setStatus(tr8("准备录音"), tr8("提示音后开始"));
            playRecordingBeep(id);
            QTimer::singleShot(250, this, [this, id, generation]() {
                startRecordingNow(id, generation);
            });
            return;
        }

        startRecordingNow(id, 0);
    }

    bool shouldPlayRecordingBeep(const QString &id) const
    {
        return m_settings->recordingBeepEnabled() && m_settings->recordingBeepEnabledFor(id);
    }

    void playRecordingBeep(const QString &id) const
    {
        const QString path = m_settings->recordingBeepPathFor(id);
        if (!path.isEmpty() && QFileInfo(path).isFile()) {
            QSound::play(path);
            return;
        }
        QApplication::beep();
    }

    void startRecordingNow(const QString &id, int generation)
    {
        if (generation != 0 && (!m_countdownActive || generation != m_countdownGeneration || id != m_modeId)) {
            return;
        }
        m_countdownActive = false;

        QString error;
        if (!m_recorder.start(functionTitle(id), m_settings->recordDirectoryPath(), &error)) {
            m_bar->setWaveformVisible(false);
            showError(error);
            return;
        }
        m_recording = true;
        m_bar->setWaveformVisible(true);
        m_bar->setStatus(tr8("正在") + functionTitle(id), tr8("再次按同一快捷键结束录音"));
        m_waveformTimer.start();
        QTimer::singleShot(60000, this, [this, id]() {
            if (m_recording && m_modeId == id) {
                stopAndProcess();
            }
        });
    }

    void rememberTargetWindow()
    {
#ifdef Q_OS_WIN
        m_targetWindow = GetForegroundWindow();
#else
        m_targetWindow = nullptr;
#endif
    }

    bool isCustomFunction(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            if (fn.id == id) {
                return true;
            }
        }
        return false;
    }

    CustomFunctionDef customFunction(const QString &id) const
    {
        for (const CustomFunctionDef &fn : m_settings->customFunctions()) {
            if (fn.id == id) {
                return fn;
            }
        }
        return CustomFunctionDef();
    }

    QString functionTitle(const QString &id) const
    {
        if (id == QStringLiteral("dictate")) return tr8("听写");
        if (id == QStringLiteral("translate")) return tr8("翻译");
        if (id == QStringLiteral("ask")) return tr8("问答");
        const CustomFunctionDef fn = customFunction(id);
        return fn.name.isEmpty() ? tr8("自定义功能") : fn.name;
    }

    void stopAndProcess()
    {
        const QString modeId = m_modeId;
        m_recording = false;
        m_waveformTimer.stop();
        m_bar->setWaveformVisible(false);
        m_lastActionHadRecording = true;
        const bool willCallModel = modeId != QStringLiteral("dictate") || m_settings->dictatePolishEnabled();
        const QString speechService = speechProviderTitle(m_settings->speechProvider());
        m_bar->setStatus(
            tr8("正在处理"),
            willCallModel
                ? tr8("正在使用") + speechService + tr8("识别语音并调用模型")
                : tr8("正在使用") + speechService + tr8("识别语音")
        );

        QString error;
        const QByteArray pcm = m_recorder.stop();
        const QString asr = m_api.speechAsr(m_settings->speechProvider(), pcm, &error);
        if (asr.trimmed().isEmpty()) {
            showError(error.isEmpty() ? tr8("没有识别到语音。") : error);
            saveHistory(modeId, QString(), QString(), error);
            return;
        }

        LastRunContext context;
        context.modeId = modeId;
        context.selectedText = m_selectedText;
        context.voiceText = asr;
        context.textOnly = false;
        m_lastRunContext = context;

        if (shouldStreamToResultPopup(context)) {
            streamContextToPopup(context);
            return;
        }

        QString output;
        if (modeId == QStringLiteral("dictate")) {
            output = processDictate(asr, &error);
        } else if (modeId == QStringLiteral("translate")) {
            output = m_selectedText.isEmpty()
                ? processTranslate(asr, QString(), &error)
                : processTranslate(m_selectedText, asr, &error);
        } else if (modeId == QStringLiteral("ask")) {
            output = processAsk(asr, &error);
        } else {
            output = processCustom(modeId, asr, &error);
        }

        if (output.trimmed().isEmpty()) {
            showError(error.isEmpty() ? tr8("模型没有返回结果。") : error);
            saveHistory(modeId, asr, QString(), error);
            return;
        }

        const QString historyInput = m_selectedText.isEmpty()
            ? asr
            : tr8("选中文字：\n") + m_selectedText + tr8("\n\n语音输入：\n") + asr;
        finishMode(modeId, historyInput, output);
    }

    void processTextOnly(const QString &modeId, const QString &text)
    {
        m_lastActionHadRecording = false;
        LastRunContext context;
        context.modeId = modeId;
        context.selectedText = m_selectedText;
        context.textOnlyInput = text;
        context.textOnly = true;
        m_lastRunContext = context;

        if (shouldStreamToResultPopup(context)) {
            streamContextToPopup(context);
            return;
        }

        QString error;
        QString output;
        if (modeId == QStringLiteral("dictate")) {
            output = processDictate(text, &error);
        } else if (modeId == QStringLiteral("translate")) {
            output = processTranslate(text, QString(), &error);
        } else if (modeId == QStringLiteral("ask")) {
            output = processAsk(QString(), &error);
        } else {
            output = processCustom(modeId, QString(), &error);
        }

        if (output.trimmed().isEmpty()) {
            showError(error.isEmpty() ? tr8("模型没有返回结果。") : error);
            saveHistory(modeId, text, QString(), error);
            return;
        }
        finishMode(modeId, text, output);
    }

    QString processDictate(
        const QString &asr,
        QString *error,
        const QString &modelOverride = QString(),
        const QString &extraInstruction = QString(),
        const std::function<void(const QString &)> &onDelta = std::function<void(const QString &)>()
    )
    {
        if (!m_settings->dictatePolishEnabled() && modelOverride.trimmed().isEmpty() && extraInstruction.trimmed().isEmpty()) {
            return asr;
        }
        const QString model = modelOverride.trimmed().isEmpty() ? m_settings->modelFor(QStringLiteral("dictate")) : modelOverride;
        if (!m_api.hasModelProvider(model)) {
            return asr;
        }
        const QString prompt = promptText(QStringLiteral("asr.txt"), tr8("整理语音识别文本，只输出可直接粘贴的结果。"));
        QString user = tr8("识别文本：\n") + asr;
        if (!extraInstruction.trimmed().isEmpty()) {
            user += tr8("\n\n补充要求：\n") + extraInstruction.trimmed();
        }
        return onDelta
            ? m_api.chatCompletionStream(model, prompt, user, onDelta, error)
            : m_api.chatCompletion(model, prompt, user, error);
    }

    QString processTranslate(
        const QString &text,
        const QString &voiceInstruction,
        QString *error,
        const QString &modelOverride = QString(),
        const QString &extraInstruction = QString(),
        const std::function<void(const QString &)> &onDelta = std::function<void(const QString &)>()
    )
    {
        const QString prompt = promptText(QStringLiteral("translate.txt"), tr8("翻译为简体中文，只输出翻译结果。"));
        QString user = tr8("目标语言：简体中文\n待翻译内容：\n") + text;
        if (!voiceInstruction.trimmed().isEmpty()) {
            user += tr8("\n\n语音补充要求：\n") + voiceInstruction;
        }
        if (!extraInstruction.trimmed().isEmpty()) {
            user += tr8("\n\n继续追问或补充要求：\n") + extraInstruction.trimmed();
        }
        const QString model = modelOverride.trimmed().isEmpty() ? m_settings->modelFor(QStringLiteral("translate")) : modelOverride;
        return onDelta
            ? m_api.chatCompletionStream(model, prompt, user, onDelta, error)
            : m_api.chatCompletion(model, prompt, user, error);
    }

    QString processAsk(
        const QString &question,
        QString *error,
        const QString &modelOverride = QString(),
        const QString &extraInstruction = QString(),
        const std::function<void(const QString &)> &onDelta = std::function<void(const QString &)>()
    )
    {
        const QString prompt = promptText(QStringLiteral("qa.txt"), tr8("基于选中文本回答用户问题。"));
        QString user = tr8("选中文本：\n") + (m_selectedText.isEmpty() ? tr8("（无）") : m_selectedText)
            + tr8("\n\n用户问题：\n") + (question.trimmed().isEmpty() ? tr8("请直接分析或处理选中的文字。") : question);
        if (!extraInstruction.trimmed().isEmpty()) {
            user += tr8("\n\n继续追问或补充要求：\n") + extraInstruction.trimmed();
        }
        const QString model = modelOverride.trimmed().isEmpty() ? m_settings->modelFor(QStringLiteral("ask")) : modelOverride;
        return onDelta
            ? m_api.chatCompletionStream(model, prompt, user, onDelta, error)
            : m_api.chatCompletion(model, prompt, user, error);
    }

    QString processCustom(
        const QString &id,
        const QString &voiceText,
        QString *error,
        const QString &modelOverride = QString(),
        const QString &extraInstruction = QString(),
        const std::function<void(const QString &)> &onDelta = std::function<void(const QString &)>()
    )
    {
        const CustomFunctionDef fn = customFunction(id);
        const QString prompt = fn.prompt.trimmed().isEmpty()
            ? tr8("根据选中文本和语音要求完成任务，只输出可直接使用的结果。")
            : fn.prompt;
        QString user = tr8("选中文本：\n") + (m_selectedText.isEmpty() ? tr8("（无）") : m_selectedText)
            + tr8("\n\n用户要求：\n") + (voiceText.trimmed().isEmpty() ? tr8("请直接按照提示词处理选中的文字。") : voiceText);
        if (!extraInstruction.trimmed().isEmpty()) {
            user += tr8("\n\n继续追问或补充要求：\n") + extraInstruction.trimmed();
        }
        const QString model = modelOverride.trimmed().isEmpty() ? fn.model : modelOverride;
        return onDelta
            ? m_api.chatCompletionStream(model, prompt, user, onDelta, error)
            : m_api.chatCompletion(model, prompt, user, error);
    }

    bool shouldStreamToResultPopup(const LastRunContext &context) const
    {
        if (m_settings->outputModeFor(context.modeId) != outputModePopup()) {
            return false;
        }
        if (context.modeId == QStringLiteral("dictate")) {
            const QString model = m_settings->modelFor(QStringLiteral("dictate"));
            return m_settings->dictatePolishEnabled() && m_api.hasModelProvider(model);
        }
        return true;
    }

    void configurePopupActions(ResultChoicePopup *popup, const LastRunContext &context)
    {
        if (!popup) {
            return;
        }
        popup->setCurrentModel(m_settings->modelFor(context.modeId));
        popup->setActionCallbacks(
            [this, popup, context]() {
                rerunLastIntoPopup(popup, context, QString(), QString());
            },
            [this, popup, context](const QString &model) {
                rerunLastIntoPopup(popup, context, model, QString());
            },
            [this, popup, context](const QString &followUp) {
                rerunLastIntoPopup(popup, context, QString(), followUp);
            }
        );
        popup->setDraftCallback([this, popup, context](const QString &draftText) {
            saveHistory(context.modeId, historyInputForContext(context), draftText, QString(), true, popup ? popup->currentModel() : QString());
            if (m_hub) {
                m_hub->applySettingsChanged();
            }
        });
    }

    void streamContextToPopup(const LastRunContext &context)
    {
        auto *popup = new ResultChoicePopup(
            m_settings,
            functionTitle(context.modeId),
            QString(),
            m_targetWindow,
            !context.selectedText.trimmed().isEmpty(),
            0
        );
        configurePopupActions(popup, context);
        popup->setBusy(true, tr8("正在生成"));
        popup->showNearBottom();

        m_bar->setStatus(tr8("正在处理"), tr8("正在流式生成结果"));
        QApplication::processEvents();

        QString error;
        const QString output = runContext(
            context,
            QString(),
            QString(),
            &error,
            [popup](const QString &delta) {
                if (popup) {
                    popup->appendResultText(delta);
                    QApplication::processEvents();
                }
            }
        );

        if (output.trimmed().isEmpty()) {
            popup->setBusy(false, tr8("生成失败"));
            showError(error.isEmpty() ? tr8("模型没有返回结果。") : error);
            saveHistory(context.modeId, historyInputForContext(context), QString(), error);
            return;
        }

        popup->setResultText(output);
        popup->setBusy(false, tr8("已生成"));
        saveHistory(context.modeId, historyInputForContext(context), output, QString());
        m_bar->setResult(tr8("处理完成"), output);
        m_bar->hideLater();
    }

    QString historyInputForContext(const LastRunContext &context) const
    {
        if (context.textOnly) {
            return context.textOnlyInput;
        }
        if (context.selectedText.trimmed().isEmpty()) {
            return context.voiceText;
        }
        return tr8("选中文字：\n") + context.selectedText + tr8("\n\n语音输入：\n") + context.voiceText;
    }

    QString runContext(
        const LastRunContext &context,
        const QString &modelOverride,
        const QString &extraInstruction,
        QString *error,
        const std::function<void(const QString &)> &onDelta = std::function<void(const QString &)>()
    )
    {
        const QString previousSelectedText = m_selectedText;
        m_selectedText = context.selectedText;

        QString output;
        if (context.modeId == QStringLiteral("dictate")) {
            const QString text = context.textOnly ? context.textOnlyInput : context.voiceText;
            output = processDictate(text, error, modelOverride, extraInstruction, onDelta);
        } else if (context.modeId == QStringLiteral("translate")) {
            if (context.textOnly) {
                output = processTranslate(context.textOnlyInput, QString(), error, modelOverride, extraInstruction, onDelta);
            } else {
                output = context.selectedText.isEmpty()
                    ? processTranslate(context.voiceText, QString(), error, modelOverride, extraInstruction, onDelta)
                    : processTranslate(context.selectedText, context.voiceText, error, modelOverride, extraInstruction, onDelta);
            }
        } else if (context.modeId == QStringLiteral("ask")) {
            const QString question = context.textOnly ? QString() : context.voiceText;
            output = processAsk(question, error, modelOverride, extraInstruction, onDelta);
        } else {
            const QString voiceText = context.textOnly ? QString() : context.voiceText;
            output = processCustom(context.modeId, voiceText, error, modelOverride, extraInstruction, onDelta);
        }

        m_selectedText = previousSelectedText;
        return output;
    }

    void rerunLastIntoPopup(ResultChoicePopup *popup, const LastRunContext &context, const QString &modelOverride, const QString &extraInstruction)
    {
        if (!popup) {
            return;
        }

        m_actionTimer.restart();
        popup->setBusy(true, extraInstruction.trimmed().isEmpty() ? tr8("正在重新生成") : tr8("正在继续处理"));
        popup->setResultText(QString());
        m_bar->setStatus(tr8("正在处理"), tr8("正在调用模型生成结果"));
        QApplication::processEvents();

        QString error;
        const QString output = runContext(
            context,
            modelOverride,
            extraInstruction,
            &error,
            [popup](const QString &delta) {
                if (popup) {
                    popup->appendResultText(delta);
                    QApplication::processEvents();
                }
            }
        );
        if (output.trimmed().isEmpty()) {
            popup->setBusy(false, tr8("生成失败"));
            showError(error.isEmpty() ? tr8("模型没有返回结果。") : error);
            return;
        }

        const QString finalModel = modelOverride.trimmed().isEmpty() ? m_settings->modelFor(context.modeId) : modelOverride;
        popup->setCurrentModel(finalModel);
        popup->setResultText(output);
        popup->setBusy(false, tr8("已生成"));
        saveHistory(context.modeId, historyInputForContext(context), output, QString(), false, finalModel);
        m_bar->setResult(tr8("处理完成"), output);
        m_bar->hideLater();
    }

    void finishMode(const QString &modeId, const QString &input, const QString &output)
    {
        saveHistory(modeId, input, output, QString());
        m_bar->setResult(tr8("处理完成"), output);

        if (m_settings->outputModeFor(modeId) == outputModeAutoWrite()) {
            ClipboardBridge::pasteTextToWindow(output, m_targetWindow, true, hasSelectedTextForMode(modeId));
            m_bar->setStatus(tr8("已写入"), tr8("结果已粘贴到当前输入位置"));
            m_bar->hideLater();
        } else {
            m_bar->hideLater();
            showResultChoicePopup(modeId, output);
        }
    }

    bool hasSelectedTextForMode(const QString &modeId) const
    {
        Q_UNUSED(modeId);
        return !m_selectedText.trimmed().isEmpty();
    }

    void showResultChoicePopup(const QString &modeId, const QString &result)
    {
        const LastRunContext context = m_lastRunContext;
        auto *popup = new ResultChoicePopup(
            m_settings,
            functionTitle(modeId),
            result,
            m_targetWindow,
            hasSelectedTextForMode(modeId),
            m_settings->resultPopupSecondsFor(modeId) * 1000
        );
        configurePopupActions(popup, context);
        popup->showNearBottom();
    }

    void showResultDialog(const QString &title, const QString &result)
    {
        QDialog dialog(m_hub);
        dialog.setWindowTitle(title);
        dialog.setMinimumSize(560, 360);

        auto *layout = new QVBoxLayout(&dialog);
        auto *editor = new QTextEdit;
        editor->setReadOnly(true);
        editor->setPlainText(result);
        layout->addWidget(editor, 1);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        auto *copy = new QPushButton(tr8("复制"));
        auto *paste = new QPushButton(tr8("粘贴到当前输入框"));
        auto *close = new QPushButton(tr8("关闭"));
        buttons->addWidget(copy);
        buttons->addWidget(paste);
        buttons->addWidget(close);
        layout->addLayout(buttons);

        connect(copy, &QPushButton::clicked, &dialog, [result]() {
            QApplication::clipboard()->setText(result);
        });
        connect(paste, &QPushButton::clicked, &dialog, [result]() {
            ClipboardBridge::pasteText(result);
        });
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);

        dialog.exec();
    }

    void showError(const QString &message)
    {
        const QString text = message.trimmed().isEmpty() ? tr8("发生未知错误。") : message;
        m_bar->setStatus(tr8("处理失败"), text);
        m_bar->hideLater();
        showAttentionWarning(m_hub, tr8("处理失败"), text);
    }

    qint64 currentActionElapsedMs() const
    {
        return m_actionTimer.isValid() ? m_actionTimer.elapsed() : -1;
    }

    void saveHistory(const QString &modeId, const QString &input, const QString &output, const QString &error, bool draft = false, const QString &modelOverride = QString())
    {
        const QString date = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
        const QString recordRoot = m_settings->recordDirectoryPath();
        const QString modeTitle = functionTitle(modeId);
        ensureHistoryModeDateStructure(recordRoot, modeTitle, date);
        const QString fileBase = QDateTime::currentDateTime().toString(QStringLiteral("HHmmss_zzz_"))
            + safeFileNamePart(modeId, QStringLiteral("mode"))
            + (draft ? QStringLiteral("_draft") : QString());

        const QString modeTextPath = QDir(historyModeDateSubDirectory(recordRoot, modeTitle, date, historyTextSubFolderName()))
            .filePath(fileBase + QStringLiteral(".txt"));
        const QString modeDetailPath = QDir(historyModeDateSubDirectory(recordRoot, modeTitle, date, historyDetailSubFolderName()))
            .filePath(fileBase + QStringLiteral(".json"));
        const QString allTextPath = QDir(historyAllDateDirectory(recordRoot, historyAllTextFolderName(), date))
            .filePath(fileBase + QStringLiteral(".txt"));
        const QString allDetailPath = QDir(historyAllDateDirectory(recordRoot, historyAllDetailFolderName(), date))
            .filePath(fileBase + QStringLiteral(".json"));

        const QString audioPath = m_lastActionHadRecording ? m_recorder.lastWavPath() : QString();
        QString allAudioPath;
        if (!audioPath.trimmed().isEmpty() && QFileInfo::exists(audioPath)) {
            const QString allAudioDir = historyAllDateDirectory(recordRoot, historyAllAudioFolderName(), date);
            QDir().mkpath(allAudioDir);
            allAudioPath = uniqueFilePath(QDir(allAudioDir).filePath(QFileInfo(audioPath).fileName()));
            if (!QFile::copy(audioPath, allAudioPath)) {
                allAudioPath.clear();
            }
        }

        QJsonObject item;
        item.insert(QStringLiteral("modeId"), modeId);
        item.insert(QStringLiteral("mode"), modeTitle);
        item.insert(QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate));
        item.insert(QStringLiteral("audio"), audioPath);
        item.insert(QStringLiteral("textFile"), modeTextPath);
        item.insert(QStringLiteral("allAudioFile"), allAudioPath);
        item.insert(QStringLiteral("allTextFile"), allTextPath);
        item.insert(QStringLiteral("allDetailFile"), allDetailPath);
        item.insert(QStringLiteral("input"), input);
        item.insert(QStringLiteral("output"), output);
        item.insert(QStringLiteral("error"), error);
        const QString model = modelOverride.trimmed().isEmpty() ? m_settings->modelFor(modeId) : modelOverride;
        const bool usedModel = !(modeId == QStringLiteral("dictate")
            && !m_settings->dictatePolishEnabled()
            && modelOverride.trimmed().isEmpty());
        item.insert(QStringLiteral("model"), usedModel ? model : QString());
        item.insert(QStringLiteral("elapsedMs"), static_cast<double>(currentActionElapsedMs()));
        item.insert(QStringLiteral("favorite"), false);
        item.insert(QStringLiteral("draft"), draft);

        const QString readableText = historyTextFromJsonObject(item);
        writeTextFile(modeTextPath, readableText);
        writeTextFile(allTextPath, readableText);

        const QByteArray json = QJsonDocument(item).toJson(QJsonDocument::Indented);
        for (const QString &path : QStringList() << modeDetailPath << allDetailPath) {
            QFileInfo info(path);
            if (!info.dir().exists()) {
                info.dir().mkpath(QStringLiteral("."));
            }
            QFile file(path);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                file.write(json);
            }
        }
    }

    AppSettings *m_settings;
    FloatingBar *m_bar;
    HubWindow *m_hub;
    ApiClient m_api;
    AudioRecorder m_recorder;
    bool m_recording = false;
    bool m_countdownActive = false;
    int m_countdownGeneration = 0;
    bool m_lastActionHadRecording = false;
    QElapsedTimer m_actionTimer;
    QString m_modeId;
    QString m_selectedText;
    LastRunContext m_lastRunContext;
    NativeWindowHandle m_targetWindow = nullptr;
    QTimer m_waveformTimer;
};

// 托盘控制器：让软件后台常驻，并提供托盘菜单快速切换代理、语音服务和浮动条。
class TrayController : public QObject
{
public:
    TrayController(HubWindow *hub, FloatingBar *bar, AppSettings *settings, const std::function<void()> &onSettingsChanged, QObject *parent = nullptr)
        : QObject(parent), m_hub(hub), m_bar(bar), m_settings(settings), m_onSettingsChanged(onSettingsChanged)
    {
        m_tray = new QSystemTrayIcon(this);
        m_tray->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaVolume));
        m_tray->setToolTip(tr8("vocekit"));

        auto *menu = new QMenu;
        auto *openHub = menu->addAction(tr8("打开主界面"));
        auto *settingsAction = menu->addAction(tr8("设置"));
        menu->addSeparator();

        auto *speechMenu = menu->addMenu(tr8("语音识别服务"));
        auto *speechGroup = new QActionGroup(speechMenu);
        speechGroup->setExclusive(true);
        auto *baiduSpeech = speechMenu->addAction(tr8("百度语音识别"));
        baiduSpeech->setCheckable(true);
        baiduSpeech->setActionGroup(speechGroup);
        auto *xfyunSpeech = speechMenu->addAction(tr8("讯飞语音听写"));
        xfyunSpeech->setCheckable(true);
        xfyunSpeech->setActionGroup(speechGroup);

        auto *proxyAction = menu->addAction(tr8("网络代理：直连"));
        proxyAction->setCheckable(true);
        auto *floatingBarAction = menu->addAction(tr8("浮动条：语音时显示"));
        floatingBarAction->setCheckable(true);
        menu->addSeparator();

        auto *showBar = menu->addAction(tr8("测试浮动条"));
        menu->addSeparator();
        auto *quit = menu->addAction(tr8("退出"));

        connect(openHub, &QAction::triggered, m_hub, &QWidget::showNormal);
        connect(menu, &QMenu::aboutToShow, this, [this, baiduSpeech, xfyunSpeech, proxyAction, floatingBarAction]() {
            const QString provider = m_settings->speechProvider();
            baiduSpeech->setChecked(provider == speechProviderBaidu());
            xfyunSpeech->setChecked(provider == speechProviderXfyun());
            proxyAction->setChecked(m_settings->useSystemProxy());
            proxyAction->setText(m_settings->useSystemProxy() ? tr8("网络代理：使用系统代理") : tr8("网络代理：直连"));
            floatingBarAction->setChecked(m_settings->floatingBarEnabled());
            floatingBarAction->setText(m_settings->floatingBarEnabled() ? tr8("浮动条：语音时显示") : tr8("浮动条：已关闭"));
        });
        connect(baiduSpeech, &QAction::triggered, this, [this]() {
            if (m_settings->speechProvider() == speechProviderBaidu()) {
                return;
            }
            m_settings->setSpeechProvider(speechProviderBaidu());
            applyQuickSetting(tr8("已切换为百度语音识别"));
        });
        connect(xfyunSpeech, &QAction::triggered, this, [this]() {
            if (m_settings->speechProvider() == speechProviderXfyun()) {
                return;
            }
            m_settings->setSpeechProvider(speechProviderXfyun());
            applyQuickSetting(tr8("已切换为讯飞语音听写"));
        });
        connect(proxyAction, &QAction::triggered, this, [this](bool checked) {
            m_settings->setUseSystemProxy(checked);
            applyQuickSetting(checked ? tr8("网络代理已切换为系统代理") : tr8("网络代理已切换为直连"));
        });
        connect(floatingBarAction, &QAction::triggered, this, [this](bool checked) {
            m_settings->setFloatingBarEnabled(checked);
            applyQuickSetting(checked ? tr8("浮动条已启用") : tr8("浮动条已关闭"));
        });
        connect(showBar, &QAction::triggered, this, [this]() {
            if (!m_settings->floatingBarEnabled()) {
                showAttentionInformation(m_hub, tr8("浮动条已关闭"), tr8("请在设置的“常用设置”页勾选“启用浮动条”。"));
                return;
            }
            m_bar->setSuppressed(false);
            m_bar->setStatus(tr8("浮动条测试"), tr8("语音输入时显示，结束后自动关闭"));
            m_bar->hideLater();
        });
        connect(settingsAction, &QAction::triggered, this, [this]() {
            if (m_hub) {
                m_hub->showSettingsPage();
            }
        });
        connect(quit, &QAction::triggered, qApp, &QApplication::quit);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                m_hub->showNormal();
                m_hub->raise();
                m_hub->activateWindow();
            }
        });

        m_tray->setContextMenu(menu);
        m_tray->show();
    }

private:
    void applyQuickSetting(const QString &message)
    {
        if (m_onSettingsChanged) {
            m_onSettingsChanged();
        } else if (m_settings) {
            m_settings->save();
            if (m_bar) {
                m_bar->setEnabledVisible(m_settings->floatingBarEnabled());
            }
            if (m_hub) {
                m_hub->applySettingsChanged();
            }
        }
        if (m_tray && !message.trimmed().isEmpty()) {
            m_tray->showMessage(tr8("语音助手"), message, QSystemTrayIcon::Information, 1600);
        }
    }

    HubWindow *m_hub;
    FloatingBar *m_bar;
    AppSettings *m_settings;
    std::function<void()> m_onSettingsChanged;
    QSystemTrayIcon *m_tray;
};

// 中文右键菜单：把 Qt 文本框默认英文菜单翻译成中文，同时保留 Ctrl+C 等快捷键提示。
class ChineseTextContextMenu : public QObject
{
public:
    explicit ChineseTextContextMenu(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::ContextMenu) {
            return QObject::eventFilter(watched, event);
        }

        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (!widget) {
            return QObject::eventFilter(watched, event);
        }

        QMenu *menu = nullptr;
        QWidget *current = widget;
        while (current && !menu) {
            if (auto *lineEdit = qobject_cast<QLineEdit *>(current)) {
                menu = lineEdit->createStandardContextMenu();
            } else if (auto *textEdit = qobject_cast<QTextEdit *>(current)) {
                menu = textEdit->createStandardContextMenu();
            } else if (auto *plainTextEdit = qobject_cast<QPlainTextEdit *>(current)) {
                menu = plainTextEdit->createStandardContextMenu();
            } else {
                current = current->parentWidget();
            }
        }
        if (!menu) {
            return QObject::eventFilter(watched, event);
        }

        translateActions(menu);
        auto *contextEvent = static_cast<QContextMenuEvent *>(event);
        menu->exec(contextEvent->globalPos());
        menu->deleteLater();
        return true;
    }

private:
    static QString normalizedActionText(QString text)
    {
        text = text.section(QLatin1Char('\t'), 0, 0);
        text.remove(QLatin1Char('&'));
        return text.trimmed().toLower();
    }

    static QString shortcutSuffix(const QString &text)
    {
        const int separator = text.indexOf(QLatin1Char('\t'));
        return separator >= 0 ? text.mid(separator) : QString();
    }

    static void translateActions(QMenu *menu)
    {
        for (QAction *action : menu->actions()) {
            const QString text = normalizedActionText(action->text());
            const QString shortcut = shortcutSuffix(action->text());
            if (text == QStringLiteral("undo")) {
                action->setText(tr8("撤销") + shortcut);
            } else if (text == QStringLiteral("redo")) {
                action->setText(tr8("恢复") + shortcut);
            } else if (text == QStringLiteral("cut")) {
                action->setText(tr8("剪切") + shortcut);
            } else if (text == QStringLiteral("copy")) {
                action->setText(tr8("复制") + shortcut);
            } else if (text == QStringLiteral("paste")) {
                action->setText(tr8("粘贴") + shortcut);
            } else if (text == QStringLiteral("delete")) {
                action->setText(tr8("删除") + shortcut);
            } else if (text == QStringLiteral("select all")) {
                action->setText(tr8("全选") + shortcut);
            }
        }
    }
};

// 程序入口：初始化 Qt、配置、托盘、快捷键和主界面，默认以后台助手形态运行。
int runVocekit(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    QTranslator qtChineseTranslator;
    const QString deployedTranslations = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations"));
    if (!qtChineseTranslator.load(QStringLiteral("qt_zh_CN"), deployedTranslations)) {
        qtChineseTranslator.load(QStringLiteral("qt_zh_CN"), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    }
    app.installTranslator(&qtChineseTranslator);

    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setApplicationName(QStringLiteral("vocekit"));
    QApplication::setOrganizationName(QStringLiteral("vocekit"));
    app.setFont(appFont());
    ChineseTextContextMenu chineseTextContextMenu(&app);
    app.installEventFilter(&chineseTextContextMenu);

    AppSettings settings;
    settings.load();
    settings.save();

    FloatingBar bar(&settings);
    GlobalHotkeys hotkeys;
    VoiceController *controller = nullptr;

    std::function<void()> settingsChanged;
    HubWindow hub(&settings, &bar, [&]() {
        if (settingsChanged) {
            settingsChanged();
        }
    });
    VoiceController voice(&settings, &bar, &hub);
    controller = &voice;

    hotkeys.setCallback([&](const QString &id) {
        if (controller) {
            controller->handleHotkey(id);
        }
    });
    qApp->installNativeEventFilter(&hotkeys);

    settingsChanged = [&]() {
        settings.save();
        setWindowsAutoStartEnabled(settings.autoStartEnabled());
        voice.reload();
        hotkeys.registerFromSettings(settings);
        bar.setEnabledVisible(settings.floatingBarEnabled());
        hub.refreshShortcuts();
    };
    settingsChanged();

    TrayController tray(&hub, &bar, &settings, settingsChanged);

    const QRect screen = QApplication::desktop()->availableGeometry();
    hub.move(screen.left() + 60, screen.top() + 40);
    hub.show();
    bar.setEnabledVisible(settings.floatingBarEnabled());

    return app.exec();
}
