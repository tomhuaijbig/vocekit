#ifndef VOCEKIT_ADVANCED_API_DIALOG_H
#define VOCEKIT_ADVANCED_API_DIALOG_H

class QWidget;

// 高级 API 功能保持在独立对话框中，普通设置和普通聊天流程不受影响。
void showAdvancedApiDialog(bool useSystemProxy, QWidget *parent = nullptr);

#endif // VOCEKIT_ADVANCED_API_DIALOG_H
