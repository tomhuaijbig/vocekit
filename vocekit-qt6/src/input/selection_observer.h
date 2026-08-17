#ifndef VOCEKIT_SELECTION_OBSERVER_H
#define VOCEKIT_SELECTION_OBSERVER_H

#include "selection_snapshot.h"

#include <QPoint>
#include <QString>

#include <functional>

enum class SelectionObservationReason
{
    MouseSelection,
    KeyboardSelection,
    FallbackShortcut,
    OutsidePointerRelease,
    ForegroundChanged,
    EscapePressed,
    SystemUnavailable,
    SystemAvailable
};

struct SelectionObservation
{
    SelectionObservationReason reason =
        SelectionObservationReason::MouseSelection;
    QPoint cursorPhysicalPosition;
    SelectedTextNativeWindowHandle targetWindow = nullptr;
};

class SelectionObservationMatcher
{
public:
    void setAutomaticEnabled(bool enabled);
    void setKeyboardSelectionEnabled(bool enabled);
    void setPaused(bool paused);
    void setSurfaceVisible(bool visible);

    void mousePressed(const QPoint &point);
    bool mouseReleased(const QPoint &point);
    void keyPressed(unsigned int nativeKey);
    bool keyReleased(unsigned int nativeKey);
    bool fallbackShortcutReleased() const;
    bool escapeReleased();
    void reset();

    bool isPaused() const;

private:
    bool m_automaticEnabled = true;
    bool m_keyboardSelectionEnabled = true;
    bool m_paused = false;
    bool m_surfaceVisible = false;
    bool m_mousePressed = false;
    bool m_shiftPressed = false;
    bool m_controlPressed = false;
    bool m_implicitShiftReleaseEmitted = false;
    bool m_ctrlAArmed = false;
    unsigned int m_armedSelectionKey = 0;
    QPoint m_mousePressPoint;
};

class SelectionObserver
{
public:
    SelectionObserver();
    ~SelectionObserver();

    bool install(
        SelectedTextNativeWindowHandle notificationWindow,
        QString *error = nullptr
    );
    void uninstall();
    bool isInstalled() const;

    void setAutomaticEnabled(bool enabled);
    void setKeyboardSelectionEnabled(bool enabled);
    void setPaused(bool paused);
    void setSurfaceVisible(bool visible);
    void setCallback(
        const std::function<void(const SelectionObservation &)> &callback
    );
    void triggerFallbackShortcut();

    void processNativeMouse(unsigned int message, const QPoint &point);
    void processNativeKey(unsigned int message, unsigned int nativeKey);
    void processForegroundWindowChanged(
        SelectedTextNativeWindowHandle window
    );
    void processSystemAvailabilityChanged(bool available);

private:
    class Impl;
    Impl *m_impl = nullptr;
};

#endif // VOCEKIT_SELECTION_OBSERVER_H
