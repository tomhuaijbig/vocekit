# Sidebar Add-Function Placement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the left-sidebar `新增功能` action directly below the final function row and make it scroll with the function list.

**Architecture:** Keep function creation and navigation callbacks unchanged. `CommandCenterShell::refreshFunctions()` will own all rows inside the function scroll area, appending one add action after the current function rows and before the trailing stretch; the outer sidebar will no longer render a separate bottom-fixed add button.

**Tech Stack:** C++11, Qt 5.9 Widgets/Test, qmake, MinGW 5.3, PowerShell.

---

## File map

- `tests/ui/command_center_shell_header_tests.cpp`: extend the existing shell contract test into a real widget/layout regression test.
- `tests/ui/command_center_shell_header_tests.pro`: link the real shell and search-router implementation needed by the widget test.
- `src/ui/command_center_shell.h`: declare the focused add-button construction helper.
- `src/ui/command_center_shell.cpp`: remove the bottom-fixed action and append it to the refreshed function list.

### Task 1: Prove the desired sidebar order with a failing widget test

**Files:**
- Modify: `tests/ui/command_center_shell_header_tests.cpp`
- Modify: `tests/ui/command_center_shell_header_tests.pro`
- Test: `tests/ui/command_center_shell_header_tests.cpp`

- [ ] **Step 1: Link the real shell implementation in the focused test project**

Change the project file to compile the real shell and its only implementation dependency:

```qmake
QT += core gui widgets testlib

CONFIG += c++11 console testcase
CONFIG -= app_bundle

TEMPLATE = app
TARGET = command_center_shell_header_tests

SOURCES += \
    command_center_shell_header_tests.cpp \
    ../../src/ui/command_center_shell.cpp \
    ../../src/ui/command_search_router.cpp

HEADERS += \
    ../../src/ui/command_center_shell.h \
    ../../src/ui/command_search_router.h
```

- [ ] **Step 2: Add the failing real-widget regression test**

Add `addFunctionActionFollowsFunctionRows()` to the test class, switch from `QTEST_APPLESS_MAIN` to `QTEST_MAIN`, include `QCoreApplication`, `QEvent`, `QPushButton`, `QScrollArea`, `QScrollBar`, and `QVBoxLayout`, then add:

```cpp
void CommandCenterShellHeaderTests::addFunctionActionFollowsFunctionRows()
{
    int addCalls = 0;
    QVector<CommandCenterFunctionItem> functions;

    CommandCenterFunctionItem builtIn;
    builtIn.id = QStringLiteral("dictate");
    builtIn.title = QString::fromUtf8("听写");
    functions.append(builtIn);

    CommandCenterFunctionItem custom;
    custom.id = QStringLiteral("custom_1");
    custom.title = QString::fromUtf8("自定义功能 1");
    functions.append(custom);

    CommandCenterShellAccess access;
    access.functionsProvider = [&functions]() { return functions; };
    access.addFunction = [&addCalls]() { ++addCalls; };

    CommandCenterShell shell(access, new QWidget);
    shell.resize(720, 220);
    shell.show();
    QCoreApplication::processEvents();

    auto *scroll = shell.findChild<QScrollArea *>(
        QStringLiteral("commandFunctionScroll")
    );
    auto *add = shell.findChild<QPushButton *>(
        QStringLiteral("commandAddFunctionButton")
    );
    QVERIFY(scroll);
    QVERIFY(add);
    QVERIFY(scroll->widget()->isAncestorOf(add));

    auto *list = qobject_cast<QVBoxLayout *>(scroll->widget()->layout());
    QVERIFY(list);
    QCOMPARE(list->indexOf(add), functions.size());
    QVERIFY(list->itemAt(list->count() - 1)->spacerItem());

    add->click();
    QCOMPARE(addCalls, 1);

    shell.refreshFunctions();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    add = shell.findChild<QPushButton *>(
        QStringLiteral("commandAddFunctionButton")
    );
    QVERIFY(add);
    QCOMPARE(
        shell.findChildren<QPushButton *>(
            QStringLiteral("commandAddFunctionButton")
        ).size(),
        1
    );
    QCOMPARE(list->indexOf(add), functions.size());

    QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
    scroll->verticalScrollBar()->setValue(
        scroll->verticalScrollBar()->maximum()
    );
    QCoreApplication::processEvents();
    QVERIFY(add->isVisibleTo(scroll->viewport()));
}
```

- [ ] **Step 3: Build and run the focused test to verify RED**

Run from `vocekit/tests/ui`:

