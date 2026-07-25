#ifndef VOCEKIT_BASIC_SETTINGS_SECTION_H
#define VOCEKIT_BASIC_SETTINGS_SECTION_H

#include <QCheckBox>
#include <QComboBox>
#include <QString>
#include <QWidget>

#include <functional>

class QVBoxLayout;

struct BasicSettingsSnapshot
{
    bool trayResident = true;
    bool autoStartEnabled = false;
    bool strongSelectionEnabled = false;
    bool vocabularyEnabled = true;
    QString vocabularyAddMode;
    bool vocabularyOnlyForVoiceInput = false;
    int vocabularyPromptEntryLimit = 16;
    bool floatingBarEnabled = true;
    bool preRecordCountdownEnabled = false;
    bool recordingBeepEnabled = false;
    bool dictatePolishEnabled = false;
    bool useSystemProxy = false;
};

// 设置页中的基础分区：常用设置、词库、语音录音和网络。
class BasicSettingsSection : public QWidget
{
public:
    enum Kind
    {
        General,
        Vocabulary,
        Voice,
        Network
    };

    struct Callbacks
    {
        std::function<BasicSettingsSnapshot()> snapshotProvider;
        std::function<void(const BasicSettingsSnapshot &)> applySnapshot;
        std::function<void()> saveAndRefresh;
        std::function<void(const QString &, const QString &)> showDetail;
    };

    explicit BasicSettingsSection(
        Kind kind,
        const Callbacks &callbacks,
        QWidget *parent = nullptr
    );

    void refreshFromSettings();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildRows();
    void addGeneralRows(QVBoxLayout *layout);
    void addVocabularyRows(QVBoxLayout *layout);
    void addVoiceRows(QVBoxLayout *layout);
    void addNetworkRows(QVBoxLayout *layout);

    QWidget *toggleRow(
        const QString &title,
        const QString &hint,
        bool checked,
        const std::function<void(bool)> &onChanged
    );
    QWidget *numberRow(
        const QString &title,
        const QString &hint,
        int value,
        int minimum,
        int maximum,
        const QString &suffix,
        const std::function<void(int)> &onChanged
    );
    QWidget *comboRow(
        const QString &title,
        const QString &hint,
        const QVector<QPair<QString, QString>> &items,
        const QString &currentValue,
        const std::function<void(const QString &)> &onChanged
    );
    QWidget *autoStartRow();
    QWidget *strongSelectionRow();

    void attachSettingDetail(QWidget *card, const QString &title, const QString &detail);
    void installSettingDetailTarget(QWidget *target, const QString &title, const QString &detail);
    void saveAndRefresh();
    BasicSettingsSnapshot snapshot() const;
    void applyAndRefresh(const BasicSettingsSnapshot &snapshot);

    QString settingDetailText(const QString &title, const QString &hint) const;
    static QString autoStartDetailText();
    static QString strongSelectionDetailText();

    Kind m_kind = General;
    Callbacks m_callbacks;
    QCheckBox *m_autoStartBox = nullptr;
    QCheckBox *m_strongSelectionBox = nullptr;
};

#endif // VOCEKIT_BASIC_SETTINGS_SECTION_H
