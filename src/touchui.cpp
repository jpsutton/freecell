#include "touchui.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QEvent>
#include <QFrame>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
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
        ""));

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

    m_more = barButton(m_bar);
    m_more->setIcon(barIcon(QStringLiteral(":/touch/more.svg")));
    connect(m_more, &QToolButton::clicked, this, &TouchShell::toggleMenu);

    m_menuLayer = new QWidget(this);
    m_menuLayer->hide();
    m_menuLayer->installEventFilter(this);
    m_menuPanel = new QFrame(m_menuLayer);
    m_menuPanel->setObjectName(QStringLiteral("touchMenu"));
    m_menuPanel->setAttribute(Qt::WA_StyledBackground);
    m_menuPanel->setAutoFillBackground(true);
    m_menuPanel->setStyleSheet(QStringLiteral(
        "#touchMenu { background: palette(window); border: 1px solid palette(mid); }"
        "#touchMenu QPushButton { text-align: left; border: none; border-radius: 0;"
        " min-height: 48px; padding: 0 40px 0 20px; background: transparent; }"
        "#touchMenu QPushButton:pressed { background: palette(highlight); color: palette(highlighted-text); }"
        "#touchMenu QFrame[separator=true] { background: palette(mid); max-height: 1px; min-height: 1px; }"));
    m_menuLay = new QVBoxLayout(m_menuPanel);
    m_menuLay->setContentsMargins(0, 8, 0, 8);
    m_menuLay->setSpacing(0);
}

void TouchShell::addMenuAction(QAction* a) {
    auto* b = new QPushButton(a->text(), m_menuPanel);
    b->setFocusPolicy(Qt::NoFocus);
    // Run the action once the menu has closed.
    connect(b, &QPushButton::clicked, this, [this, a] {
        closeMenu();
        QTimer::singleShot(0, a, [a] { a->trigger(); });
    });
    m_menuLay->addWidget(b);
}

void TouchShell::addMenuSeparator() {
    auto* line = new QFrame(m_menuPanel);
    line->setProperty("separator", true);
    m_menuLay->addSpacing(6);
    m_menuLay->addWidget(line);
    m_menuLay->addSpacing(6);
}

void TouchShell::toggleMenu() {
    if (m_menuLayer->isVisible()) closeMenu();
    else openMenu();
}

void TouchShell::closeMenu() { m_menuLayer->hide(); }

void TouchShell::openMenu() {
    m_menuLayer->setGeometry(rect());
    m_menuLay->activate();
    m_menuPanel->adjustSize();
    QMargins safe;
    if (QWindow* w = window()->windowHandle()) safe = w->safeAreaMargins();
    QSize s = m_menuPanel->size().boundedTo(rect().marginsRemoved(safe).size() - QSize(16, 16));
    // Below the app bar in portrait, left of the rail in landscape.
    int x = (m_rail ? m_bar->x() : width() - safe.right()) - s.width() - 8;
    int y = m_rail ? safe.top() + 8 : m_bar->geometry().bottom() + 4;
    m_menuPanel->setGeometry(x, y, s.width(), s.height());
    m_menuLayer->show();
    m_menuLayer->raise();
}

bool TouchShell::eventFilter(QObject* o, QEvent* e) {
    // A tap outside the panel closes it.
    if (o == m_menuLayer && e->type() == QEvent::MouseButtonPress) {
        closeMenu();
        return true;
    }
    return QWidget::eventFilter(o, e);
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

void TouchShell::resizeEvent(QResizeEvent*) {
    closeMenu();
    relayout();
}

void TouchShell::showEvent(QShowEvent*) {
    if (!m_watchingSafeArea) {
        if (QWindow* w = window()->windowHandle()) {
            connect(w, &QWindow::safeAreaMarginsChanged, this, &TouchShell::relayout);
            m_watchingSafeArea = true;
        }
    }
    relayout();
}

namespace {
// A tooltip or a text context menu is a new window, which can abort the app
// on Android; a long press would otherwise raise one.
class NoPopups : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject*, QEvent* e) override {
        return e->type() == QEvent::ToolTip || e->type() == QEvent::ContextMenu;
    }
};
}  // namespace

void applyTouchStyle(QApplication& app) {
    app.installEventFilter(new NoPopups(&app));
    // Dark dialogs and menu, and light system bar icons over the dark app bar.
    app.styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    app.setStyleSheet(QStringLiteral(
        "QPushButton { min-height: 40px; min-width: 72px; padding: 0 12px; }"
        "QComboBox, QSpinBox { min-height: 48px; }"
        // Check boxes and radio buttons: a 32px glyph in a 56px row, easy to hit.
        "QCheckBox, QRadioButton { spacing: 16px; min-height: 56px; }"
        "QCheckBox::indicator, QRadioButton::indicator { width: 32px; height: 32px; }"
        "QCheckBox::indicator:unchecked { image: url(:/touch/check-off.svg); }"
        "QCheckBox::indicator:checked { image: url(:/touch/check-on.svg); }"
        "QRadioButton::indicator:unchecked { image: url(:/touch/radio-off.svg); }"
        "QRadioButton::indicator:checked { image: url(:/touch/radio-on.svg); }"
        "QGroupBox { margin-top: 16px; }"
        "QDialog { font-size: 17px; }"
        "QDialog QPushButton { min-height: 48px; }"));
}
