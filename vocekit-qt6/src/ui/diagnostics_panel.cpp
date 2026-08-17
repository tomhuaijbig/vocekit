#include "diagnostics_panel.h"

#include "floating_bar_test_card.h"
#include "interface_self_check_card.h"
#include "microphone_input_test_card.h"
#include "network_diagnostics_card.h"
#include "selection_input_test_card.h"
#include "write_input_test_card.h"
#include "ui_style.h"

#include <QtWidgets>
#include <QCoreApplication>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

DiagnosticsPanel::DiagnosticsPanel(
    const FaqMatchCounter &faqMatchCounter,
    const FaqOpener &faqOpener,
    QWidget *parent
)
    : QWidget(parent),
      m_faqMatchCounter(faqMatchCounter),
      m_faqOpener(faqOpener)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr8("测试工具"));
    title->setFont(appFont(24, QFont::DemiBold));
    layout->addWidget(title);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setMinimumHeight(42);
    m_searchEdit->setPlaceholderText(tr8("搜索测试项目或常见问题关键词"));
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
        refreshSearch();
    });
    layout->addWidget(m_searchEdit);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
    ));

    auto *holder = new QWidget;
    m_itemsLayout = new QVBoxLayout(holder);
    m_itemsLayout->setContentsMargins(0, 0, 10, 0);
    m_itemsLayout->setSpacing(12);

    m_faqMatchButton = new QPushButton;
    m_faqMatchButton->setMinimumHeight(42);
    m_faqMatchButton->setStyleSheet(
        compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
    );
    m_faqMatchButton->setVisible(false);
    connect(m_faqMatchButton, &QPushButton::clicked, this, [this]() {
        if (m_faqOpener) {
            m_faqOpener(searchText());
        }
    });
    m_itemsLayout->addWidget(m_faqMatchButton);

    scroll->setWidget(holder);
    layout->addWidget(scroll, 1);
}

void DiagnosticsPanel::addTestCard(QWidget *card)
{
    if (!card || !m_itemsLayout) {
        return;
    }
    if (m_finalized && m_tailStretch) {
        m_itemsLayout->removeItem(m_tailStretch);
        if (m_emptyLabel) {
            m_itemsLayout->removeWidget(m_emptyLabel);
        }
    }
    m_itemsLayout->addWidget(card);
    if (m_finalized) {
        if (m_emptyLabel) {
            m_itemsLayout->addWidget(m_emptyLabel);
        }
        m_itemsLayout->addItem(m_tailStretch);
    }
}

void DiagnosticsPanel::addDefaultCards(const DiagnosticsPanelDefaultCards &cards)
{
    if (!cards.settingsProvider) {
        finalizeCards();
        return;
    }

    FloatingBar *floatingBar = cards.floatingBar;
    DiagnosticsPanelDefaultCards::SettingsProvider settingsProvider =
        cards.settingsProvider;
    DiagnosticsPanelDefaultCards::PathProvider appBasePathProvider = cards.appBasePathProvider;
    DiagnosticsPanelDefaultCards::PathProvider recordDirectoryProvider =
        cards.recordDirectoryProvider;
    DiagnosticsPanelDefaultCards::SecretConfigProvider secretsProvider = cards.secretsProvider;
    if (!appBasePathProvider) {
        appBasePathProvider = []() { return QString(); };
    }
    if (!secretsProvider) {
        secretsProvider = []() { return SecretConfig(); };
    }
    if (!recordDirectoryProvider) {
        recordDirectoryProvider = []() { return QString(); };
    }

    m_interfaceSelfCheckCard = new InterfaceSelfCheckCard(
        [settingsProvider]() { return settingsProvider().useSystemProxy; },
        [settingsProvider]() { return settingsProvider().ocrEngine; },
        [settingsProvider]() {
            return settingsProvider().windowsSpeechLanguage;
        },
        [settingsProvider]() { return settingsProvider().ocrTimeoutMs; },
        appBasePathProvider,
        []() { return QCoreApplication::applicationDirPath(); },
        secretsProvider
    );
    addTestCard(m_interfaceSelfCheckCard);
    m_networkDiagnosticsCard = new NetworkDiagnosticsCard(
        [settingsProvider]() { return settingsProvider().useSystemProxy; },
        secretsProvider
    );
    addTestCard(m_networkDiagnosticsCard);
    addTestCard(new MicrophoneInputTestCard(recordDirectoryProvider));
    addTestCard(new SelectionInputTestCard(
        [settingsProvider]() { return settingsProvider().floatingBarEnabled; },
        floatingBar,
        this
    ));
    addTestCard(new WriteInputTestCard);
    addTestCard(cards.vocabularyTestCard);
    addTestCard(new FloatingBarTestCard(
        [settingsProvider]() { return settingsProvider().floatingBarEnabled; },
        [settingsProvider]() {
            return settingsProvider().dictateFloatingBarSeconds;
        },
        floatingBar
    ));
    addTestCard(cards.resultPopupTestCard);
    finalizeCards();
}

