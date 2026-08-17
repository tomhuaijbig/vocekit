#include "function_canvas_palette.h"

#include "function_canvas_visual_style.h"
#include "ui_style.h"

#include <QApplication>
#include <QDrag>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

class DraggableNodeButton : public QPushButton
{
public:
    DraggableNodeButton(
        FunctionFlowNodeType type,
        const QString &title,
        const FunctionCanvasPalette::DragRunner &dragRunner,
        QWidget *parent)
        : QPushButton(title, parent),
          m_type(type),
          m_dragRunner(dragRunner)
    {
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(34);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event && event->button() == Qt::LeftButton) {
            m_pressPosition = event->pos();
            m_dragging = false;
        }
        QPushButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging) {
            if (event) {
                event->accept();
            }
            return;
        }
        if (!event
            || !(event->buttons() & Qt::LeftButton)
            || (event->pos() - m_pressPosition).manhattanLength()
                < QApplication::startDragDistance()) {
            QPushButton::mouseMoveEvent(event);
            return;
        }
        m_dragging = true;
        setDown(false);
        QDrag drag(this);
        QMimeData *mime = new QMimeData;
        mime->setData(
            functionCanvasNodeMimeType(),
            functionFlowNodeTypeId(m_type).toUtf8()
        );
        drag.setMimeData(mime);
        m_dragRunner(drag);
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event
            && event->button() == Qt::LeftButton
            && m_dragging) {
            setDown(false);
            m_dragging = false;
            event->accept();
            return;
        }
        m_dragging = false;
        QPushButton::mouseReleaseEvent(event);
    }

private:
    FunctionFlowNodeType m_type;
    FunctionCanvasPalette::DragRunner m_dragRunner;
    QPoint m_pressPosition;
    bool m_dragging = false;
};

struct EntryDefinition
{
    FunctionFlowNodeType type;
    const char *title;
    const char *keywords;
    int category;
};

const EntryDefinition kEntries[] = {
    {FunctionFlowNodeType::VoiceSource, "语音采集", "语音 麦克风 录音 来源", 0},
    {FunctionFlowNodeType::SelectionSource, "选中文字", "选中 文字 来源", 0},
    {FunctionFlowNodeType::ScreenshotSource, "截图识别", "截图 OCR 来源", 0},
    {FunctionFlowNodeType::Input, "输入节点", "输入 数据 整理", 1},
    {FunctionFlowNodeType::Model, "调用大模型", "模型 LLM 翻译 润色 总结", 1},
    {FunctionFlowNodeType::Output, "输出节点", "输出 数据 整理", 1},
    {FunctionFlowNodeType::ResultPopup, "结果小框", "结果 弹窗 动作", 2},
    {FunctionFlowNodeType::ScreenshotPanel, "截图对照窗", "截图 对照 结果 动作", 2},
    {FunctionFlowNodeType::AutoWrite, "自动写入", "写入 替换 插入 动作", 2}
};

struct CategoryDefinition
{
    const char *title;
    const char *objectName;
};

const CategoryDefinition kCategories[] = {
    {"内容来源", "flowPaletteCategorySources"},
    {"内容处理", "flowPaletteCategoryProcessing"},
    {"结果动作", "flowPaletteCategoryActions"}
};

Qt::DropAction runNativeDrag(QDrag &drag)
{
    return drag.exec(Qt::CopyAction);
}

} // namespace

QString functionCanvasNodeMimeType()
{
    return QStringLiteral(
        "application/x-vocekit-function-flow-node"
    );
}

