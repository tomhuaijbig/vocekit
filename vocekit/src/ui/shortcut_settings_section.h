#ifndef VOCEKIT_SHORTCUT_SETTINGS_SECTION_H
#define VOCEKIT_SHORTCUT_SETTINGS_SECTION_H

#include <QLabel>
#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class QVBoxLayout;

struct ShortcutSettingsItem
{
    QString id;
    QString title;
    QString hint;
    QString shortcut;
    QString defaultShortcut;
    bool custom = false;
    bool screenshotShortcutEnabled = false;
    QString screenshotShortcut;
    QString defaultScreenshotShortcut;
};

struct ShortcutSettingsSnapshot
{
    QVector<ShortcutSettingsItem> builtInItems;
    QVector<ShortcutSettingsItem> customItems;
};

// 设置页里的快捷键分区，集中处理内置功能、自定义功能和截图快捷键。
class ShortcutSettingsSection : public QWidget
{
public:
    struct Callbacks
    {
        std::function<ShortcutSettingsSnapshot()> snapshotProvider;
        std::function<bool(const QString &, const QString &, QString *)> conflictsWithOther;
        std::function<void(const QString &, const QString &)> setFunctionShortcut;
        std::function<void(const QString &, const QString &)> setScreenshotShortcut;
        std::function<void()> saveAndRefresh;
        std::function<void(const QString &, const QString &)> showDetail;
    };

    explicit ShortcutSettingsSection(
        const Callbacks &callbacks,
        QWidget *parent = nullptr
    );

    void refreshFromSettings();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLabel *shortcutSectionLabel(const QString &title);
    QWidget *hotkeyRow(const ShortcutSettingsItem &item);
    QWidget *customHotkeyRow(const ShortcutSettingsItem &item);
    QWidget *screenshotHotkeyRow(const ShortcutSettingsItem &item);

    ShortcutSettingsSnapshot snapshot() const;
    bool shortcutItemById(const QString &id, ShortcutSettingsItem *out) const;
    QString suggestedShortcutForCustom(const QString &id) const;

    void editHotkey(const ShortcutSettingsItem &item);
    void editCustomHotkey(const ShortcutSettingsItem &item);
    void editScreenshotHotkey(const ShortcutSettingsItem &item);

    void attachSettingDetail(QWidget *card, const QString &title, const QString &detail);
    void installSettingDetailTarget(QWidget *target, const QString &title, const QString &detail);
    void saveAndRefresh();

    QString hotkeyDetailText(const ShortcutSettingsItem &item) const;
    static QString customHotkeyDetailText(const ShortcutSettingsItem &item);
    static QString screenshotHotkeyDetailText();

    Callbacks m_callbacks;
    QVBoxLayout *m_rowsLayout = nullptr;
    QMap<QString, QLabel *> m_hotkeyLabels;
};

#endif // VOCEKIT_SHORTCUT_SETTINGS_SECTION_H
