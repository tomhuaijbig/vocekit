#ifndef VOCEKIT_SELECTION_PROBE_RUNNER_H
#define VOCEKIT_SELECTION_PROBE_RUNNER_H

#include "selection_snapshot.h"

#include <QObject>

#include <functional>

struct SelectionProbeRunnerCallbacks
{
    std::function<void(
        quint64 generation,
        const SelectionSnapshot &snapshot
    )> completed;
    std::function<void(quint64 generation)> timedOut;
};

struct SelectionProbeRunnerAccess
{
    std::function<SelectionPhysicalProbeResult(
        const SelectionProbeRequest &request
    )> probeUiAutomationPhysical;
    std::function<void(quint32 workerThreadId)> cancelComCall;
    std::function<quint32()> clipboardSequenceNumber;
    std::function<quint32()> clipboardOwnerProcessId;
    std::function<bool(SelectedTextNativeWindowHandle)>
        targetStillForeground;
    std::function<void()> sendCopyShortcut;
};

class SelectionProbeRunner : public QObject
{
    Q_OBJECT

public:
    explicit SelectionProbeRunner(
        const SelectionProbeRunnerAccess &access =
            SelectionProbeRunnerAccess(),
        QObject *parent = nullptr
    );
    ~SelectionProbeRunner() override;

    void start(
        const SelectionProbeRequest &request,
        bool strongSelectionEnabled,
        quint64 generation,
        const SelectionProbeRunnerCallbacks &callbacks
    );
    void cancel();
    bool isRunning() const;
    void validateSelectionAsync(
        SelectedTextNativeWindowHandle window,
        quint64 generation,
        const std::function<void(quint64, bool)> &completed
    );

private:
    class Impl;
    Impl *m_impl = nullptr;
};

#endif // VOCEKIT_SELECTION_PROBE_RUNNER_H
