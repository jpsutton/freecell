#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QRandomGenerator>
#include <QSettings>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <algorithm>
#include <random>

#include "dialogs.h"
#ifdef FREECELL_UPDATER
#include "updater.h"
#endif
#ifdef FREECELL_TOUCH_UI
#include "touchui.h"
#endif

MainWindow::MainWindow() {
    setWindowTitle(tr("FreeCell"));
    m_board = new Board;

    QSettings s;
    m_messages = s.value("options/messages", true).toBool();
    m_animations = s.value("options/animations", true).toBool();
    m_board->setAnimations(m_animations);
    m_board->setCardStyle(s.value("options/cardStyle", "faithful").toString() == "refined" ? "refined" : "faithful");
    m_stats.load();

    auto act = [&](const QString& text, const QKeySequence& key, auto slot) {
        auto* a = new QAction(text, this);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    QAction* newGame = act(tr("&New Game"), Qt::Key_F2, [this] { newRandomGame(); });
    QAction* select = act(tr("&Select Game..."), Qt::Key_F3, [this] { selectGame(); });
    QAction* restart = act(tr("&Restart Game"), QKeySequence(), [this] {
        m_board->restartGame();
    });
    QAction* stats = act(tr("S&tatistics..."), Qt::Key_F4, [this] { showStatsDialog(this, m_stats); });
    QAction* options = act(tr("&Options..."), Qt::Key_F5, [this] { showOptions(); });
    m_undo = act(tr("&Undo"), QKeySequence(Qt::CTRL | Qt::Key_Z), [this] { m_board->undo(); });
    QAction* hint = act(tr("&Hint"), Qt::Key_F10, [this] { m_board->showHint(); });
    QAction* about = act(tr("&About FreeCell"), QKeySequence(), [this] {
        dialogs::tell(this, tr("About FreeCell"),
                      tr("FreeCell %1\n\nA Windows XP FreeCell clone (Qt).\nSelect Game numbers 1-1000000 use the "
                              "Microsoft FreeCell deal algorithm. New Game deals a random "
                              "shuffle.").arg(QCoreApplication::applicationVersion()));
    });

#ifdef FREECELL_TOUCH_UI
    // An app bar replaces the menu bar; the shell keeps clear of the system
    // bars itself.
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
    m_board->setLetterbox(true);
    m_shell = new TouchShell(m_board);
    setCentralWidget(m_shell);
    m_shell->addBarAction(m_undo, QStringLiteral(":/touch/undo.svg"));
    m_shell->addBarAction(hint, QStringLiteral(":/touch/hint.svg"));
    m_shell->addBarAction(newGame, QStringLiteral(":/touch/new.svg"));
    m_shell->addMenuAction(select);
    m_shell->addMenuAction(restart);
    m_shell->addMenuSeparator();
    m_shell->addMenuAction(stats);
    m_shell->addMenuAction(options);
    m_shell->addMenuSeparator();
    m_shell->addMenuAction(about);
    // Touch menus show no mnemonics, shortcut keys or dialog ellipses.
    for (QAction* a : {newGame, select, restart, stats, options, m_undo, hint, about}) {
        a->setText(a->text().remove('&').remove(QStringLiteral("...")));
        a->setShortcut(QKeySequence());
    }
    // Android can stop a backgrounded app without closing its window, so
    // save the game whenever the app leaves the foreground.
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState st) {
        if (st == Qt::ApplicationSuspended) saveGame();
    });
#else
    setCentralWidget(m_board);
    m_left = new QLabel;
    m_left->setContentsMargins(0, 0, 8, 0);
    menuBar()->setCornerWidget(m_left, Qt::TopRightCorner);

    auto* game = menuBar()->addMenu(tr("&Game"));
    game->addAction(newGame);
    game->addAction(select);
    game->addAction(restart);
    game->addSeparator();
    game->addAction(stats);
    game->addAction(options);
    game->addSeparator();
    game->addAction(m_undo);
    game->addAction(hint);
    game->addSeparator();
    game->addAction(act(tr("E&xit"), QKeySequence(), [this] { close(); }));

    auto* help = menuBar()->addMenu(tr("&Help"));
#ifdef FREECELL_UPDATER
    m_updater = new Updater(this);
    connect(m_updater, &Updater::restartRequested, this, [this](const QString& program, const QStringList& args) {
        m_restartProgram = program;
        m_restartArgs = args;
        close();
    });
    help->addAction(act(tr("Check for &Updates..."), QKeySequence(), [this] { m_updater->checkNow(); }));
    help->addSeparator();
#endif
    help->addAction(about);
#endif

    connect(m_board, &Board::stateChanged, this, &MainWindow::updateStatus);
    connect(m_board, &Board::gameWon, this, &MainWindow::onWon);
    connect(m_board, &Board::gameStuck, this, &MainWindow::onStuck);
    connect(m_board, &Board::noHint, this, [this] {
        if (m_messages) dialogs::tell(this, tr("FreeCell"), tr("There are no hints available."));
    });

