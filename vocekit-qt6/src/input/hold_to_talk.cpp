#include "hold_to_talk.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QTimer>
#include <Qt>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static int holdNativeKeyFromQtKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return 'A' + key - Qt::Key_A;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return '0' + key - Qt::Key_0;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return 0x70 + key - Qt::Key_F1;
    }
    if (key == Qt::Key_Space) {
        return 0x20;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        return 0x0d;
    }
    if (key == Qt::Key_Escape) {
        return 0x1b;
    }
    if (key == Qt::Key_Tab) {
        return 0x09;
    }
    return 0;
}

bool HoldShortcutMatcher::configure(const QString &shortcut)
{
    const QKeySequence sequence(shortcut);
    if (sequence.isEmpty()) {
        reset();
        return false;
    }

    const int value = sequence[0];
    unsigned int modifiers = HoldModifierNone;
    if (value & Qt::CTRL) {
        modifiers |= HoldModifierControl;
    }
    if (value & Qt::ALT) {
        modifiers |= HoldModifierAlt;
    }
    if (value & Qt::SHIFT) {
        modifiers |= HoldModifierShift;
    }
    if (value & Qt::META) {
        modifiers |= HoldModifierMeta;
    }

    const int modifierMask = static_cast<int>(Qt::CTRL)
        | static_cast<int>(Qt::ALT)
        | static_cast<int>(Qt::SHIFT)
        | static_cast<int>(Qt::META);
    const int nativeKey = holdNativeKeyFromQtKey(value & ~modifierMask);
    if (nativeKey == 0 || modifiers == HoldModifierNone) {
        reset();
        return false;
    }

    m_nativeKey = nativeKey;
    m_modifiers = modifiers;
    m_pressed = false;
    return true;
}

void HoldShortcutMatcher::reset()
{
    m_nativeKey = 0;
    m_modifiers = HoldModifierNone;
    m_pressed = false;
}

HoldShortcutTransition HoldShortcutMatcher::process(
    int nativeKey,
    unsigned int modifiers,
    bool pressed,
    bool autoRepeat
)
{
    if (nativeKey != m_nativeKey || m_nativeKey == 0) {
        return HoldShortcutTransition::None;
    }

    if (pressed) {
        if (autoRepeat || m_pressed || (modifiers & m_modifiers) != m_modifiers) {
            return HoldShortcutTransition::None;
        }
        m_pressed = true;
        return HoldShortcutTransition::Pressed;
    }

    if (!m_pressed) {
        return HoldShortcutTransition::None;
    }
    m_pressed = false;
    return HoldShortcutTransition::Released;
}

int HoldShortcutMatcher::nativeKey() const
{
    return m_nativeKey;
}

unsigned int HoldShortcutMatcher::modifiers() const
{
    return m_modifiers;
}

bool HoldShortcutMatcher::isPressed() const
{
    return m_pressed;
}

#ifdef Q_OS_WIN
static HoldToTalkHook *g_holdToTalkHook = nullptr;

static LRESULT CALLBACK holdToTalkKeyboardProc(
    int code,
    WPARAM message,
    LPARAM data
)
{
    if (code == HC_ACTION && g_holdToTalkHook && data) {
        const KBDLLHOOKSTRUCT *event =
            reinterpret_cast<const KBDLLHOOKSTRUCT *>(data);
        g_holdToTalkHook->processNativeKey(
            static_cast<unsigned int>(message),
            static_cast<unsigned int>(event->vkCode)
        );
    }
    return CallNextHookEx(nullptr, code, message, data);
}

static unsigned int currentHoldModifiers()
{
    unsigned int modifiers = HoldModifierNone;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        modifiers |= HoldModifierControl;
    }
    if (GetAsyncKeyState(VK_MENU) & 0x8000) {
        modifiers |= HoldModifierAlt;
    }
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        modifiers |= HoldModifierShift;
    }
    if ((GetAsyncKeyState(VK_LWIN) & 0x8000)
        || (GetAsyncKeyState(VK_RWIN) & 0x8000)) {
        modifiers |= HoldModifierMeta;
    }
    return modifiers;
}
#endif

HoldToTalkHook::~HoldToTalkHook()
{
    uninstall();
}

void HoldToTalkHook::setCallback(
    const std::function<void(const QString &, HoldShortcutTransition)> &callback
)
{
    m_callback = callback;
}

bool HoldToTalkHook::configure(
    const QMap<QString, QString> &shortcuts,
    QString *error
)
{
    uninstall();
    m_matchers.clear();

    for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it) {
        HoldShortcutMatcher matcher;
        if (!matcher.configure(it.value())) {
            if (error) {
                *error = QStringLiteral("快捷键格式无效：") + it.value();
            }
            m_matchers.clear();
            return false;
        }
        m_matchers.insert(it.key(), matcher);
    }

    if (m_matchers.isEmpty()) {
        return true;
    }

#ifdef Q_OS_WIN
    g_holdToTalkHook = this;
    HHOOK hook = SetWindowsHookExW(
        WH_KEYBOARD_LL,
        holdToTalkKeyboardProc,
        GetModuleHandleW(nullptr),
        0
    );
    if (!hook) {
        g_holdToTalkHook = nullptr;
        if (error) {
            *error = QStringLiteral("Windows 键盘监听安装失败，错误码 ")
                + QString::number(GetLastError());
        }
        return false;
    }
    m_hook = hook;
    return true;
#else
    if (error) {
        *error = QStringLiteral("当前系统不支持按住说话");
    }
    return false;
#endif
}

void HoldToTalkHook::uninstall()
{
#ifdef Q_OS_WIN
    if (m_hook) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_hook));
        m_hook = nullptr;
    }
    if (g_holdToTalkHook == this) {
        g_holdToTalkHook = nullptr;
    }
#else
    m_hook = nullptr;
#endif
    for (auto it = m_matchers.begin(); it != m_matchers.end(); ++it) {
        it.value().reset();
    }
}

bool HoldToTalkHook::isInstalled() const
{
    return m_hook != nullptr;
}

bool HoldToTalkHook::hasPressedHold() const
{
    for (auto it = m_matchers.constBegin();
         it != m_matchers.constEnd();
         ++it) {
        if (it.value().isPressed()) {
            return true;
        }
    }
    return false;
}

void HoldToTalkHook::processNativeKey(
    unsigned int message,
    unsigned int nativeKey
)
{
#ifdef Q_OS_WIN
    const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool released = message == WM_KEYUP || message == WM_SYSKEYUP;
    if (!pressed && !released) {
        return;
    }

    const unsigned int modifiers = currentHoldModifiers();
    for (auto it = m_matchers.begin(); it != m_matchers.end(); ++it) {
        const HoldShortcutTransition transition = it.value().process(
            static_cast<int>(nativeKey),
            modifiers,
            pressed,
            false
        );
        if (transition == HoldShortcutTransition::None || !m_callback) {
            continue;
        }

        const QString id = it.key();
        const std::function<void(const QString &, HoldShortcutTransition)>
            callback = m_callback;
        QTimer::singleShot(
            0,
            QCoreApplication::instance(),
            [callback, id, transition]() {
                if (callback) {
                    callback(id, transition);
                }
            }
        );
    }
#else
    Q_UNUSED(message);
    Q_UNUSED(nativeKey);
#endif
}
