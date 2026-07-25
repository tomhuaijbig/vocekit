#include "vocabulary_page.h"

#include "tab_bar_wheel_filter.h"
#include "ui_style.h"
#include "../storage/vocabulary_store.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

void configureVocabularyTabs(QTabWidget *tabs)
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

} // namespace

VocabularyPage::VocabularyPage(
    const ScopeProvider &scopeProvider,
    const EntryProvider &entryProvider,
    QWidget *parent
)
    : QWidget(parent),
      m_scopeProvider(scopeProvider),
      m_entryProvider(entryProvider)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto *top = new QHBoxLayout;
    auto *title = new QLabel(tr8("词库"));
    title->setFont(appFont(24, QFont::DemiBold));

    auto *add = new QPushButton(tr8("新增词条"));
    add->setMinimumSize(104, 38);
    add->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    connect(add, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.addEntry) {
            m_callbacks.addEntry();
        }
    });

    auto *recommendButton = new QPushButton(tr8("候选推荐"));
    recommendButton->setMinimumSize(104, 38);
    recommendButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(recommendButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.showCandidates) {
            m_callbacks.showCandidates();
        }
    });

    auto *importButton = new QPushButton(tr8("导入"));
    importButton->setMinimumSize(76, 38);
    importButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(importButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.importEntries) {
            m_callbacks.importEntries();
        }
    });

    auto *exportButton = new QPushButton(tr8("导出"));
    exportButton->setMinimumSize(76, 38);
    exportButton->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.exportEntries) {
            m_callbacks.exportEntries();
        }
    });

    auto *openFolder = new QPushButton(tr8("打开目录"));
    openFolder->setMinimumSize(98, 38);
    openFolder->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
    connect(openFolder, &QPushButton::clicked, this, [this]() {
        if (m_callbacks.openDirectory) {
            m_callbacks.openDirectory();
        }
    });

    top->addWidget(title, 1);
    top->addWidget(add);
    top->addWidget(recommendButton);
    top->addWidget(importButton);
    top->addWidget(exportButton);
    top->addWidget(openFolder);
    layout->addLayout(top);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setMinimumHeight(40);
    m_searchEdit->setPlaceholderText(tr8("搜索原词、标准词、别名、备注或作用范围"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: #ffffff;"
        "  border: 1px solid #d0d5dd;"
        "  border-radius: 8px;"
        "  padding: 0 12px;"
        "  color: #111827;"
        "}"
    ));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() {
        refresh();
    });
    layout->addWidget(m_searchEdit);

    m_tabs = new QTabWidget;
    configureVocabularyTabs(m_tabs);
    m_tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid #dde2ea; background: #ffffff; border-radius: 8px; }"
        "QTabBar::tab { padding: 9px 16px; color: #667085; }"
        "QTabBar::tab:selected { color: #111827; font-weight: 600; }"
        "QTabBar QToolButton { width: 28px; background: #ffffff; border: 1px solid #d0d5dd; color: #111827; }"
        "QTabBar QToolButton:hover { background: #eef2ff; }"
    ));
    layout->addWidget(m_tabs, 1);
}

void VocabularyPage::setCallbacks(const VocabularyPageCallbacks &callbacks)
{
    m_callbacks = callbacks;
}

void VocabularyPage::refresh()
{
    if (!m_tabs) {
        return;
    }
    const int previousIndex = qMax(0, m_tabs->currentIndex());
    while (m_tabs->count() > 0) {
        QWidget *page = m_tabs->widget(0);
        m_tabs->removeTab(0);
        if (page) {
            page->deleteLater();
        }
    }

    const QVector<VocabularyScopeOption> scopeOptions = scopes();
    for (const VocabularyScopeOption &scope : scopeOptions) {
        m_tabs->addTab(tabContent(scope.id, scope.title), scope.title);
    }
    if (m_tabs->count() > 0) {
        m_tabs->setCurrentIndex(qMin(previousIndex, m_tabs->count() - 1));
    }
}

QString VocabularyPage::currentScopeId() const
{
    if (!m_tabs) {
        return QStringLiteral("__all");
    }
    QWidget *page = m_tabs->currentWidget();
    const QString scope = page ? page->property("vocabularyScope").toString() : QString();
    return scope.trimmed().isEmpty() ? QStringLiteral("__all") : scope;
}

QString VocabularyPage::searchText() const
{
    return m_searchEdit ? m_searchEdit->text().trimmed() : QString();
}