#ifndef FREECELL_TOUCH_UI
    resize(1280, 960 + menuBar()->sizeHint().height());
#endif
    if (!loadSavedGame()) startMsGame(1 + QRandomGenerator::global()->bounded(32000));
#ifdef FREECELL_UPDATER
    m_updater->checkAtStartup();
#endif
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

// Runs `proceed` once the game in progress, if any, is resigned.
void MainWindow::confirmResign(std::function<void()> proceed) {
    if (!m_board->gameInProgress()) {
        proceed();
        return;
    }
    auto resign = [this, proceed] {
        m_stats.record(false);
        proceed();
    };
    if (m_messages)
        dialogs::ask(this, tr("FreeCell"), tr("Do you want to resign this game?"),
                     [resign](bool yes) { if (yes) resign(); });
    else
        resign();
}

void MainWindow::newRandomGame() {
    confirmResign([this] {
        quint32 seed = QRandomGenerator::global()->generate();
        std::vector<int> deck(52);
        for (int i = 0; i < 52; ++i) deck[i] = i;
        std::mt19937 rng(seed);
        std::shuffle(deck.begin(), deck.end(), rng);
        m_gameName = tr("Random %1").arg(seed, 8, 16, QLatin1Char('0')).toUpper();
        m_board->startGame(deck);
        updateStatus();
    });
}

void MainWindow::selectGame() {
    auto* dlg = new QInputDialog(this);
    dlg->setWindowTitle(tr("Select Game"));
    dlg->setLabelText(tr("Game number (1 - 1000000):"));
    dlg->setInputMode(QInputDialog::IntInput);
    dlg->setIntRange(1, 1000000);
    dlg->setIntValue(1);
    dialogs::open(this, dlg, [this, dlg](int result) {
        if (result != QDialog::Accepted) return;
        quint32 n = quint32(dlg->intValue());
        confirmResign([this, n] { startMsGame(n); });
    });
}

void MainWindow::startMsGame(quint32 number) {
    m_gameName = tr("#%1").arg(number);
    m_board->startGame(fc::msDeal(number));
    updateStatus();
}

void MainWindow::updateStatus() {
#ifdef FREECELL_TOUCH_UI
    m_shell->setStatus(m_gameName, m_board->game().cardsLeft());
    m_undo->setEnabled(m_board->game().canUndo());
#else
    m_left->setText(tr("Cards Left: %1").arg(m_board->game().cardsLeft()));
#endif
    setWindowTitle(tr("FreeCell Game %1").arg(m_gameName));
}

void MainWindow::onWon() {
    m_stats.record(true);
    if (!m_messages) return;
    dialogs::ask(this, tr("FreeCell"),
                 tr("Congratulations, you win!\n\nWon: %1  Lost: %2\n\nDo you want to play again?")
                     .arg(m_stats.total.wins).arg(m_stats.total.losses),
                 [this](bool yes) { if (yes) newRandomGame(); });
}

void MainWindow::onStuck() {
    if (m_messages) dialogs::tell(this, tr("FreeCell"), tr("Sorry, there are no more moves."));
}

void MainWindow::showOptions() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Options"));
    auto* lay = new QVBoxLayout(dlg);
    auto* anim = new QCheckBox(tr("Play animations"));
    anim->setChecked(m_animations);
    auto* msg = new QCheckBox(tr("Display messages"));
    msg->setChecked(m_messages);
    lay->addWidget(anim);
    lay->addWidget(msg);
    // Radio buttons, not a combo box: its drop-down list is a new window.
    auto* styleBox = new QGroupBox(tr("Card style"));
    auto* styleLay = new QVBoxLayout(styleBox);
    auto* classic = new QRadioButton(tr("Classic (XP colors)"));
    auto* refined = new QRadioButton(tr("Refined"));
    (m_board->cardStyle() == QLatin1String("refined") ? refined : classic)->setChecked(true);
    styleLay->addWidget(classic);
    styleLay->addWidget(refined);
    lay->addWidget(styleBox);
#ifdef FREECELL_UPDATER
    auto* updates = new QCheckBox(tr("Check for &updates at startup"));
    updates->setChecked(Updater::checkAtStartupEnabled());
    lay->addWidget(updates);
#endif
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    dialogs::open(this, dlg, [=](int result) {
        if (result != QDialog::Accepted) return;
        m_animations = anim->isChecked();
        m_messages = msg->isChecked();
        m_board->setAnimations(m_animations);
        m_board->setCardStyle(refined->isChecked() ? QStringLiteral("refined") : QStringLiteral("faithful"));
#ifdef FREECELL_UPDATER
        Updater::setCheckAtStartup(updates->isChecked());
#endif
        QSettings s;
        s.setValue("options/messages", m_messages);
        s.setValue("options/animations", m_animations);
        s.setValue("options/cardStyle", m_board->cardStyle());
    });
}

void MainWindow::closeEvent(QCloseEvent* e) {
    saveGame();
    e->accept();
    if (!m_restartProgram.isEmpty()) QProcess::startDetached(m_restartProgram, m_restartArgs);
}
