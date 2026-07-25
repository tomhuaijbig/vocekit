#ifndef VOCEKIT_VOCABULARY_QUICK_ADD_DIALOG_H
#define VOCEKIT_VOCABULARY_QUICK_ADD_DIALOG_H

#include "../controllers/vocabulary_quick_add_controller.h"

class QWidget;

// “每次询问”模式的界面适配器，业务控制器只接收选择结果。
VocabularyQuickAddChoice askVocabularyQuickAddChoice(QWidget *parent);

#endif // VOCEKIT_VOCABULARY_QUICK_ADD_DIALOG_H
