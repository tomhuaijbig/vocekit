#include "vocabulary_candidates_dialog.h"

#include "attention_message.h"
#include "ui_style.h"

#include <QtWidgets>

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

VocabularyCandidatesDialog::VocabularyCandidatesDialog(
    const QVector<VocabularyCandidate> &candidates,
    const QVector<VocabularyScopeOption> &scopes,
    QWidget *parent
)
    : AppDialog(parent),
      m_candidates(candidates),
      m_scopes(scopes.isEmpty() ? builtinVocabularyScopeOptions() : scopes)
{
    setWindowTitle(tr8("词库候选推荐"));
    setMinimumSize(760, 560);
    setStyleSheet(QStringLiteral("QDialog { background: #f6f7f9; } QLabel { color: #111827; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(tr8("词库候选推荐"));
    title->setFont(appFont(20, QFont::DemiBold));
    layout->addWidget(title);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *holder = new QWidget;
    auto *items = new QVBoxLayout(holder);
    items->setContentsMargins(0, 0, 8, 0);
    items->setSpacing(10);
    for (const VocabularyCandidate &candidate : m_candidates) {
        items->addWidget(candidateCard(candidate));
    }
    items->addStretch();
    scroll->setWidget(holder);
    layout->addWidget(scroll, 1);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    auto *close = new QPushButton(tr8("关闭"));
    close->setFixedSize(90, 40);
    close->setStyleSheet(buttonStyle(QStringLiteral("#111827")));
    buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}

void VocabularyCandidatesDialog::setEditCallback(const EditCallback &callback)
{
    m_editCallback = callback;
}

void VocabularyCandidatesDialog::setAddCallback(const AddCallback &callback)
{
    m_addCallback = callback;
}

QWidget *VocabularyCandidatesDialog::candidateCard(const VocabularyCandidate &candidate)
{
    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("card"));
    frame->setStyleSheet(cardStyle());
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout;
    auto *title = new QLabel(candidate.entry.source + tr8(" -> ") + candidate.entry.target);
    title->setFont(appFont(12, QFont::DemiBold));
    title->setWordWrap(true);

    auto *edit = new QPushButton(tr8("编辑"));
    edit->setFixedSize(78, 38);
    edit->setStyleSheet(buttonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));

    auto *add = new QPushButton(tr8("加入"));
    add->setFixedSize(78, 38);
    add->setStyleSheet(buttonStyle(QStringLiteral("#111827")));

    top->addWidget(title, 1);
    top->addWidget(edit);
    top->addWidget(add);
    layout->addLayout(top);

    auto *meta = new QLabel(
        tr8("范围：") + scopeTitle(candidate.entry.scopeId)
        + tr8(" · 出现 ") + QString::number(candidate.score) + tr8(" 次")
    );
    meta->setWordWrap(true);
    meta->setStyleSheet(QStringLiteral("color: #047857; font-weight: 600;"));
    layout->addWidget(meta);

    auto *reason = new QLabel(candidate.reason);
    reason->setWordWrap(true);
    reason->setStyleSheet(QStringLiteral("color: #475467;"));
    layout->addWidget(reason);

    connect(edit, &QPushButton::clicked, this, [this, candidate]() {
        if (m_editCallback) {
            m_editCallback(candidate.entry);
        }
    });
    connect(add, &QPushButton::clicked, this, [this, candidate, add]() {
        if (!m_addCallback) {
            return;
        }
        QString error;
        if (!m_addCallback(candidate.entry, &error)) {
            showAttentionWarning(this, tr8("加入失败"), error.isEmpty() ? tr8("无法加入词条。") : error);
            return;
        }
        add->setEnabled(false);
        add->setText(tr8("已加入"));
    });

    return frame;
}

QString VocabularyCandidatesDialog::scopeTitle(const QString &scopeId) const
{
    return vocabularyScopeTitleForId(scopeId, m_scopes);
}
