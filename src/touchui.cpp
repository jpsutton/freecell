#include "touchui.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QStyleHints>
#include <QSvgRenderer>
#include <QToolButton>
#include <QWindow>
#include <cmath>

namespace {
const QColor kFelt(0, 127, 0);
const QColor kBarColor(0, 75, 0);
constexpr int kBarHeight = 56;  // Material top app bar
constexpr int kRailWidth = 72;  // Material navigation rail
constexpr int kButton = 48;     // minimum touch target
constexpr int kIcon = 24;

// White glyph, plus a dimmed copy for the disabled state.
QIcon barIcon(const QString& path) {
    QSvgRenderer svg(path);
    qreal dpr = qApp->devicePixelRatio();
    int side = int(std::ceil(kIcon * dpr));
    QIcon icon;
    for (qreal opacity : {1.0, 0.38}) {
        QPixmap pm(side, side);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setOpacity(opacity);
            svg.render(&p, QRectF(pm.rect()));
        }
        pm.setDevicePixelRatio(dpr);
        icon.addPixmap(pm, opacity < 1 ? QIcon::Disabled : QIcon::Normal);
    }
    return icon;
}

QToolButton* barButton(QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setFixedSize(kButton, kButton);
    b->setIconSize(QSize(kIcon, kIcon));
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
}  // namespace

TouchShell::TouchShell(QWidget* board, QWidget* parent) : QWidget(parent), m_board(board) {
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, kFelt);
    setPalette(pal);
    board->setParent(this);

    m_bar = new QWidget(this);
    m_bar->setAutoFillBackground(true);
    QPalette barPal = m_bar->palette();
    barPal.setColor(QPalette::Window, kBarColor);
    m_bar->setPalette(barPal);
    m_bar->setStyleSheet(QStringLiteral(
        "QLabel { color: white; }"
        "QToolButton { border: none; border-radius: 24px; background: transparent; }"
        "QToolButton:pressed { background: rgba(255, 255, 255, 60); }"
        "QToolButton::menu-indicator { image: none; width: 0; }"));

    m_title = new QLabel(tr("FreeCell"), m_bar);
    QFont tf = m_title->font();
    tf.setPixelSize(20);
    tf.setWeight(QFont::Medium);
    m_title->setFont(tf);
    m_status = new QLabel(m_bar);
    QFont sf = m_status->font();
    sf.setPixelSize(13);
    m_status->setFont(sf);
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    m_menu = new QMenu(this);
    m_more = barButton(m_bar);
    m_more->setIcon(barIcon(QStringLiteral(":/touch/more.svg")));
    m_more->setMenu(m_menu);
    m_more->setPopupMode(QToolButton::InstantPopup);
}

void TouchShell::addBarAction(QAction* a, const QString& icon) {
    a->setIcon(barIcon(icon));
    auto* b = barButton(m_bar);
    b->setDefaultAction(a);
    m_buttons.push_back(b);
    if (m_bar->layout()) rebuildBar();
}

void TouchShell::setStatus(const QString& game, int cardsLeft) {
    m_game = game;
    m_cardsLeft = cardsLeft;
    updateStatusText();
}

void TouchShell::updateStatusText() {
    if (m_rail)
        m_status->setText(tr("%1\nleft").arg(m_cardsLeft));
    else
        m_status->setText(tr("Game %1 · %2 cards left").arg(m_game).arg(m_cardsLeft));
}

void TouchShell::rebuildBar() {
    delete m_bar->layout();
    auto* lay = new QBoxLayout(m_rail ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight, m_bar);
    lay->setSpacing(m_rail ? 8 : 0);
    if (m_rail) {
        m_title->hide();
        lay->addWidget(m_more, 0, Qt::AlignHCenter);
        lay->addSpacing(8);
        for (auto* b : m_buttons) lay->addWidget(b, 0, Qt::AlignHCenter);
        lay->addStretch(1);
        lay->addWidget(m_status, 0, Qt::AlignHCenter);
        m_status->setAlignment(Qt::AlignHCenter);
    } else {
        m_title->show();
        auto* text = new QVBoxLayout;
        text->setSpacing(0);
        text->addStretch(1);
        text->addWidget(m_title);
        text->addWidget(m_status);
        text->addStretch(1);
        lay->addLayout(text, 1);
        for (auto* b : m_buttons) lay->addWidget(b);
        lay->addWidget(m_more);
        m_status->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    updateStatusText();
}

// The bar's background runs under the system bars; its controls, and the
// board, stay inside the window's safe area. The board letterboxes itself
// within whatever is left.
void TouchShell::relayout() {
    QMargins safe;
    if (QWindow* w = window()->windowHandle()) safe = w->safeAreaMargins();
    bool rail = width() > height();
    if (rail != m_rail || !m_bar->layout()) {
        m_rail = rail;
        rebuildBar();
    }
    QRect area = rect().marginsRemoved(safe);
    if (m_rail) {
        int w = kRailWidth + safe.right();
        m_bar->setGeometry(width() - w, 0, w, height());
        m_bar->layout()->setContentsMargins(0, safe.top() + 8, safe.right(), safe.bottom() + 8);
        area.setRight(width() - w - 1);
    } else {
        int h = kBarHeight + safe.top();
        m_bar->setGeometry(0, 0, width(), h);
        m_bar->layout()->setContentsMargins(safe.left() + 16, safe.top(), safe.right() + 4, 0);
        area.setTop(h);
    }
    m_board->setGeometry(area);
}

void TouchShell::resizeEvent(QResizeEvent*) { relayout(); }

void TouchShell::showEvent(QShowEvent*) {
    if (!m_watchingSafeArea) {
        if (QWindow* w = window()->windowHandle()) {
            connect(w, &QWindow::safeAreaMarginsChanged, this, &TouchShell::relayout);
            m_watchingSafeArea = true;
        }
    }
    relayout();
}

void applyTouchStyle(QApplication& app) {
    // Dark dialogs and menus, and light system bar icons over the dark app bar.
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    app.setStyleSheet(QStringLiteral(
        "QMenu { padding: 8px 0; }"
        "QMenu::item { padding: 12px 40px 12px 20px; }"
        "QMenu::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
        "QMenu::item:disabled { color: palette(mid); }"
        "QMenu::separator { height: 1px; margin: 6px 0; background: palette(mid); }"
        "QPushButton { min-height: 40px; min-width: 72px; padding: 0 12px; }"
        "QComboBox, QSpinBox, QCheckBox { min-height: 40px; }"));
}
