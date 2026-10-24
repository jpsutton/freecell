#pragma once
#include <QMainWindow>
#include <QStringList>
#include <functional>
#include <vector>

#include "board.h"
#include "stats.h"

class QAction;
class QLabel;
class TouchShell;
class Updater;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent*) override;

private:
    void confirmResign(std::function<void()> proceed);
    void newRandomGame();
    void selectGame();
    void startMsGame(quint32 number);
    void saveGame();
    bool loadSavedGame();
    void showOptions();
    void updateStatus();
    void onWon();
    void onStuck();

    Board* m_board;
    Stats m_stats;
    QLabel* m_left = nullptr;       // desktop: "Cards Left" in the menu bar
    TouchShell* m_shell = nullptr;  // touch: app bar and board
    QAction* m_undo;
    QString m_gameName;
    Updater* m_updater = nullptr;
    QString m_restartProgram;  // set when an installed update asks to restart
    QStringList m_restartArgs;
    bool m_messages = true;
    bool m_animations = true;
};
