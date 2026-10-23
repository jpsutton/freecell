#include "stats.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

void StatBlock::record(bool win) {
    if (win) {
        ++wins;
        streak = streak > 0 ? streak + 1 : 1;
        if (streak > maxWins) maxWins = streak;
    } else {
        ++losses;
        streak = streak < 0 ? streak - 1 : -1;
        if (-streak > maxLosses) maxLosses = -streak;
    }
}

int StatBlock::percent() const {
    int n = wins + losses;
    return n ? (wins * 100) / n : 0;
}

void Stats::load() {
    QSettings s;
    total.wins = s.value("stats/wins", 0).toInt();
    total.losses = s.value("stats/losses", 0).toInt();
    total.streak = s.value("stats/streak", 0).toInt();
    total.maxWins = s.value("stats/maxWins", 0).toInt();
    total.maxLosses = s.value("stats/maxLosses", 0).toInt();
}

void Stats::save() const {
    QSettings s;
    s.setValue("stats/wins", total.wins);
    s.setValue("stats/losses", total.losses);
    s.setValue("stats/streak", total.streak);
    s.setValue("stats/maxWins", total.maxWins);
    s.setValue("stats/maxLosses", total.maxLosses);
}

void Stats::record(bool win) {
    session.record(win);
    total.record(win);
    save();
}

void Stats::clearTotal() {
    total = StatBlock();
    save();
}

static QString streakText(const StatBlock& b) {
    if (b.streak > 0) return QObject::tr("%1 wins").arg(b.streak);
    if (b.streak < 0) return QObject::tr("%1 losses").arg(-b.streak);
    return QStringLiteral("0");
}

static QWidget* blockWidget(const QString& title, const StatBlock& b) {
    auto* box = new QGroupBox(title);
    auto* g = new QGridLayout(box);
    auto row = [&](int r, const QString& l, const QString& v) {
        g->addWidget(new QLabel(l), r, 0);
        auto* val = new QLabel(v);
        val->setAlignment(Qt::AlignRight);
        g->addWidget(val, r, 1);
    };
    row(0, QObject::tr("Won:"), QString::number(b.wins));
    row(1, QObject::tr("Lost:"), QString::number(b.losses));
    row(2, QObject::tr("Percentage:"), QString::number(b.percent()) + "%");
    row(3, QObject::tr("Current streak:"), streakText(b));
    row(4, QObject::tr("Longest win streak:"), QString::number(b.maxWins));
    row(5, QObject::tr("Longest loss streak:"), QString::number(b.maxLosses));
    return box;
}

void showStatsDialog(QWidget* parent, Stats& stats) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("FreeCell Statistics"));
    auto* lay = new QVBoxLayout(&dlg);
    auto* row = new QHBoxLayout;
    row->addWidget(blockWidget(QObject::tr("This session"), stats.session));
    row->addWidget(blockWidget(QObject::tr("Total"), stats.total));
    lay->addLayout(row);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok);
    auto* clear = bb->addButton(QObject::tr("Clear"), QDialogButtonBox::ResetRole);
    lay->addWidget(bb);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(clear, &QPushButton::clicked, &dlg, [&] {
        if (QMessageBox::question(&dlg, QObject::tr("FreeCell"),
                                  QObject::tr("Are you sure you want to clear all statistics?")) ==
            QMessageBox::Yes) {
            stats.clearTotal();
            stats.session = StatBlock();
            dlg.accept();
        }
    });
    dlg.exec();
}
