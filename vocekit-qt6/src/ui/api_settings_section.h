#ifndef VOCEKIT_API_SETTINGS_SECTION_H
#define VOCEKIT_API_SETTINGS_SECTION_H

#include "../config/secret_config.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QVector>
#include <QWidget>

#include <functional>

class QPushButton;
class QCheckBox;
class QDoubleSpinBox;
class WindowsSpeechSettingsCard;

struct ApiSettingsSnapshot
{
    QString speechProvider;
    QString ocrEngine;
    QString windowsSpeechLanguage;
    bool useSystemProxy = false;
};

// 设置页中的接口配置分区，集中管理语音识别、OCR 和大模型接口密钥。
class ApiSettingsSection : public QWidget
{
public:
    struct Callbacks
    {
        std::function<ApiSettingsSnapshot()> snapshotProvider;
        std::function<bool(const QString &, const QString &, const QString &)> saveRuntimeSettings;
        std::function<void()> onChanged;
        std::function<void(const QString &, const QString &)> showDetail;
    };

    explicit ApiSettingsSection(
        const Callbacks &callbacks,
        QWidget *parent = nullptr
    );

    bool saveSecretsFromUi(bool showConfirmation = false);
    void refreshFromSettings();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void setComboCurrentData(QComboBox *box, const QString &value);
    void attachSettingDetail(QWidget *card, const QString &title, const QString &detail);
    void installSettingDetailTarget(QWidget *target, const QString &title, const QString &detail);
    QString apiRowDetailText(const QString &title, const QString &hint) const;
    ApiSettingsSnapshot snapshot() const;

    QLineEdit *newSecretEdit(const QString &value);
    QLineEdit *newPlainEdit(const QString &value, const QString &placeholder);
    QWidget *speechProviderRow();
    QWidget *ocrProviderRow();
    QWidget *baiduSampleCodeImportRow();
    void updateSpeechSecretRows();
    void updateOcrSecretRows();
    void restoreRuntimeSettings(const ApiSettingsSnapshot &settings);
    QWidget *secretSection(const QString &title, const QString &hint, const QVector<QWidget *> &rows);
    QWidget *secretInputRow(const QString &title, const QString &hint, QLineEdit *edit);
    QWidget *plainInputRow(const QString &title, const QString &hint, QLineEdit *edit);

    QString customModelSummaryText() const;
    void refreshCustomModelSummary();
    QWidget *customModelConfigCard();
    QWidget *advancedApiConfigCard();
    QWidget *customModelEditorRow(
        const CustomModelProfile &profile,
        QLineEdit **nameEdit,
        QLineEdit **urlEdit,
        QLineEdit **keyEdit,
        QLineEdit **modelEdit,
        QCheckBox **temperatureEnabled,
        QDoubleSpinBox **temperatureSpin,
        QCheckBox **topPEnabled,
        QDoubleSpinBox **topPSpin,
        QPushButton **deleteButton
    );
    void showCustomModelConfigDialog();

    Callbacks m_callbacks;

    QLineEdit *m_deepseekKeyEdit = nullptr;
    QLineEdit *m_openaiKeyEdit = nullptr;
    QLineEdit *m_openaiBaseUrlEdit = nullptr;
    QLineEdit *m_anthropicKeyEdit = nullptr;
    QLineEdit *m_anthropicBaseUrlEdit = nullptr;
    QLineEdit *m_baiduApiKeyEdit = nullptr;
    QLineEdit *m_baiduSecretKeyEdit = nullptr;
    QLineEdit *m_baiduAppIdEdit = nullptr;
    QWidget *m_baiduSampleCodeImportRow = nullptr;
    WindowsSpeechSettingsCard *m_windowsSpeechSettingsCard = nullptr;
    QLineEdit *m_xfyunAppIdEdit = nullptr;
    QLineEdit *m_xfyunApiKeyEdit = nullptr;
    QLineEdit *m_xfyunApiSecretEdit = nullptr;
    QLineEdit *m_customSpeechUrlEdit = nullptr;
    QLineEdit *m_customSpeechApiKeyEdit = nullptr;
    QLineEdit *m_customSpeechModelEdit = nullptr;
    QLineEdit *m_customOcrUrlEdit = nullptr;
    QLineEdit *m_customOcrApiKeyEdit = nullptr;
    QLineEdit *m_customOcrModelEdit = nullptr;
    QLabel *m_customModelSummaryLabel = nullptr;
    QVector<CustomModelProfile> m_customModelProfiles;
    QComboBox *m_speechProviderBox = nullptr;
    QComboBox *m_ocrProviderBox = nullptr;
    QWidget *m_baiduApiKeyRow = nullptr;
    QWidget *m_baiduSecretKeyRow = nullptr;
    QWidget *m_baiduAppIdRow = nullptr;
    QWidget *m_xfyunAppIdRow = nullptr;
    QWidget *m_xfyunApiKeyRow = nullptr;
    QWidget *m_xfyunApiSecretRow = nullptr;
    QWidget *m_customSpeechUrlRow = nullptr;
    QWidget *m_customSpeechApiKeyRow = nullptr;
    QWidget *m_customSpeechModelRow = nullptr;
    QWidget *m_customOcrUrlRow = nullptr;
    QWidget *m_customOcrApiKeyRow = nullptr;
    QWidget *m_customOcrModelRow = nullptr;
    QWidget *m_openaiBaseUrlRow = nullptr;
    QWidget *m_anthropicBaseUrlRow = nullptr;
};

#endif // VOCEKIT_API_SETTINGS_SECTION_H
