#include <QtTest>

#include "../../src/domain/prompt_runtime_library.h"

class PromptRuntimeLibraryTests : public QObject
{
    Q_OBJECT

private slots:
    void resolvesSelectedLibraryPromptForFunction()
    {
        PromptRuntimeSnapshot snapshot;
        FunctionSettings function;
        function.id = QStringLiteral("custom-1");
        function.name = QStringLiteral("Custom");
        function.promptId = QStringLiteral("prompt-1");
        function.prompt = QStringLiteral("own prompt");
        snapshot.settings.functions.append(function);

        PromptLibraryItem library;
        library.id = QStringLiteral("prompt-1");
        library.name = QStringLiteral("Library prompt");
        library.content = QStringLiteral("library content");
        snapshot.libraryItems.append(library);

        QCOMPARE(
            promptRuntimeForFunction(
                snapshot,
                QStringLiteral("custom-1"),
                QStringLiteral("fallback")
            ),
            QStringLiteral("library content")
        );
    }

    void resolvesFunctionOwnedPrompt()
    {
        PromptRuntimeSnapshot snapshot;
        FunctionSettings function;
        function.id = QStringLiteral("custom-1");
        function.name = QStringLiteral("Custom");
        function.promptId = QStringLiteral("custom-1");
        function.prompt = QStringLiteral("own prompt");
        snapshot.settings.functions.append(function);

        QCOMPARE(
            promptRuntimeForFunction(
                snapshot,
                QStringLiteral("custom-1"),
                QStringLiteral("fallback")
            ),
            QStringLiteral("own prompt")
        );
    }
};

QTEST_MAIN(PromptRuntimeLibraryTests)
#include "prompt_runtime_library_tests.moc"
