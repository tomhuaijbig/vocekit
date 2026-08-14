#include "selection_probe_runner.h"

#include "selected_text_reader.h"
#include "selection_coordinate_mapper.h"

#include <QApplication>
#include <QAtomicInt>
#include <QClipboard>
#include <QCursor>
#include <QFutureWatcher>
#include <QList>
#include <QMap>
#include <QMimeData>
#include <QPointer>
#include <QSharedPointer>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVariant>
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#endif

namespace {

struct ClipboardSnapshot
{
    QMap<QString, QByteArray> formats;
    QString text;
    QString html;
    QList<QUrl> urls;
    QVariant imageData;
    bool hasText = false;
    bool hasHtml = false;
    bool hasUrls = false;
    bool hasImage = false;
};

ClipboardSnapshot captureClipboardSnapshot(const QMimeData *source)
{
    ClipboardSnapshot snapshot;
    if (!source) {
        return snapshot;
    }
    for (const QString &format : source->formats()) {
        snapshot.formats.insert(format, source->data(format));
    }
    snapshot.hasText = source->hasText();
    snapshot.text = source->text();
    snapshot.hasHtml = source->hasHtml();
    snapshot.html = source->html();
    snapshot.hasUrls = source->hasUrls();
    snapshot.urls = source->urls();
    snapshot.hasImage = source->hasImage();
    snapshot.imageData = source->imageData();
    return snapshot;
}

QMimeData *mimeDataFromClipboardSnapshot(const ClipboardSnapshot &snapshot)
{
    QMimeData *data = new QMimeData;
    for (auto it = snapshot.formats.constBegin();
         it != snapshot.formats.constEnd();
         ++it) {
        data->setData(it.key(), it.value());
    }
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

#ifdef Q_OS_WIN
void sendCtrlC()
{
    INPUT inputs[4];
    ZeroMemory(inputs, sizeof(inputs));
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'C';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'C';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
}

quint32 clipboardOwnerProcessId()
{
    const HWND owner = GetClipboardOwner();
    DWORD processId = 0;
    if (owner) {
        GetWindowThreadProcessId(owner, &processId);
    }
    return processId;
}
#endif

} // namespace

class SelectionProbeRunner::Impl
{
public:
    struct Job
    {
        SelectionProbeRequest request;
        bool strongSelectionEnabled = false;
        quint64 generation = 0;
        SelectionProbeRunnerCallbacks callbacks;
        bool validation = false;
        std::function<void(quint64, bool)> validationCompleted;
    };

    struct WorkerResult
    {
        SelectionPhysicalProbeResult physical;
    };

    Impl(SelectionProbeRunner *owner,
         const SelectionProbeRunnerAccess &requestedAccess)
        : q(owner),
          access(requestedAccess)
    {
        if (!access.probeUiAutomationPhysical) {
            access.probeUiAutomationPhysical =
                &SelectedTextReader::probeUiAutomationPhysical;
        }
#ifdef Q_OS_WIN
        if (!access.cancelComCall) {
            access.cancelComCall = [](quint32 workerThreadId) {
                if (workerThreadId) {
                    CoCancelCall(workerThreadId, 0);
                }
            };
        }
        if (!access.clipboardSequenceNumber) {
            access.clipboardSequenceNumber = []() {
                return quint32(GetClipboardSequenceNumber());
            };
        }
        if (!access.clipboardOwnerProcessId) {
            access.clipboardOwnerProcessId = &clipboardOwnerProcessId;
        }
        if (!access.targetStillForeground) {
            access.targetStillForeground = [](
                SelectedTextNativeWindowHandle window) {
                return window
                    && GetForegroundWindow() == static_cast<HWND>(window);
            };
        }
        if (!access.sendCopyShortcut) {
            access.sendCopyShortcut = &sendCtrlC;
        }
#else
        if (!access.cancelComCall) {
            access.cancelComCall = [](quint32) {};
        }
        if (!access.clipboardSequenceNumber) {
            access.clipboardSequenceNumber = []() { return quint32(0); };
        }
        if (!access.clipboardOwnerProcessId) {
            access.clipboardOwnerProcessId = []() { return quint32(0); };
        }
        if (!access.targetStillForeground) {
            access.targetStillForeground = [](
                SelectedTextNativeWindowHandle) { return false; };
        }
        if (!access.sendCopyShortcut) {
            access.sendCopyShortcut = []() {};
        }
#endif

        deadline = new QTimer(q);
        deadline->setSingleShot(true);
        QObject::connect(deadline, &QTimer::timeout, q, [this]() {
            onDeadline();
        });
    }

    ~Impl()
    {
        cancel();
    }

    void start(const Job &job)
    {
        latestGeneration = job.generation;
        if (workerActive || fallbackActive) {
            pending = job;
            hasPending = true;
            return;
        }
        launch(job);
    }

