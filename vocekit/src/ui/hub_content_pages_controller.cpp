#include "hub_content_pages_controller.h"

#include "history_page_access_factory.h"
#include "history_page_controller.h"
#include "ocr_page_access_factory.h"
#include "ocr_page_controller.h"
#include "vocabulary_page_access_factory.h"
#include "vocabulary_page_controller.h"

#include <QWidget>

class HubContentPagesController::Impl
{
public:
    explicit Impl(const HubContentPagesControllerAccess &controllerAccess)
        : access(controllerAccess)
    {
    }

    HistoryPageController *historyController()
    {
        if (!history) {
            history.reset(new HistoryPageController(
                access.pageParent,
                createHistoryPageAccess(access.settings, access.historyChanged)
            ));
        }
        return history.data();
    }

    VocabularyPageController *vocabularyController()
    {
        if (!vocabulary) {
            VocabularyPageAccessFactoryDependencies dependencies;
            dependencies.settings = access.settings;
            dependencies.vocabularyAi = access.vocabularyAi;
            dependencies.historyEntries = [this]() {
                return historyController()->loadEntries();
            };
            dependencies.vocabularyChanged = access.vocabularyChanged;
            vocabulary.reset(new VocabularyPageController(
                access.pageParent,
                createVocabularyPageAccess(dependencies)
            ));
        }
        return vocabulary.data();
    }

    OcrPageController *ocrController()
    {
        if (!ocr) {
            OcrPageAccessFactoryDependencies dependencies;
            dependencies.settings = access.settings;
            dependencies.historyRecordSaved = access.historyRecordSaved;
            QWidget *dialogParent = access.dialogParent
                ? access.dialogParent
                : access.pageParent;
            ocr.reset(new OcrPageController(
                createOcrPageAccess(dependencies),
                dialogParent,
                access.pageParent
            ));
        }
        return ocr.data();
    }

    HubContentPagesControllerAccess access;
    QScopedPointer<HistoryPageController> history;
    QScopedPointer<VocabularyPageController> vocabulary;
    QScopedPointer<OcrPageController> ocr;
};

HubContentPagesController::HubContentPagesController(
    const HubContentPagesControllerAccess &access
)
    : m_impl(new Impl(access))
{
}

HubContentPagesController::~HubContentPagesController() = default;

QWidget *HubContentPagesController::historyPage()
{
    return m_impl->historyController()->page();
}

QWidget *HubContentPagesController::vocabularyPage()
{
    return m_impl->vocabularyController()->page();
}

QWidget *HubContentPagesController::ocrPage()
{
    return m_impl->ocrController()->page();
}

HubRefreshDataAccess HubContentPagesController::historyRefreshDataAccess()
{
    HubRefreshDataAccess access;
    access.historyCacheValid = [this]() {
        return m_impl->history
            && m_impl->history->historyCacheValid();
    };
    access.refreshHistory = [this](bool forceReload) {
        if (m_impl->history) {
            m_impl->history->refreshTabs(forceReload);
        }
    };
    access.invalidateHistoryCache = [this]() {
        if (m_impl->history) {
            m_impl->history->invalidateCache();
        }
    };
    access.historyPageCreated = [this]() {
        return m_impl->history && m_impl->history->pageCreated();
    };
    return access;
}

QVector<HistoryEntry> HubContentPagesController::historyEntries()
{
    return m_impl->historyController()->loadEntries();
}

QVector<HistoryTabDef> HubContentPagesController::historyTabs()
{
    return m_impl->historyController()->tabModes();
}

QWidget *HubContentPagesController::historyViewForMode(
    const QString &modeId,
    const QVector<HistoryEntry> &entries,
    int maxRows
)
{
    return m_impl->historyController()->viewForMode(
        modeId,
        entries,
        maxRows
    );
}

void HubContentPagesController::refreshHistory(bool forceReload)
{
    if (m_impl->history) {
        m_impl->history->refreshTabs(forceReload);
    }
}

void HubContentPagesController::editVocabularyEntry(
    const VocabularyEntry &entry
)
{
    m_impl->vocabularyController()->editEntry(entry);
}

void HubContentPagesController::refreshVocabulary()
{
    if (m_impl->vocabulary) {
        m_impl->vocabulary->refresh();
    }
}

void HubContentPagesController::refreshOcrConfiguration()
{
    m_impl->ocrController()->refreshConfiguration();
}

void HubContentPagesController::refreshOcrPage()
{
    if (m_impl->ocr) {
        m_impl->ocr->refreshPage();
    }
}
