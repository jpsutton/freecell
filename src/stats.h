#pragma once
#include <QWidget>

struct StatBlock {
    int wins = 0;
    int losses = 0;
    int streak = 0;     // >0 win streak, <0 loss streak
    int maxWins = 0;
    int maxLosses = 0;

    void record(bool win);
    int percent() const;
};

class Stats {
public:
    void load();
    void save() const;
    void record(bool win);
    void clearTotal();

    StatBlock session;
    StatBlock total;
};

// Modal statistics dialog (like MS FreeCell's Game > Statistics).
void showStatsDialog(QWidget* parent, Stats& stats);
