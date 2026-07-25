#ifndef VOCEKIT_HUB_CONTENT_PAGES_CONTROLLER_H
#define VOCEKIT_HUB_CONTENT_PAGES_CONTROLLER_H

#include "../domain/app_legacy_types.h"
#include "../domain/history_modes.h"
#include "../domain/history_types.h"
#include "../domain/vocabulary_io.h"
#include "hub_refresh_coordinator_action_factory.h"

#include <QScopedPointer>
#include <QStringList>
#include <QVector>

#include <functional>

class HubSettingsState;
class QWidget;

// 内容页面装配输入：集中提供历史、词库和图片识别页面共享的状态与通知。
struct HubContentPagesControllerAccess
{
    HubSettingsState *settings = nullptr;
    QWidget *pageParent = nullptr;
    QWidget *dialogParent = nullptr;
    VocabularyAiCallback vocabularyAi;
    std::function<void(const QStringList &, bool)> historyChanged;
    std::function<void(const QStringList &, bool)> vocabularyChanged;
    std::function<void(const QString &)> historyRecordSaved;
};

// 集中管理历史、词库和图片识别页面的生命周期及跨页数据访问。
class HubContentPagesController
{
public:
    explicit HubContentPagesController(
        const HubContentPagesControllerAccess &access
    );
    ~HubContentPagesController();

    QWidget *historyPage();
    QWidget *vocabularyPage();
    QWidget *ocrPage();

    HubRefreshDataAccess historyRefreshDataAccess();
    QVector<HistoryEntry> historyEntries();
    QVector<HistoryTabDef> historyTabs();
    QWidget *historyViewForMode(
        const QString &modeId,
        const QVector<HistoryEntry> &entries,
        int maxRows = 0
    );

    void refreshHistory(bool forceReload = false);
    void editVocabularyEntry(
        const VocabularyEntry &entry = VocabularyEntry()
    );
    void refreshVocabulary();
    void refreshOcrConfiguration();
    void refreshOcrPage();

private:
    HubContentPagesController(const HubContentPagesController &) = delete;
    HubContentPagesController &operator=(
        const HubContentPagesController &
    ) = delete;

    class Impl;
    QScopedPointer<Impl> m_impl;
};

#endif // VOCEKIT_HUB_CONTENT_PAGES_CONTROLLER_H
