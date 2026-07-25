#ifndef VOCEKIT_APP_PATHS_H
#define VOCEKIT_APP_PATHS_H

#include <QString>

// 应用级路径规则集中在这里，避免设置、历史、词库和 OCR 各自复制判断 debug/release 目录的逻辑。
QString appBasePath();
QString defaultRecordDirectory();

#endif // VOCEKIT_APP_PATHS_H