```powershell
$env:Path='D:\QQQQQT0001\5.9\mingw53_32\bin;D:\QQQQQT0001\Tools\mingw530_32\bin;' + $env:Path
qmake.exe command_center_shell_header_tests.pro -o Makefile.codex.command_center_shell_header_tests -spec win32-g++ CONFIG+=release
mingw32-make.exe -f Makefile.codex.command_center_shell_header_tests -j2
$env:QT_QPA_PLATFORM='offscreen'
.\release\command_center_shell_header_tests.exe -maxwarnings 0
```

Expected: the new test fails because `commandFunctionScroll` and `commandAddFunctionButton` do not yet identify an add action inside the function-list content.

- [ ] **Step 4: Commit the verified failing test**

```powershell
git add vocekit/tests/ui/command_center_shell_header_tests.cpp vocekit/tests/ui/command_center_shell_header_tests.pro
git commit -m "test: require add function inside sidebar list"
```

### Task 2: Move the add action into the refreshed function list

**Files:**
- Modify: `src/ui/command_center_shell.h`
- Modify: `src/ui/command_center_shell.cpp`
- Test: `tests/ui/command_center_shell_header_tests.cpp`

- [ ] **Step 1: Declare a focused button-construction helper**

Add this private declaration beside `functionButton()`:

```cpp
QPushButton *addFunctionButton();
```

- [ ] **Step 2: Identify the function scroll area and remove the bottom-fixed button**

In `CommandCenterShell::sidebar()`, set the scroll area's stable object name immediately after construction:

```cpp
auto *functionScroll = new QScrollArea;
functionScroll->setObjectName(QStringLiteral("commandFunctionScroll"));
```

Delete the existing block that constructs `add`, connects it, and calls `layout->addWidget(add)` after `layout->addWidget(functionScroll, 1)`.

- [ ] **Step 3: Implement the unchanged add action as a reusable row**

Add this method after `functionButton()`:

```cpp
QPushButton *CommandCenterShell::addFunctionButton()
{
    auto *button = new QPushButton(uiText("新增功能"));
    button->setObjectName(QStringLiteral("commandAddFunctionButton"));
    button->setFixedHeight(44);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(commandCenterFunctionButtonStyle(false, true));
    connect(button, &QPushButton::clicked, this, [this]() {
        if (m_access.addFunction) {
            m_access.addFunction();
        }
    });
    return button;
}
```

- [ ] **Step 4: Append one add action before the list stretch**

Change the end of `refreshFunctions()` to:

```cpp
for (const CommandCenterFunctionItem &item : functions) {
    m_functionLayout->addWidget(functionButton(item));
}
m_functionLayout->addWidget(addFunctionButton());
m_functionLayout->addStretch();
refreshButtonStyles();
```

- [ ] **Step 5: Rebuild and run the focused test to verify GREEN**

Run the Task 1 command again.

Expected: `5 passed, 0 failed`; the add action is after the two function rows, survives refresh without duplication, invokes the callback once, and is visible after scrolling to the bottom.

- [ ] **Step 6: Commit the minimal production change**

```powershell
git add vocekit/src/ui/command_center_shell.h vocekit/src/ui/command_center_shell.cpp
git commit -m "fix: place add function below sidebar functions"
```

### Task 3: Verify the integrated layout and release build

**Files:**
- Verify: `src/ui/command_center_shell.cpp`
- Verify: `tests/ui/command_center_shell_header_tests.cpp`
- Verify: `vocekit.pro`

- [ ] **Step 1: Run related navigation tests**

Run the full test script after the focused GREEN:

```powershell
Set-Location C:\Users\13736\Desktop\tts\vocekit
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-all-tests.ps1 -Configuration release
```

Expected: every discovered project passes with zero failed, skipped, or infrastructure failures.

- [ ] **Step 2: Build a fresh Release executable**

```powershell
$env:Path='D:\QQQQQT0001\5.9\mingw53_32\bin;D:\QQQQQT0001\Tools\mingw530_32\bin;' + $env:Path
$buildDir='C:\Users\13736\Desktop\tts\vocekit\build\sidebar-add-function-verify'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Set-Location $buildDir
qmake.exe ..\..\vocekit.pro -spec win32-g++ CONFIG+=release
mingw32-make.exe -j2 release
Get-FileHash .\release\vocekit.exe -Algorithm SHA256
```

Expected: exit code `0`, a new `release\vocekit.exe`, and a recorded SHA-256 hash.

- [ ] **Step 3: Perform the visual placement check**

Open the new Release build, navigate to a custom-function page, and inspect both the current window size and a compact-height window. Confirm:

- `新增功能` is directly below the final custom-function row;
- there is no separate button pinned to the lower-left corner;
- scrolling the function list reaches the add action;
- the button text and dashed-border style remain unchanged.

- [ ] **Step 4: Run final repository checks**

```powershell
Set-Location C:\Users\13736\Desktop\tts
git diff --check
git status --short
```

Expected: no whitespace errors and no unexpected tracked changes beyond the planned implementation.
