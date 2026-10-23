#pragma once
#include <QMainWindow>
#include <vector>

#include "board.h"
#include "stats.h"

class QAction;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent*) override;

private:
    bool confirmResign();
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
    QLabel* m_left;
    QString m_gameName;
    bool m_messages = true;
    bool m_animations = true;
};
