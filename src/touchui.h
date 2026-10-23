#pragma once
#include <QWidget>
#include <vector>

class QAction;
class QApplication;
class QLabel;
class QMenu;
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
    QMenu* overflowMenu() const { return m_menu; }
    void setStatus(const QString& game, int cardsLeft);

protected:
    void resizeEvent(QResizeEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    void relayout();
    void rebuildBar();
    void updateStatusText();

    QWidget* m_board;
    QWidget* m_bar;
    QLabel* m_title;
    QLabel* m_status;
    QToolButton* m_more;
    QMenu* m_menu;
    std::vector<QToolButton*> m_buttons;
    bool m_rail = false;
    bool m_watchingSafeArea = false;
    QString m_game;
    int m_cardsLeft = 52;
};

// Touch-sized menus and dialog controls.
void applyTouchStyle(QApplication& app);
