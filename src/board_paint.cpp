#include <QPainter>
#include <QImage>
#include <QSvgRenderer>
#include <QResizeEvent>
#include <algorithm>
#include <cmath>

#include "board.h"
#include "board_consts.h"

using namespace fc;

namespace {
// XP FreeCell colors.
const QColor kFelt(0, 127, 0);
const QColor kBright(0, 255, 0);
const QColor kDark(0, 0, 0);
}  // namespace

double Board::scale() const {
    if (m_letterbox) return std::min(width() / double(kLW), height() / double(kLH));
    return width() / double(kLW);
}

double Board::logicalHeight() const { return m_letterbox ? kLH : height() / scale(); }

QPointF Board::origin() const {
    if (!m_letterbox) return QPointF();
    return QPointF((width() - kLW * scale()) / 2, (height() - kLH * scale()) / 2);
}

void Board::toLogical(QPainter& p) const {
    p.translate(origin());
    p.scale(scale(), scale());
}

double Board::offset() const {
    size_t n = 0;
    for (int i = 0; i < kCascades; ++i) n = std::max(n, m_game.cascade(i).size());
    if (n <= 1) return 18;
    double avail = logicalHeight() - kCY - kCH - 6;
    return std::clamp(avail / double(n - 1), 9.0, 18.0);
}

QPointF Board::cascadePos(int col, int k) const { return QPointF(kGap + col * (kCW + kGap), kCY + k * offset()); }
QPointF Board::cellPos(int i) const { return QPointF(i * kCW, kTopY); }
QPointF Board::foundPos(int i) const { return QPointF(kFoundX + i * kCW, kTopY); }

QPointF Board::cardPos(Area a, int idx, int k) const {
    switch (a) {
    case Area::Cascade: return cascadePos(idx, k);
    case Area::Cell: return cellPos(idx);
    default: return foundPos(idx);
    }
}

QPointF Board::logical(const QPointF& p) const { return (p - origin()) / scale(); }

QSize Board::minimumSizeHint() const { return QSize(480, 360); }
QSize Board::sizeHint() const { return QSize(kLW, kLH); }

void Board::setLetterbox(bool on) {
    m_letterbox = on;
    update();
}

void Board::resizeEvent(QResizeEvent*) {
    if (m_cascade) stopCascade();
}

void Board::setCardStyle(const QString& style) {
    if (style == m_style) return;
    m_style = style;
    for (auto*& r : m_svg) {
        delete r;
        r = nullptr;
    }
    for (auto& pm : m_cache) pm = QPixmap();
    update();
}

