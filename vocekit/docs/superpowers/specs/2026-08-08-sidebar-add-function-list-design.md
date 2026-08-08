# Sidebar Add-Function Placement Design

## Goal

Move the left-sidebar `新增功能` action from the bottom-fixed area into the function list so it appears immediately after the final function entry and scrolls with that list.

## Chosen behavior

- The button is the final actionable row in the function scroll area.
- It appears after all built-in and custom function rows. When custom functions exist, it is directly below the last custom function.
- It scrolls together with the function rows in short windows and remains reachable through the existing vertical scrollbar.
- It keeps the current text, dashed-border appearance, pointer cursor, and add-function callback.
- Refreshing the function list recreates one add button in the correct final position; it must not duplicate the button.

## Implementation boundary

`CommandCenterShell` remains responsible for rendering the navigation controls. The sidebar constructor creates the scroll area and function layout, while `refreshFunctions()` populates that layout in this order:

1. current function rows;
2. the `新增功能` action;
3. the trailing stretch item.

The separate bottom-fixed add button is removed from `sidebar()`. Function data providers and function-creation controllers are unchanged; the existing `CommandCenterShellAccess::addFunction` callback remains the only creation entry point used by this button.

## Verification

An actual Qt Widgets test will construct `CommandCenterShell` and verify:

- the add button belongs to the function scroll-area content rather than the outer sidebar layout;
- its layout index is after the final function row and before the stretch;
- clicking it invokes the existing callback exactly once;
- calling `refreshFunctions()` does not create duplicates and preserves the ordering;
- compact-height rendering keeps the button reachable through the function list's scrollbar.

The focused shell test and the full existing test suite must pass. A release build or visual widget render will confirm that the button is no longer pinned to the lower-left corner.

## Out of scope

- Changing the button label or visual style.
- Changing custom-function creation, persistence, or navigation behavior.
- Reordering built-in or custom functions.
- Changing the toolbar or main content layout.
