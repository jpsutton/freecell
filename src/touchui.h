#pragma once
#include <QWidget>
#include <vector>

class QAction;
class QApplication;
class QEvent;
class QLabel;
class QFrame;
class QVBoxLayout;
class QToolButton;

// Touch-screen layout (Android) in place of the menu bar: an app bar with the
// common actions as icons and the rest in an overflow menu. The bar sits on
// top in portrait and becomes a rail on the right in landscape, where height
// is scarce. The window draws under the system bars; the shell keeps its
// controls and the board inside the safe area.
class TouchShell : public QWidget {
    Q_OBJECT
public:
    explicit TouchShell(QWidget* board, QWidget* parent = nullptr);

    void addBarAction(QAction* a, const QString& icon);
    // The overflow menu is a panel inside the window: a QMenu is a popup
    // window, and a second window can abort the app on Android.
    void addMenuAction(QAction* a);
    void addMenuSeparator();
    void setStatus(const QString& game, int cardsLeft);

protected:
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    void relayout();
    void rebuildBar();
    void updateStatusText();
    void toggleMenu();
    void closeMenu();
    void openMenu();

    QWidget* m_board;
    QWidget* m_bar;
    QLabel* m_title;
    QLabel* m_status;
    QToolButton* m_more;
    QWidget* m_menuLayer;  // transparent, covers the shell, closes the menu when tapped
    QFrame* m_menuPanel;
    QVBoxLayout* m_menuLay;
    std::vector<QToolButton*> m_buttons;
    bool m_rail = false;
    bool m_watchingSafeArea = false;
    QString m_game;
    int m_cardsLeft = 52;
};

// Touch-sized menus and dialog controls.
void applyTouchStyle(QApplication& app);