    void launch(const Job &job)
    {
        current = job;
        workerActive = true;
        timedOut = false;
        cancellationRequested = false;
        workerThreadId.reset(new QAtomicInt(0));

        QFutureWatcher<WorkerResult> *jobWatcher =
            new QFutureWatcher<WorkerResult>(q);
        watcher = jobWatcher;
        const SelectionProbeRunnerAccess accessCopy = access;
        const SelectionProbeRequest request = job.request;
        const QSharedPointer<QAtomicInt> threadId = workerThreadId;
        QObject::connect(
            jobWatcher,
            &QFutureWatcher<WorkerResult>::finished,
            q,
            [this, jobWatcher]() {
                const WorkerResult result = jobWatcher->result();
                jobWatcher->deleteLater();
                if (watcher == jobWatcher) {
                    watcher = nullptr;
                }
                onWorkerFinished(result);
            }
        );
        jobWatcher->setFuture(QtConcurrent::run(
            [accessCopy, request, threadId]() {
                WorkerResult result;
#ifdef Q_OS_WIN
                const HRESULT initialized = CoInitializeEx(
                    nullptr,
                    COINIT_APARTMENTTHREADED
                );
                const bool uninitialize = SUCCEEDED(initialized);
                const bool cancellationEnabled =
                    SUCCEEDED(CoEnableCallCancellation(nullptr));
                threadId->storeRelease(int(GetCurrentThreadId()));
#else
                threadId->storeRelease(1);
#endif
                result.physical =
                    accessCopy.probeUiAutomationPhysical(request);
#ifdef Q_OS_WIN
                if (cancellationEnabled) {
                    CoDisableCallCancellation(nullptr);
                }
                if (uninitialize) {
                    CoUninitialize();
                }
#endif
                return result;
            }
        ));
        deadline->start(800);
    }

    void onDeadline()
    {
        if (!workerActive || cancellationRequested) {
            return;
        }
        cancellationRequested = true;
        const quint32 threadId = workerThreadId
            ? quint32(qMax(0, workerThreadId->loadAcquire()))
            : 0;
        access.cancelComCall(threadId);
        timedOut = true;
        if (current.generation == latestGeneration
            && current.callbacks.timedOut) {
            const std::function<void(quint64)> callback =
                current.callbacks.timedOut;
            const quint64 generation = current.generation;
            callback(generation);
        }
    }

    void onWorkerFinished(const WorkerResult &result)
    {
        deadline->stop();
        workerActive = false;
        workerThreadId.clear();
        if (timedOut || current.generation != latestGeneration) {
            finishWithoutDelivery();
            return;
        }

        SelectionSnapshot snapshot = selectionSnapshotFromPhysicalProbe(
            result.physical,
            selectionMonitorGeometries()
        );
        snapshot.generation = current.generation;
        if (current.validation) {
            const bool valid = access.targetStillForeground(
                current.request.targetWindow
            ) && snapshot.isUsable();
            const quint64 generation = current.generation;
            const std::function<void(quint64, bool)> callback =
                current.validationCompleted;
            finishCurrentState();
            if (callback) {
                callback(generation, valid);
            }
            return;
        }

        if (snapshot.sensitivity == SelectionSensitivity::Normal
            && snapshot.text.trimmed().isEmpty()
            && current.strongSelectionEnabled
            && current.request.targetWindow) {
            beginStrongSelectionFallback(snapshot);
            return;
        }
        deliverSnapshot(snapshot);
    }

    void beginStrongSelectionFallback(const SelectionSnapshot &base)
    {
        fallbackActive = true;
        fallbackSnapshot = base;
        clipboardOriginal = captureClipboardSnapshot(
            QApplication::clipboard()->mimeData()
        );
        clipboardSentinel =
            QStringLiteral("__VOCEKIT_SELECTION_SENTINEL__")
            + QUuid::createUuid().toString();
        QApplication::clipboard()->setText(clipboardSentinel);
        sentinelSequence = access.clipboardSequenceNumber();
        clipboardPollCount = 0;
        QTimer::singleShot(60, q, [this]() {
            if (!fallbackActive
                || current.generation != latestGeneration) {
                finishWithoutDelivery();
                return;
            }
            if (!access.targetStillForeground(current.request.targetWindow)) {
                restoreSentinelClipboardIfOwned();
                deliverSnapshot(fallbackSnapshot);
                return;
            }
            access.sendCopyShortcut();
            scheduleClipboardPoll();
        });
    }

    void scheduleClipboardPoll()
    {
        QTimer::singleShot(40, q, [this]() {
            pollClipboard();
        });
    }