const QPixmap& Board::cardPixmap(int card, bool inverted) {
    double dpr = devicePixelRatioF();
    double key = scale() * dpr;
    if (std::abs(key - m_cacheScale) > 1e-6) {
        for (auto& pm : m_cache) pm = QPixmap();
        m_cacheScale = key;
    }
    QPixmap& pm = m_cache[card + (inverted ? 52 : 0)];
    if (pm.isNull()) {
        static const char* ranks[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
        static const char* suits = "CDHS";
        QSvgRenderer*& svg = m_svg[card];
        if (!svg)
            svg = new QSvgRenderer(QStringLiteral(":/faces/%1/%2%3.svg")
                                       .arg(m_style, ranks[rankOf(card)])
                                       .arg(suits[suitOf(card)]),
                                   this);
        QImage img(int(std::ceil(kCW * key)), int(std::ceil(kCH * key)), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            svg->render(&p, QRectF(img.rect()));
        }
        // XP shows a selected card as its color negative.
        if (inverted) img.invertPixels();
        pm = QPixmap::fromImage(img);
    }
    return pm;
}

void Board::drawCard(QPainter& p, int card, const QPointF& pos, bool selected) {
    const QPixmap& pm = cardPixmap(card, selected);
    p.drawPixmap(QRectF(pos, QSizeF(kCW, kCH)), pm, QRectF(pm.rect()));
}

void Board::drawSlot(QPainter& p, const QPointF& pos) const {
    // XP: each free cell and foundation is its own sunken 71x96 box, dark on
    // the top and left, bright on the bottom and right. The bright bottom edge
    // stops one unit short of the next box's dark left edge.
    double x = pos.x(), y = pos.y();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(QRectF(x, y, kCW - 1, 1), kDark);
    p.fillRect(QRectF(x, y, 1, kCH - 1), kDark);
    p.fillRect(QRectF(x + kCW - 1, y + 1, 1, kCH - 1), kBright);
    p.fillRect(QRectF(x + 1, y + kCH - 1, kCW - 1, 1), kBright);
}

const QPixmap& Board::kingPixmap() {
    double key = scale() * devicePixelRatioF();
    if (std::abs(key - m_kingScale) > 1e-6) {
        for (auto& pm : m_kingPm) pm = QPixmap();
        m_kingScale = key;
    }
    QPixmap& pm = m_kingPm[m_kingLeft ? 1 : 0];
    if (pm.isNull()) {
        if (!m_kingSvg) m_kingSvg = new QSvgRenderer(QStringLiteral(":/art/king.svg"), this);
        int side = int(std::ceil(kKingIcon * key));
        QImage img(side, side, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        {
            QPainter ip(&img);
            ip.setRenderHint(QPainter::Antialiasing);
            // The art faces right; mirror it when the mouse is on the left.
            if (m_kingLeft) ip.setTransform(QTransform(-1, 0, 0, 1, side, 0));
            // Rendering into an image of exactly the icon's size also crops the
            // art to its viewBox (Qt SVG ignores the file's clipPath).
            m_kingSvg->render(&ip, QRectF(img.rect()));
        }
        pm = QPixmap::fromImage(img);
    }
    return pm;
}

void Board::drawKing(QPainter& p) {
    // XP: a raised 38x38 box, bright on the top and left, dark on the bottom
    // and right.
    const double x = kKingX, y = kKingY, b = kKingBox;
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(QRectF(x, y, b - 1, 1), kBright);
    p.fillRect(QRectF(x, y, 1, b - 1), kBright);
    p.fillRect(QRectF(x + b - 1, y + 1, 1, b - 1), kDark);
    p.fillRect(QRectF(x + 1, y + b - 1, b - 1, 1), kDark);
    const QPixmap& pm = kingPixmap();
    p.drawPixmap(QRectF(x + 3, y + 3, kKingIcon, kKingIcon), pm, QRectF(pm.rect()));
}

void Board::setKingLeft(bool left) {
    if (left == m_kingLeft) return;
    m_kingLeft = left;
    update();
}

void Board::paintBoard(QPainter& p, bool overlays) {
    p.fillRect(QRectF(0, 0, kLW, logicalHeight() + 1), kFelt);
    drawKing(p);

    auto isSel = [&](Area a, int idx, int k) {
        if (!m_sel.on || m_sel.area != a || m_sel.idx != idx) return false;
        return a != Area::Cascade || k >= m_sel.first;
    };

    for (int i = 0; i < kCells; ++i) {
        drawSlot(p, cellPos(i));
        int c = m_game.cell(i);
        if (c >= 0 && !m_hidden.contains(c)) drawCard(p, c, cellPos(i), isSel(Area::Cell, i, 0));
    }
    for (int i = 0; i < kFounds; ++i) {
        drawSlot(p, foundPos(i));
        const auto& f = m_game.foundation(i);
        for (int k = int(f.size()) - 1; k >= 0; --k)
            if (!m_hidden.contains(f[k])) { drawCard(p, f[k], foundPos(i), isSel(Area::Foundation, i, 0)); break; }
    }
    for (int i = 0; i < kCascades; ++i) {
        const auto& c = m_game.cascade(i);
        for (int k = 0; k < int(c.size()); ++k)
            if (!m_hidden.contains(c[k])) drawCard(p, c[k], cascadePos(i, k), isSel(Area::Cascade, i, k));
    }
    if (!overlays) return;

    if (m_hintOn) {
        QPointF pos;
        if (m_hint.to == Area::Cascade) {
            int n = int(m_game.cascade(m_hint.toIdx).size());
            pos = cascadePos(m_hint.toIdx, std::max(0, n - 1));
        } else {
            pos = cardPos(m_hint.to, m_hint.toIdx, 0);
        }
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(255, 255, 0), 3));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(pos, QSizeF(kCW, kCH)).adjusted(1, 1, -1, -1), 5, 5);
    }
    if (m_dragging) {
        QPointF tl = m_dragPos - (m_pressPos - m_dragOrigin);
        for (size_t k = 0; k < m_dragCards.size(); ++k)
            drawCard(p, m_dragCards[k], tl + QPointF(0, k * offset()), false);
    }
    if (m_flight.active) {
        double t = std::min(1.0, m_clock.elapsed() / double(m_flight.duration));
        t = t * t * (3 - 2 * t);
        for (size_t k = 0; k < m_flight.cards.size(); ++k) {
            QPointF pos = m_flight.from[k] + (m_flight.to[k] - m_flight.from[k]) * t;
            drawCard(p, m_flight.cards[k], pos, false);
        }
    }
}

void Board::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (m_cascade && !m_buf.isNull()) {
        p.drawPixmap(0, 0, m_buf);
        return;
    }
    if (m_letterbox) p.fillRect(rect(), kFelt);
    toLogical(p);
    paintBoard(p, true);
}
