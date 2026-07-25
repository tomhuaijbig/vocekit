#include <QtTest>

#include "../../src/ui/ocr_page_controller.h"

#include <type_traits>

class OcrPageControllerHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromTypedSettingsAccess();
};

void OcrPageControllerHeaderTests::constructsFromTypedSettingsAccess()
{
    QVERIFY((std::is_constructible<
        OcrPageController,
        const OcrPageAccess &,
        QWidget *,
        QObject *
    >::value));
}

QTEST_MAIN(OcrPageControllerHeaderTests)

#include "ocr_page_controller_header_tests.moc"
