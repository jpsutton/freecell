#pragma once
#include <QString>
#include <functional>

class QDialog;
class QWidget;

// Non-blocking dialogs. Nothing here runs a nested event loop, and in the
// touch layout nothing creates a native window: Qt on Android aborts the app
// (QTBUG-141579, "Failed to acquire deadlock protector") when it opens a
// second top-level window, so there dialogs are drawn inside the main window.
namespace dialogs {

// Shows `dlg` modally over `host`'s window and takes ownership of it. `done`
// gets the dialog's result after it closes; the dialog is still alive then.
void open(QWidget* host, QDialog* dlg, std::function<void(int)> done = {});

// Yes/No question; `done` gets true for Yes.
void ask(QWidget* host, const QString& title, const QString& text, std::function<void(bool)> done);

// Message with an OK button.
void tell(QWidget* host, const QString& title, const QString& text);

}  // namespace dialogs