void DiagnosticsPanel::finalizeCards()
{
    if (m_finalized || !m_itemsLayout) {
        refreshSearch();
        return;
    }

    m_emptyLabel = new QLabel(tr8("没有找到匹配的测试项目或常见问题。"));
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background: #f2f4f7;"
        "  color: #667085;"
        "  border-radius: 8px;"
        "  padding: 16px;"
        "}"
    ));
    m_emptyLabel->setVisible(false);
    m_itemsLayout->addWidget(m_emptyLabel);

    m_tailStretch = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    m_itemsLayout->addItem(m_tailStretch);
    m_finalized = true;
    refreshSearch();
}

void DiagnosticsPanel::refreshRuntimeTargets()
{
    if (m_interfaceSelfCheckCard) {
        m_interfaceSelfCheckCard->refreshTargets();
    }
}

void DiagnosticsPanel::hideEvent(QHideEvent *event)
{
    if (m_interfaceSelfCheckCard) {
        m_interfaceSelfCheckCard->cancelCheck();
    }
    if (m_networkDiagnosticsCard) {
        m_networkDiagnosticsCard->cancelCheck();
    }
    QWidget::hideEvent(event);
}

void DiagnosticsPanel::setSearchText(const QString &keyword)
{
    if (!m_searchEdit) {
        return;
    }
    m_searchEdit->setText(keyword);
    refreshSearch();
}

QString DiagnosticsPanel::searchText() const
{
    return m_searchEdit ? m_searchEdit->text().trimmed() : QString();
}

void DiagnosticsPanel::refreshSearch()
{
    if (!m_itemsLayout) {
        return;
    }

    const QString keyword = searchText();
    int visibleCount = 0;
    for (int i = 0; i < m_itemsLayout->count(); ++i) {
        QLayoutItem *item = m_itemsLayout->itemAt(i);
        QWidget *widget = item ? item->widget() : nullptr;
        if (!widget || widget == m_emptyLabel || widget == m_faqMatchButton) {
            continue;
        }
        const QString searchText = widget->property("testSearchText").toString();
        if (searchText.isEmpty()) {
            continue;
        }
        const bool matched = keyword.isEmpty()
            || searchText.contains(keyword, Qt::CaseInsensitive);
        widget->setVisible(matched);
        if (matched) {
            ++visibleCount;
        }
    }

    const int faqMatches = !keyword.isEmpty() && m_faqMatchCounter
        ? m_faqMatchCounter(keyword)
        : 0;
    if (m_faqMatchButton) {
        m_faqMatchButton->setVisible(faqMatches > 0);
        m_faqMatchButton->setText(
            tr8("常见问题找到 ")
            + QString::number(faqMatches)
            + tr8(" 条匹配，点击查看")
        );
    }
    if (m_emptyLabel) {
        m_emptyLabel->setVisible(!keyword.isEmpty() && visibleCount == 0 && faqMatches == 0);
    }
}