FunctionCanvasPalette::FunctionCanvasPalette(
    QWidget *parent,
    const DragRunner &dragRunner)
    : QWidget(parent),
      m_dragRunner(
          dragRunner
              ? dragRunner
              : DragRunner(runNativeDrag)
      )
{
    setObjectName(QStringLiteral("functionCanvasPalette"));
    setMinimumWidth(224);
    setMaximumWidth(248);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    auto *title = new QLabel(QString::fromUtf8("节点库"));
    title->setFont(appFont(13, QFont::DemiBold));
    layout->addWidget(title);

    m_filter = new QLineEdit;
    m_filter->setObjectName(QStringLiteral("flowPaletteSearch"));
    m_filter->setPlaceholderText(QString::fromUtf8("搜索节点"));
    m_filter->setClearButtonEnabled(true);
    layout->addWidget(m_filter);

    for (const CategoryDefinition &definition : kCategories) {
        Category category;
        category.container = new QWidget(this);
        category.container->setObjectName(
            QString::fromLatin1(definition.objectName)
        );
        category.layout = new QVBoxLayout(category.container);
        category.layout->setContentsMargins(0, 2, 0, 0);
        category.layout->setSpacing(4);
        QLabel *categoryTitle = new QLabel(
            QString::fromUtf8(definition.title),
            category.container
        );
        categoryTitle->setFont(appFont(9, QFont::DemiBold));
        categoryTitle->setStyleSheet(QStringLiteral(
            "QLabel { color:#64748b; padding:2px 4px; }"
        ));
        category.layout->addWidget(categoryTitle);
        layout->addWidget(category.container);
        m_categories.append(category);
    }

    for (const EntryDefinition &definition : kEntries) {
        Entry entry;
        entry.type = definition.type;
        entry.title = QString::fromUtf8(definition.title);
        entry.keywords = QString::fromUtf8(definition.keywords);
        entry.category = definition.category;
        entry.button = new DraggableNodeButton(
            entry.type,
            QStringLiteral("%1  %2").arg(
                functionCanvasNodeGlyph(entry.type),
                functionCanvasNodeDisplayName(entry.type)
            ),
            m_dragRunner,
            m_categories[entry.category].container
        );
        entry.button->setObjectName(
            QStringLiteral("flowPalette_%1")
                .arg(functionFlowNodeTypeId(entry.type))
        );
        entry.button->setToolTip(
            QString::fromUtf8("点击放置，或拖到画布")
        );
        entry.button->setFont(appFont(10));
        entry.button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            " text-align:left; padding:7px 10px;"
            " border:1px solid #dbe3ee;"
            " border-left:3px solid %1;"
            " border-radius:6px; background:#ffffff;"
            " color:#172033;"
            "}"
            "QPushButton:hover { background:#f7f9fc; }"
            "QPushButton:pressed { background:#eef2f8; }"
        ).arg(functionCanvasNodeAccent(entry.type).name()));
        const FunctionFlowNodeType type = entry.type;
        connect(
            entry.button,
            &QPushButton::clicked,
            this,
            [this, type]() {
                Q_EMIT nodeTypeChosen(type);
            }
        );
        m_categories[entry.category].layout->addWidget(
            entry.button
        );
        m_entries.append(entry);
    }
    m_empty = new QLabel(
        QString::fromUtf8("没有匹配的节点"),
        this
    );
    m_empty->setObjectName(QStringLiteral("flowPaletteEmptyLabel"));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setStyleSheet(QStringLiteral(
        "QLabel { color:#778397; padding:20px 8px; }"
    ));
    m_empty->hide();
    layout->addWidget(m_empty);
    layout->addStretch();
    connect(
        m_filter,
        &QLineEdit::textChanged,
        this,
        &FunctionCanvasPalette::setFilterText
    );
}

QVector<FunctionFlowNodeType>
FunctionCanvasPalette::nodeTypes() const
{
    QVector<FunctionFlowNodeType> types;
    for (const Entry &entry : m_entries) {
        types.append(entry.type);
    }
    return types;
}

void FunctionCanvasPalette::setFilterText(const QString &text)
{
    const QString query = text.trimmed();
    if (m_filter && m_filter->text() != text) {
        m_filter->setText(text);
    }
    for (const Entry &entry : m_entries) {
        const bool matches = query.isEmpty()
            || entry.title.contains(query, Qt::CaseInsensitive)
            || entry.keywords.contains(query, Qt::CaseInsensitive)
            || functionFlowNodeTypeId(entry.type).contains(
                query,
                Qt::CaseInsensitive
            );
        entry.button->setVisible(matches);
    }
    for (int categoryIndex = 0;
         categoryIndex < m_categories.size();
         ++categoryIndex) {
        bool categoryHasMatch = false;
        for (const Entry &entry : m_entries) {
            if (entry.category == categoryIndex
                && !entry.button->isHidden()) {
                categoryHasMatch = true;
                break;
            }
        }
        m_categories[categoryIndex].container->setVisible(
            categoryHasMatch
        );
    }
    if (m_empty) {
        m_empty->setVisible(visibleNodeCount() == 0);
    }
}

int FunctionCanvasPalette::visibleNodeCount() const
{
    int count = 0;
    for (const Entry &entry : m_entries) {
        if (!entry.button->isHidden()) {
            ++count;
        }
    }
    return count;
}