    void pollClipboard()
    {
        if (!fallbackActive
            || current.generation != latestGeneration) {
            finishWithoutDelivery();
            return;
        }
        ++clipboardPollCount;
        const QString text = QApplication::clipboard()->text();
        if (text == clipboardSentinel) {
            if (clipboardPollCount < 10) {
                scheduleClipboardPoll();
                return;
            }
            restoreSentinelClipboardIfOwned();
            deliverSnapshot(fallbackSnapshot);
            return;
        }

        const quint32 ownedSequence = access.clipboardSequenceNumber();
        const quint32 ownerProcess = access.clipboardOwnerProcessId();
        const bool foreground = access.targetStillForeground(
            current.request.targetWindow
        );
        if (!selectionClipboardOwnershipMatches(
                ownedSequence,
                ownedSequence,
                fallbackSnapshot.targetProcessId,
                ownerProcess,
                foreground)) {
            deliverSnapshot(fallbackSnapshot);
            return;
        }

        fallbackSnapshot.text = text.trimmed();
        fallbackSnapshot.method =
            SelectionAcquisitionMethod::ClipboardFallback;
        if (selectionClipboardOwnershipMatches(
                ownedSequence,
                access.clipboardSequenceNumber(),
                fallbackSnapshot.targetProcessId,
                access.clipboardOwnerProcessId(),
                access.targetStillForeground(current.request.targetWindow))) {
            QApplication::clipboard()->setMimeData(
                mimeDataFromClipboardSnapshot(clipboardOriginal)
            );
        }
        deliverSnapshot(fallbackSnapshot);
    }

    void restoreSentinelClipboardIfOwned()
    {
#ifdef Q_OS_WIN
        const quint32 ownProcessId = quint32(GetCurrentProcessId());
#else
        const quint32 ownProcessId = access.clipboardOwnerProcessId();
#endif
        if (selectionClipboardOwnershipMatches(
                sentinelSequence,
                access.clipboardSequenceNumber(),
                ownProcessId,
                access.clipboardOwnerProcessId(),
                access.targetStillForeground(current.request.targetWindow))) {
            QApplication::clipboard()->setMimeData(
                mimeDataFromClipboardSnapshot(clipboardOriginal)
            );
        }
    }

    void deliverSnapshot(const SelectionSnapshot &snapshot)
    {
        const quint64 generation = current.generation;
        const std::function<void(quint64, const SelectionSnapshot &)> callback =
            current.callbacks.completed;
        finishCurrentState();
        if (callback) {
            callback(generation, snapshot);
        }
    }

    void finishWithoutDelivery()
    {
        finishCurrentState();
    }

    void finishCurrentState()
    {
        workerActive = false;
        fallbackActive = false;
        timedOut = false;
        cancellationRequested = false;
        current = Job();
        clipboardSentinel.clear();
        if (hasPending) {
            const Job next = pending;
            pending = Job();
            hasPending = false;
            QTimer::singleShot(0, q, [this, next]() {
                if (!workerActive && !fallbackActive) {
                    launch(next);
                }
            });
        }
    }

    void cancel()
    {
        ++latestGeneration;
        hasPending = false;
        pending = Job();
        current.callbacks = SelectionProbeRunnerCallbacks();
        current.validationCompleted =
            std::function<void(quint64, bool)>();
        deadline->stop();
        if (workerActive && !cancellationRequested) {
            cancellationRequested = true;
            const quint32 threadId = workerThreadId
                ? quint32(qMax(0, workerThreadId->loadAcquire()))
                : 0;
            access.cancelComCall(threadId);
        }
        if (fallbackActive) {
            restoreSentinelClipboardIfOwned();
            fallbackActive = false;
            current = Job();
        }
    }

    SelectionProbeRunner *q = nullptr;
    SelectionProbeRunnerAccess access;
    QTimer *deadline = nullptr;
    QFutureWatcher<WorkerResult> *watcher = nullptr;
    QSharedPointer<QAtomicInt> workerThreadId;
    Job current;
    Job pending;
    bool hasPending = false;
    bool workerActive = false;
    bool fallbackActive = false;
    bool timedOut = false;
    bool cancellationRequested = false;
    quint64 latestGeneration = 0;
    ClipboardSnapshot clipboardOriginal;
    SelectionSnapshot fallbackSnapshot;
    QString clipboardSentinel;
    quint32 sentinelSequence = 0;
    int clipboardPollCount = 0;
};

SelectionProbeRunner::SelectionProbeRunner(
    const SelectionProbeRunnerAccess &access,
    QObject *parent)
    : QObject(parent),
      m_impl(new Impl(this, access))
{
}

SelectionProbeRunner::~SelectionProbeRunner()
{
    delete m_impl;
    m_impl = nullptr;
}

void SelectionProbeRunner::start(
    const SelectionProbeRequest &request,
    bool strongSelectionEnabled,
    quint64 generation,
    const SelectionProbeRunnerCallbacks &callbacks)
{
    Impl::Job job;
    job.request = request;
    job.strongSelectionEnabled = strongSelectionEnabled;
    job.generation = generation;
    job.callbacks = callbacks;
    m_impl->start(job);
}

void SelectionProbeRunner::cancel()
{
    m_impl->cancel();
}

bool SelectionProbeRunner::isRunning() const
{
    return m_impl->workerActive
        || m_impl->fallbackActive
        || m_impl->hasPending;
}

void SelectionProbeRunner::validateSelectionAsync(
    SelectedTextNativeWindowHandle window,
    quint64 generation,
    const std::function<void(quint64, bool)> &completed)
{
    Impl::Job job;
    job.request.targetWindow = window;
    job.request.cursorPhysicalPosition = QCursor::pos();
    job.generation = generation;
    job.validation = true;
    job.validationCompleted = completed;
    m_impl->start(job);
}
