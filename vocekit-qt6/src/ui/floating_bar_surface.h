#ifndef VOCEKIT_FLOATING_BAR_SURFACE_H
#define VOCEKIT_FLOATING_BAR_SURFACE_H

#include <QString>
#include <QWidget>

#include <functional>

enum class FloatingBarStage
{
    Preparing,
    Recording,
    Recognizing,
    ModelProcessing,
    Writing,
    Completed,
    Failed,
    Streaming,
    StreamingFinalizing,
    StreamingFallback
};

struct FloatingBarViewState
{
    FloatingBarStage stage = FloatingBarStage::Preparing;
    QString title;
    QString detail;
    QString committedText;
    QString provisionalText;
    int waveformPeak = 0;
    bool waveformVisible = false;
    bool cancelEnabled = false;
    bool confirmEnabled = false;
};

struct FloatingBarActions
{
    std::function<void()> cancel;
    std::function<void()> confirm;
};

class FloatingBarSurface : public QWidget
{
public:
    explicit FloatingBarSurface(QWidget *parent = nullptr)
        : QWidget(parent) {}
    virtual ~FloatingBarSurface() {}
    virtual void render(
        const FloatingBarViewState &state,
        const FloatingBarActions &actions
    ) = 0;
};

FloatingBarSurface *createFloatingBarSurface(
    const QString &style,
    QWidget *parent
);

#endif // VOCEKIT_FLOATING_BAR_SURFACE_H
