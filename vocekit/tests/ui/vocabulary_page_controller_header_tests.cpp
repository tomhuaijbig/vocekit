#include <QtTest>

#include "../../src/ui/vocabulary_page_controller.h"

#include <type_traits>

class VocabularyPageControllerHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromTypedAccessOnly();
};

void VocabularyPageControllerHeaderTests::constructsFromTypedAccessOnly()
{
    QVERIFY((std::is_constructible<
        VocabularyPageController,
        QWidget *,
        const VocabularyPageAccess &
    >::value));
}

QTEST_MAIN(VocabularyPageControllerHeaderTests)

#include "vocabulary_page_controller_header_tests.moc"
