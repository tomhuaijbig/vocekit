#include "vocabulary_quick_add_dialog.h"

#include <QMessageBox>
#include <QPushButton>

namespace {

QString dialogText(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

VocabularyQuickAddChoice askVocabularyQuickAddChoice(QWidget *parent)
{
    QMessageBox box(parent);
    box.setWindowFlags(
        box.windowFlags() & ~Qt::WindowContextHelpButtonHint
    );
    box.setWindowTitle(dialogText("加入词库"));
    box.setText(dialogText("这次加入词库要使用 AI 自动生成词条吗？"));
    box.setInformativeText(
        dialogText("这个弹窗只会在词库页选择“每次询问”时出现。")
    );
    box.setIcon(QMessageBox::Question);
    QPushButton * const ai = box.addButton(
        dialogText("使用 AI"),
        QMessageBox::AcceptRole
    );
    QPushButton * const manual = box.addButton(
        dialogText("手动填写"),
        QMessageBox::ActionRole
    );
    box.addButton(dialogText("取消"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == ai) {
        return VocabularyQuickAddChoice::UseAi;
    }
    if (box.clickedButton() == manual) {
        return VocabularyQuickAddChoice::Manual;
    }
    return VocabularyQuickAddChoice::Cancel;
}
