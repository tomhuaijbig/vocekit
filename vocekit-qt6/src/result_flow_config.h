#ifndef VOCEKIT_RESULT_FLOW_CONFIG_H
#define VOCEKIT_RESULT_FLOW_CONFIG_H

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

struct FunctionNetworkPolicies
{
    QString speech = QStringLiteral("inherit");
    QString ocr = QStringLiteral("inherit");
    QString model = QStringLiteral("inherit");
};

QStringList defaultResultActionIds();
QStringList normalizeResultActionIds(const QStringList &ids);
QString normalizeNetworkPolicy(const QString &policy);
QString resolveNetworkPolicy(const QString &policy, bool globalUseSystemProxy);
QJsonObject functionNetworkPoliciesToJson(const FunctionNetworkPolicies &policies);
FunctionNetworkPolicies functionNetworkPoliciesFromJson(const QJsonObject &object);

bool shouldFallbackFromStreamFailure(
    const QString &error,
    int httpStatus,
    bool cancelled
);

struct ResultRecoveryState
{
    bool valid = false;
    QString modeId;
    QString functionTitle;
    QString selectedText;
    QString voiceText;
    QString textInput;
    QString ocrText;
    QString generatedText;
    QString editedText;
    QString model;
    QString promptId;
    QString stage;
    FunctionNetworkPolicies networkPolicies;
    QDateTime createdAt;
    QDateTime updatedAt;
};

QJsonObject resultRecoveryStateToJson(const ResultRecoveryState &state);
ResultRecoveryState resultRecoveryStateFromJson(const QJsonObject &object);
bool saveResultRecoveryState(
    const QString &path,
    const ResultRecoveryState &state,
    QString *error = nullptr
);
bool loadResultRecoveryState(
    const QString &path,
    ResultRecoveryState *state,
    QString *error = nullptr
);
bool clearResultRecoveryState(
    const QString &path,
    QString *error = nullptr
);

#endif // VOCEKIT_RESULT_FLOW_CONFIG_H
