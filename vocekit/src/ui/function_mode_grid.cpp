#include "function_mode_grid.h"

#include "mode_card_frame.h"
#include "shortcut_display.h"
#include "ui_style.h"

#include "../config/app_settings_defaults.h"
#include "../providers/model_catalog.h"

#include <QtWidgets>

namespace {

QString text8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

FunctionModeGrid::FunctionModeGrid(
    const FunctionModeGridAccess &access,
    QWidget *parent
)
    : QWidget(parent), m_access(access)
{
    m_grid = new QGridLayout(this);
    m_grid->setHorizontalSpacing(14);
    m_grid->setVerticalSpacing(14);
    m_grid->setContentsMargins(0, 0, 0, 0);
}

void FunctionModeGrid::setOpenCallback(const OpenCallback &callback)
{
    m_openCallback = callback;
}

void FunctionModeGrid::setSettingsChangedCallback(
    const SettingsChangedCallback &callback)
{
    m_settingsChangedCallback = callback;
}

void FunctionModeGrid::setWarningCallback(const WarningCallback &callback)
{
    m_warningCallback = callback;
}

void FunctionModeGrid::refresh()
{
    clearLayout(m_grid);

    const QVector<CardData> cards = orderedCards();
    for (int i = 0; i < cards.size(); ++i) {
        m_grid->addWidget(modeCard(cards.at(i)), i / 3, i % 3);
    }
}

QVector<FunctionModeGrid::CardData> FunctionModeGrid::orderedCards() const
{
    QVector<CardData> cards;
    const FunctionModeGridSnapshot snapshot = m_access.snapshotProvider
        ? m_access.snapshotProvider()
        : FunctionModeGridSnapshot();
    const QStringList accents = QStringList()
        << QStringLiteral("#2563eb")
        << QStringLiteral("#059669")
        << QStringLiteral("#7c3aed")
        << QStringLiteral("#db2777")
        << QStringLiteral("#ea580c")
        << QStringLiteral("#0891b2")
        << QStringLiteral("#4f46e5");

    int accentIndex = 0;
    for (const FunctionModeCardSnapshot &source : snapshot.cards) {
        CardData card;
        card.id = source.id;
        card.title = source.title;
        card.shortcut = source.shortcut;
        card.model = source.model;
        card.outputMode = source.outputMode;
        card.accent = accents.at(accentIndex % accents.size());
        card.useSelection = source.useSelection;
        card.useVoice = source.useVoice;
        card.useScreenshot = source.useScreenshot;
        card.custom = source.custom;
        card.customFunction = source.customFunction;
        cards.append(card);
        ++accentIndex;
    }

    QVector<CardData> ordered;
    for (const QString &id : snapshot.order) {
        for (const CardData &card : cards) {
            if (card.id == id) {
                ordered.append(card);
                break;
            }
        }
    }

    for (const CardData &card : cards) {
        bool exists = false;
        for (const CardData &orderedCard : ordered) {
            if (orderedCard.id == card.id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            ordered.append(card);
        }
    }
    return ordered;
}

QStringList FunctionModeGrid::orderedIds(const QVector<CardData> &cards) const
{
    QStringList ids;
    for (const CardData &card : cards) {
        ids.append(card.id);
    }
    return ids;
}

QString FunctionModeGrid::inputModeSummary(const CardData &card) const
{
    QStringList inputs;
    if (card.useSelection) {
        inputs.append(text8("选中文字"));
    }
    if (card.useVoice) {
        inputs.append(text8("语音"));
    }
    if (card.useScreenshot) {
        inputs.append(text8("截图"));
    }
    return inputs.isEmpty() ? text8("未启用") : inputs.join(text8(" + "));
}

QWidget *FunctionModeGrid::modeCard(const CardData &card)
{
    auto *frame = new ModeCardFrame(card.id);
    frame->setObjectName(QStringLiteral("card"));
    frame->setFixedHeight(210);
    frame->setStyleSheet(cardStyle());
    frame->setDropCallback([this](
        const QString &sourceId,
        const QString &targetId,
        bool dropAfter
    ) {
        reorderModeCard(sourceId, targetId, dropAfter);
    });
    frame->setDoubleClickCallback([this, card]() {
        if (m_openCallback) {
            m_openCallback(card.id);
        }
    });

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(6);

    auto *dot = new QLabel;
    dot->setFixedSize(26, 5);
    dot->setStyleSheet(
        QStringLiteral("background: %1; border-radius: 2px;").arg(card.accent)
    );

    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(6);
    topRow->addWidget(dot);
    topRow->addStretch();

    auto *name = new QLabel(card.title);
    name->setFont(appFont(13, QFont::DemiBold));
    name->setStyleSheet(QStringLiteral(
        "background: transparent; color: #111827;"
    ));
    name->setWordWrap(false);
    name->setMinimumHeight(32);
    name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto *key = new QLabel(displayShortcut(card.shortcut));
    key->setStyleSheet(QStringLiteral(
        "background: transparent; color: #344054; font-weight: 600;"
    ));
    key->setMinimumHeight(24);
    key->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *modelLabel = new QLabel(text8("模型：") + modelTitle(card.model));
    modelLabel->setWordWrap(false);
    modelLabel->setFont(appFont(9, QFont::DemiBold));
    modelLabel->setStyleSheet(QStringLiteral(
        "background: transparent; color: #047857; font-weight: 600;"
    ));
    modelLabel->setMinimumHeight(22);
    modelLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *inputLabel = new QLabel(text8("输入：") + inputModeSummary(card));
    inputLabel->setWordWrap(false);
    inputLabel->setFont(appFont(9, QFont::DemiBold));
    inputLabel->setStyleSheet(QStringLiteral(
        "background: transparent; color: #047857; font-weight: 600;"
    ));
    inputLabel->setMinimumHeight(22);
    inputLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *outputLabel = new QLabel(
        text8("展现：") + outputModeTitle(card.outputMode)
    );
    outputLabel->setWordWrap(false);
    outputLabel->setFont(appFont(9, QFont::DemiBold));
    outputLabel->setStyleSheet(QStringLiteral(
        "background: transparent; color: #047857; font-weight: 600;"
    ));
    outputLabel->setMinimumHeight(22);
    outputLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    layout->addLayout(topRow);
    layout->addWidget(name);
    layout->addWidget(key);
    layout->addWidget(modelLabel);
    layout->addWidget(inputLabel);
    layout->addWidget(outputLabel);
    layout->addStretch();

    const QList<QWidget *> childWidgets = frame->findChildren<QWidget *>();
    for (QWidget *child : childWidgets) {
        child->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    return frame;
}

void FunctionModeGrid::reorderModeCard(
    const QString &sourceId,
    const QString &targetId,
    bool dropAfter
)
{
    QStringList ids = orderedIds(orderedCards());
    const int sourceIndex = ids.indexOf(sourceId);
    int targetIndex = ids.indexOf(targetId);
    if (sourceIndex < 0 || targetIndex < 0 || sourceIndex == targetIndex) {
        return;
    }

    ids.removeAt(sourceIndex);
    if (sourceIndex < targetIndex) {
        --targetIndex;
    }
    int insertIndex = dropAfter ? targetIndex + 1 : targetIndex;
    insertIndex = qBound(0, insertIndex, ids.size());
    ids.insert(insertIndex, sourceId);

    QString error;
    if (!m_access.saveOrder || !m_access.saveOrder(ids, &error)) {
        if (!m_warningCallback) {
            return;
        }
        m_warningCallback(
            text8("保存失败"),
            error.isEmpty()
                ? text8("无法写入 config/settings.json。")
                : error
        );
        return;
    }
    refresh();
    if (m_settingsChangedCallback) {
        m_settingsChangedCallback();
    }
}

void FunctionModeGrid::clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        if (item->layout()) {
            clearLayout(item->layout());
        }
        delete item;
    }
}