QVector<VocabularyEntry> VocabularyPage::currentFilteredEntries() const
{
    const QString scopeId = currentScopeId();
    const QString keyword = searchText();
    const QVector<VocabularyScopeOption> scopeOptions = scopes();
    const QVector<VocabularyEntry> source = entries();
    QVector<VocabularyEntry> filtered;
    filtered.reserve(source.size());
    for (const VocabularyEntry &entry : source) {
        if (vocabularyEntryInScope(entry, scopeId)
            && vocabularyEntryMatchesSearch(entry, keyword, scopeOptions)) {
            filtered.append(entry);
        }
    }
    return filtered;
}

QWidget *VocabularyPage::tabContent(const QString &scopeId, const QString &scopeTitle)
{
    auto *page = new QWidget;
    page->setProperty("vocabularyScope", scopeId);
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

    int visibleCount = 0;
    const QVector<VocabularyScopeOption> scopeOptions = scopes();
    const QString keyword = searchText();
    const QVector<VocabularyEntry> source = entries();
    for (const VocabularyEntry &entry : source) {
        if (vocabularyEntryInScope(entry, scopeId)
            && vocabularyEntryMatchesSearch(entry, keyword, scopeOptions)) {
            items->addWidget(entryCard(entry));
            ++visibleCount;
        }
    }
    if (visibleCount == 0) {
        Q_UNUSED(scopeTitle);
        items->addWidget(emptyCard());
    }
    items->addStretch();
    scroll->setWidget(holder);
    layout->addWidget(scroll, 1);
    return page;
}

QWidget *VocabularyPage::entryCard(const VocabularyEntry &entry)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *title = new QLabel(entry.source + tr8(" → ") + entry.target);
    title->setFont(appFont(13, QFont::DemiBold));
    title->setWordWrap(true);
    auto *edit = new QPushButton(tr8("编辑"));
    edit->setFixedSize(78, 38);
    edit->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    auto *remove = new QPushButton(tr8("删除"));
    remove->setFixedSize(78, 38);
    remove->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#b42318")));
    top->addWidget(title, 1);
    top->addWidget(edit);
    top->addWidget(remove);
    layout->addLayout(top);

    auto *meta = new QLabel(
        vocabularyScopeTitleForId(entry.scopeId, scopes())
        + tr8(" · ") + vocabularyMatchModeTitle(entry.matchMode)
        + tr8(" · ") + (entry.enabled ? tr8("已启用") : tr8("已关闭"))
    );
    meta->setWordWrap(true);
    meta->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));
    layout->addWidget(meta);

    if (!entry.aliases.trimmed().isEmpty()) {
        auto *aliases = new QLabel(tr8("别名：") + entry.aliases.trimmed());
        aliases->setWordWrap(true);
        aliases->setStyleSheet(QStringLiteral("color: #475467;"));
        layout->addWidget(aliases);
    }
    if (!entry.note.trimmed().isEmpty()) {
        auto *note = new QLabel(entry.note.trimmed());
        note->setWordWrap(true);
        note->setStyleSheet(QStringLiteral("color: #475467;"));
        layout->addWidget(note);
    }

    connect(edit, &QPushButton::clicked, this, [this, entry]() {
        if (m_callbacks.editEntry) {
            m_callbacks.editEntry(entry);
        }
    });
    connect(remove, &QPushButton::clicked, this, [this, entry]() {
        if (m_callbacks.deleteEntry) {
            m_callbacks.deleteEntry(entry.id, entry.source);
        }
    });
    return frame;
}

QWidget *VocabularyPage::emptyCard() const
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *title = new QLabel(searchText().isEmpty() ? tr8("暂无词条") : tr8("没有匹配词条"));
    title->setFont(appFont(13, QFont::DemiBold));

    auto *structure = new QLabel(tr8("词条将包含：原词/错词、标准写法、别名、作用范围、匹配方式、备注、启用状态。"));
    structure->setWordWrap(true);
    structure->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));

    layout->addWidget(title);
    layout->addWidget(structure);
    return frame;
}

QVector<VocabularyScopeOption> VocabularyPage::scopes() const
{
    return m_scopeProvider ? m_scopeProvider() : builtinVocabularyScopeOptions();
}

QVector<VocabularyEntry> VocabularyPage::entries() const
{
    return m_entryProvider ? m_entryProvider() : QVector<VocabularyEntry>();
}
