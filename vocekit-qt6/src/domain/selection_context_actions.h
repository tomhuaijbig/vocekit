#ifndef VOCEKIT_SELECTION_CONTEXT_ACTIONS_H
#define VOCEKIT_SELECTION_CONTEXT_ACTIONS_H

#include <QString>
#include <QStringList>

QString selectionContextActionAiSearch();
QString selectionContextActionTranslate();
QString selectionContextActionExplain();
QString selectionContextActionSave();
QString selectionContextActionCopy();

QString selectionContextActionForFunction(const QString &functionId);
QString selectionContextFunctionId(const QString &actionId);
bool isSelectionContextFunctionAction(const QString &actionId);

QString selectionContextMenuBlockApplication();
QString selectionContextMenuOpenSettings();

QStringList defaultSelectionContextActionOrder();
QStringList normalizeSelectionContextActionOrder(const QStringList &values);
QString selectionContextActionTitle(const QString &id);

#endif // VOCEKIT_SELECTION_CONTEXT_ACTIONS_H
