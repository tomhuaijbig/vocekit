#include <QtTest>

#include "../../src/controllers/voice_controller.h"

#include <type_traits>

class VoiceControllerHeaderTests : public QObject
{
    Q_OBJECT

private slots:
    void constructsFromTypedAccessOnly();
};

void VoiceControllerHeaderTests::constructsFromTypedAccessOnly()
{
    QVERIFY((std::is_constructible<
        VoiceController,
        const VoiceControllerAccess &,
        FloatingBar *,
        VoiceControllerHost *,
        QObject *
    >::value));
}

QTEST_MAIN(VoiceControllerHeaderTests)

#include "voice_controller_header_tests.moc"
