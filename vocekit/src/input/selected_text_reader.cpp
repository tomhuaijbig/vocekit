#include "selected_text_reader.h"

#include <QApplication>
#include <QClipboard>
#include <QList>
#include <QMimeData>
#include <QThread>
#include <QUrl>
#include <QUuid>
#include <QVariant>

#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>

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

static void sendCtrlKeyForSelection(WORD key)
{
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
}

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

static QString selectedTextViaClipboardCopy(SelectedTextNativeWindowHandle window)
{
    if (window) {
        SetForegroundWindow(static_cast<HWND>(window));
        QThread::msleep(90);
        QApplication::processEvents();
    }

    QClipboard *clipboard = QApplication::clipboard();
    const ClipboardSnapshot previous = captureClipboardSnapshot(clipboard->mimeData());
    const QString sentinel = QStringLiteral("__VOICE_ASSISTANT_SELECTION_SENTINEL__") + QUuid::createUuid().toString();

    clipboard->setText(sentinel);
    QApplication::processEvents();
    QThread::msleep(60);

    sendCtrlKeyForSelection('C');
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
}
#endif

QString SelectedTextReader::read(
    bool strongSelectionEnabled,
    SelectedTextNativeWindowHandle window
)
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
