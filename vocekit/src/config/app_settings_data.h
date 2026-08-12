#ifndef VOCEKIT_APP_SETTINGS_DATA_H
#define VOCEKIT_APP_SETTINGS_DATA_H

#include "../domain/function_settings.h"

#include <QJsonObject>
#include <QMap>
#include <QPoint>
#include <QRect>
#include <QStringList>
#include <QVector>

// 只保存窗口位置和尺寸，避免把界面状态继续平铺到应用配置中。
struct WindowStateSettings
{
    bool hasFloatingBarPosition = false;
    QPoint floatingBarPosition;
    bool hasResultPopupGeometry = false;
    QRect resultPopupGeometry;
    bool hasScreenshotResultGeometry = false;
    QRect screenshotResultGeometry;
    int screenshotResultOpacity = 92;
    bool hasScreenshotLauncherPosition = false;
    QPoint screenshotLauncherPosition;
};

// 应用级设置。每个具体功能的设置统一放在 functions 中。
struct AppSettingsData
{
    bool trayResident = true;
    bool autoStartEnabled = false;
    bool strongSelectionEnabled = false;
    bool floatingBarEnabled = true;
    QString floatingBarStyle = QStringLiteral("statusPill");
    bool writeFailurePopupFallbackEnabled = true;
    bool streamingSpeechRecognitionEnabled = true;
    bool promptLocked = false;
    bool dictatePolishEnabled = false;
    bool useSystemProxy = false;
    int resultPopupOpacity = 100;
    bool preRecordCountdownEnabled = false;
    bool recordingBeepEnabled = false;

    bool vocabularyEnabled = true;
    QString vocabularyAddMode = QStringLiteral("ask");
    bool vocabularyOnlyForVoiceInput = false;
    int vocabularyPromptEntryLimit = 16;

    QString speechProvider = QStringLiteral("baidu");
    QString ocrEngine = QStringLiteral("automatic");
    int ocrTimeoutMs = 45000;
    QString recordDirectory;
    QString targetLanguage = QStringLiteral("简体中文");

    int historyInitialLoadCount = 12;
    int historyLoadMoreCount = 25;
    int logInitialLoadCount = 20;
    int logLoadMoreCount = 30;

    QStringList favoriteFolders;
    QMap<QString, QString> applicationHotkeys;
    QVector<FunctionSettings> functions;
    QStringList functionOrder;
    WindowStateSettings windows;

    // 迁移期间保留尚未建模的新字段，防止旧版本设置被重写丢失。
    QJsonObject retainedRootValues;
    QJsonObject retainedOrphanFunctionFlows;

    int functionIndex(const QString &id) const
    {
        for (int i = 0; i < functions.size(); ++i) {
            if (functions.at(i).id == id) {
                return i;
            }
        }
        return -1;
    }

    const FunctionSettings &function(const QString &id) const
    {
        const int index = functionIndex(id);
        if (index >= 0) {
            return functions.at(index);
        }
        static const FunctionSettings empty;
        return empty;
    }
};

#endif // VOCEKIT_APP_SETTINGS_DATA_H
