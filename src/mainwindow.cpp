#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QSettings>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <algorithm>
#include <random>

MainWindow::MainWindow() {
    setWindowTitle(tr("FreeCell"));
    m_board = new Board;
    setCentralWidget(m_board);

    QSettings s;
    m_messages = s.value("options/messages", true).toBool();
    m_animations = s.value("options/animations", true).toBool();
    m_board->setAnimations(m_animations);
    m_board->setCardStyle(s.value("options/cardStyle", "faithful").toString() == "refined" ? "refined" : "faithful");
    m_stats.load();

    m_left = new QLabel;
    m_left->setContentsMargins(0, 0, 8, 0);
    menuBar()->setCornerWidget(m_left, Qt::TopRightCorner);

    auto* game = menuBar()->addMenu(tr("&Game"));
    auto add = [&](QMenu* m, const QString& text, const QKeySequence& key, auto slot) {
        QAction* a = m->addAction(text);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    add(game, tr("&New Game"), Qt::Key_F2, [this] { newRandomGame(); });
    add(game, tr("&Select Game..."), Qt::Key_F3, [this] { selectGame(); });
    add(game, tr("&Restart Game"), QKeySequence(), [this] {
        m_board->restartGame();
    });
    game->addSeparator();
    add(game, tr("S&tatistics..."), Qt::Key_F4, [this] { showStatsDialog(this, m_stats); });
    add(game, tr("&Options..."), Qt::Key_F5, [this] { showOptions(); });
    game->addSeparator();
    add(game, tr("&Undo"), QKeySequence(Qt::CTRL | Qt::Key_Z), [this] { m_board->undo(); });
    add(game, tr("&Hint"), Qt::Key_F10, [this] { m_board->showHint(); });
    game->addSeparator();
    add(game, tr("E&xit"), QKeySequence(), [this] { close(); });

    auto* help = menuBar()->addMenu(tr("&Help"));
    add(help, tr("&About FreeCell"), QKeySequence(), [this] {
        QMessageBox::about(this, tr("About FreeCell"),
                           tr("FreeCell clone (Qt).\nSelect Game numbers 1-1000000 use the "
                              "Microsoft FreeCell deal algorithm. New Game deals a random "
                              "shuffle."));
    });

    connect(m_board, &Board::stateChanged, this, &MainWindow::updateStatus);
    connect(m_board, &Board::gameWon, this, &MainWindow::onWon);
    connect(m_board, &Board::gameStuck, this, &MainWindow::onStuck);
    connect(m_board, &Board::noHint, this, [this] {
        if (m_messages) QMessageBox::information(this, tr("FreeCell"), tr("There are no hints available."));
    });

    resize(1280, 960 + menuBar()->sizeHint().height());
    if (!loadSavedGame()) startMsGame(1 + QRandomGenerator::global()->bounded(32000));
}

// A game in progress at exit is saved as its deal plus move history and
// resumed on the next start, instead of being resigned.
void MainWindow::saveGame() {
    QSettings s;
    s.remove("saved");
    if (!m_board->gameInProgress()) return;
    const fc::Game& g = m_board->game();
    QStringList deal, moves;
    for (int c : g.dealOrder()) deal << QString::number(c);
    for (const fc::Move& m : g.history())
        moves << QStringLiteral("%1 %2 %3 %4 %5 %6")
                     .arg(int(m.from)).arg(m.fromIdx).arg(int(m.to)).arg(m.toIdx)
                     .arg(m.count).arg(int(m.chained));
    s.setValue("saved/name", m_gameName);
    s.setValue("saved/deal", deal.join(','));
    s.setValue("saved/moves", moves.join(','));
}

bool MainWindow::loadSavedGame() {
    QSettings s;
    QStringList deal = s.value("saved/deal").toString().split(',', Qt::SkipEmptyParts);
    if (deal.size() != 52) return false;
    std::vector<int> order;
    for (const QString& c : deal) order.push_back(c.toInt());
    std::vector<fc::Move> moves;
    for (const QString& t : s.value("saved/moves").toString().split(',', Qt::SkipEmptyParts)) {
        QStringList f = t.split(' ');
        if (f.size() != 6) return false;
        moves.push_back({fc::Area(f[0].toInt()), f[1].toInt(), fc::Area(f[2].toInt()),
                         f[3].toInt(), f[4].toInt(), f[5].toInt() != 0});
    }
    m_gameName = s.value("saved/name").toString();
    m_board->restoreGame(order, moves);
    s.remove("saved");
    updateStatus();
    return true;
}

bool MainWindow::confirmResign() {
    if (!m_board->gameInProgress()) return true;
    if (m_messages &&
        QMessageBox::question(this, tr("FreeCell"), tr("Do you want to resign this game?")) !=
            QMessageBox::Yes)
        return false;
    m_stats.record(false);
    return true;
}

void MainWindow::newRandomGame() {
    if (!confirmResign()) return;
    quint32 seed = QRandomGenerator::global()->generate();
    std::vector<int> deck(52);
    for (int i = 0; i < 52; ++i) deck[i] = i;
    std::mt19937 rng(seed);
    std::shuffle(deck.begin(), deck.end(), rng);
    m_gameName = tr("Random %1").arg(seed, 8, 16, QLatin1Char('0')).toUpper();
    m_board->startGame(deck);
    updateStatus();
}

void MainWindow::selectGame() {
    bool ok = false;
    int n = QInputDialog::getInt(this, tr("Select Game"),
                                 tr("Game number (1 - 1000000):"), 1, 1, 1000000, 1, &ok);
    if (!ok) return;
    if (!confirmResign()) return;
    startMsGame(quint32(n));
}

void MainWindow::startMsGame(quint32 number) {
    m_gameName = tr("#%1").arg(number);
    m_board->startGame(fc::msDeal(number));
    updateStatus();
}

void MainWindow::updateStatus() {
    m_left->setText(tr("Cards Left: %1").arg(m_board->game().cardsLeft()));
    setWindowTitle(tr("FreeCell Game %1").arg(m_gameName));
}

void MainWindow::onWon() {
    m_stats.record(true);
    if (!m_messages) return;
    auto r = QMessageBox::question(
        this, tr("FreeCell"),
        tr("Congratulations, you win!\n\nWon: %1  Lost: %2\n\nDo you want to play again?")
            .arg(m_stats.total.wins).arg(m_stats.total.losses));
    if (r == QMessageBox::Yes) newRandomGame();
}

void MainWindow::onStuck() {
    if (m_messages)
        QMessageBox::information(this, tr("FreeCell"), tr("Sorry, there are no more moves."));
}

void MainWindow::showOptions() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Options"));
    auto* lay = new QVBoxLayout(&dlg);
    auto* anim = new QCheckBox(tr("Play animations"));
    anim->setChecked(m_animations);
    auto* msg = new QCheckBox(tr("Display messages"));
    msg->setChecked(m_messages);
    lay->addWidget(anim);
    lay->addWidget(msg);
    auto* styleRow = new QHBoxLayout;
    auto* style = new QComboBox;
    style->addItem(tr("Classic (XP colors)"), QStringLiteral("faithful"));
    style->addItem(tr("Refined"), QStringLiteral("refined"));
    style->setCurrentIndex(style->findData(m_board->cardStyle()));
    auto* styleLabel = new QLabel(tr("&Card style:"));
    styleLabel->setBuddy(style);
    styleRow->addWidget(styleLabel);
    styleRow->addWidget(style, 1);
    lay->addLayout(styleRow);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    m_animations = anim->isChecked();
    m_messages = msg->isChecked();
    m_board->setAnimations(m_animations);
    m_board->setCardStyle(style->currentData().toString());
    QSettings s;
    s.setValue("options/messages", m_messages);
    s.setValue("options/animations", m_animations);
    s.setValue("options/cardStyle", m_board->cardStyle());
}

void MainWindow::closeEvent(QCloseEvent* e) {
    saveGame();
    e->accept();
}
