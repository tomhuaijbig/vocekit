#ifndef VOCEKIT_FUNCTION_MODE_GRID_H
#define VOCEKIT_FUNCTION_MODE_GRID_H

#include "../domain/app_legacy_types.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QGridLayout;
class QLayout;

// 主页卡片使用的只读数据，不依赖设置文件的具体实现。
struct FunctionModeCardSnapshot
{
    QString id;
    QString title;
    QString shortcut;
    QString model;
    QString outputMode;
    bool useSelection = false;
    bool useVoice = false;
    bool useScreenshot = false;
    bool custom = false;
    CustomFunctionDef customFunction;
};

struct FunctionModeGridSnapshot
{
    QVector<FunctionModeCardSnapshot> cards;
    QStringList order;
};

struct FunctionModeGridAccess
{
    std::function<FunctionModeGridSnapshot()> snapshotProvider;
    std::function<bool(const QStringList &, QString *)> saveOrder;
};

// Home page function-card grid. It owns card rendering and drag reorder logic.
class FunctionModeGrid : public QWidget
{
public:
    using OpenCallback = std::function<void(const QString &id)>;
    using SettingsChangedCallback = std::function<void()>;
    using WarningCallback = std::function<void(
        const QString &title,
        const QString &message
    )>;

    explicit FunctionModeGrid(
        const FunctionModeGridAccess &access,
        QWidget *parent = nullptr
    );

    void setOpenCallback(const OpenCallback &callback);
    void setSettingsChangedCallback(const SettingsChangedCallback &callback);
    void setWarningCallback(const WarningCallback &callback);

    void refresh();

private:
    struct CardData
    {
        QString id;
        QString title;
        QString shortcut;
        QString model;
        QString outputMode;
        QString accent;
        bool useSelection = false;
        bool useVoice = false;
        bool useScreenshot = false;
        bool custom = false;
        CustomFunctionDef customFunction;
    };

    QVector<CardData> orderedCards() const;
    QStringList orderedIds(const QVector<CardData> &cards) const;
    QString inputModeSummary(const CardData &card) const;
    QWidget *modeCard(const CardData &card);
    void reorderModeCard(
        const QString &sourceId,
        const QString &targetId,
        bool dropAfter
    );
    void clearLayout(QLayout *layout);

    FunctionModeGridAccess m_access;
    QGridLayout *m_grid = nullptr;
    OpenCallback m_openCallback;
    SettingsChangedCallback m_settingsChangedCallback;
    WarningCallback m_warningCallback;
};

#endif // VOCEKIT_FUNCTION_MODE_GRID_H
