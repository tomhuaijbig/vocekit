#ifndef VOCEKIT_FAQ_PANEL_H
#define VOCEKIT_FAQ_PANEL_H

#include <QWidget>
#include <QString>
#include <QStringList>

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QVBoxLayout;

class FaqPanel : public QWidget
{
public:
    explicit FaqPanel(
        const std::function<void(const QString &)> &openDiagnostic = std::function<void(const QString &)>(),
        QWidget *parent = nullptr
    );

    void showFaqId(const QString &faqId);
    int matchCount(const QString &keyword) const;

private:
    QString faqCategoryForText(const QString &text) const;
    QString faqDiagnosticKeyword(const QString &category, const QString &title) const;
    void openDiagnosticForFaq(const QString &category, const QString &title);
    void applyFaqSearch();
    void addLatestFeatureFaqItems(QVBoxLayout *items);
    void addRecentWorkflowFaqItems(QVBoxLayout *items);
    QWidget *faqCard(const QString &title, const QString &cause, const QStringList &solutions);

    std::function<void(const QString &)> m_openDiagnostic;
    QLineEdit *m_faqSearchEdit = nullptr;
    QComboBox *m_faqCategoryBox = nullptr;
    QVBoxLayout *m_faqItemsLayout = nullptr;
    QLabel *m_faqEmptyLabel = nullptr;
};

#endif // VOCEKIT_FAQ_PANEL_H
