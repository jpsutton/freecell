#include "board.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyleHints>
#include <QPainter>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

#include "board_consts.h"

using namespace fc;

Board::Board(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);  // the king follows the mouse
    m_timer.setInterval(15);
    connect(&m_timer, &QTimer::timeout, this, &Board::tick);
    m_game.deal(msDeal(1));
}

bool Board::gameInProgress() const { return m_game.moveCount() > 0 && !m_game.won(); }

void Board::startGame(const std::vector<int>& deal) {
    m_game.deal(deal);
    resetView();
}

void Board::restoreGame(const std::vector<int>& deal, const std::vector<Move>& moves) {
    m_game.deal(deal);
    for (const Move& m : moves)
        if (!m_game.apply(m)) break;
    resetView();
}

void Board::restartGame() {
    m_game.restart();
    resetView();
}

void Board::resetView() {
    m_cascade = false;
    m_buf = QPixmap();
    m_flight.active = false;
    m_hidden.clear();
    m_timer.stop();
    m_sel.on = false;
    m_hintOn = false;
    m_pressing = m_dragging = false;
    emit stateChanged();
    update();
}

// ---- hit testing ----

Board::Hit Board::hitTest(const QPointF& p) const {
    Hit h;
    if (p.y() < kCY - 6) {
        for (int i = 0; i < kCells; ++i) {
            if (QRectF(cellPos(i), QSizeF(kCW, kCH)).contains(p)) {
                h.area = Area::Cell; h.idx = i; h.cardIdx = m_game.cell(i) >= 0 ? 0 : -1;
                return h;
            }
        }
        for (int i = 0; i < kFounds; ++i) {
            if (QRectF(foundPos(i), QSizeF(kCW, kCH)).contains(p)) {
                h.area = Area::Foundation; h.idx = i;
                h.cardIdx = m_game.foundation(i).empty() ? -1 : 0;
                return h;
            }
        }
        return h;
    }
    for (int i = 0; i < kCascades; ++i) {
        double x = cascadePos(i, 0).x();
        if (p.x() < x - 4 || p.x() > x + kCW + 4) continue;
        h.area = Area::Cascade;
        h.idx = i;
        int n = int(m_game.cascade(i).size());
        for (int k = n - 1; k >= 0; --k) {
            double top = cascadePos(i, k).y();
            double bottom = (k == n - 1) ? top + kCH : top + offset();
            if (p.y() >= top && p.y() < bottom && p.x() >= x && p.x() <= x + kCW) { h.cardIdx = k; break; }
        }
        return h;
    }
    return h;
}

// Movement (logical units) before a press becomes a drag. A finger wobbles
// more than a mouse, so touch screens use the platform's drag distance.
double Board::dragSlop() const {
#ifdef FREECELL_TOUCH_UI
    return std::max(4.0, QGuiApplication::styleHints()->startDragDistance() / scale());
#else
    return 4;
#endif
}

bool Board::draggable(const Hit& h) const {
    if (h.idx < 0 || h.cardIdx < 0) return false;
    if (h.area == Area::Cell) return m_game.cell(h.idx) >= 0;
    if (h.area == Area::Foundation) return !m_game.foundation(h.idx).empty();
    if (h.area == Area::Cascade) {
        int n = int(m_game.cascade(h.idx).size());
        return m_game.isSequence(h.idx, n - h.cardIdx);
    }
    return false;
}

void Board::clearSelection() { m_sel.on = false; update(); }

void Board::selectPile(const Hit& h) {
    // XP selects a whole pile and shows only its top card inverted. How many
    // cards move is decided at the destination (see tryDestination).
    int first = h.area == Area::Cascade ? int(m_game.cascade(h.idx).size()) - 1 : 0;
    m_sel = {true, h.area, h.idx, first, 1};
    update();
}

bool Board::selectable(const Hit& h) const {
    if (h.idx < 0) return false;
    if (h.area == Area::Cell) return m_game.cell(h.idx) >= 0;
    if (h.area == Area::Cascade) return !m_game.cascade(h.idx).empty();
    return !m_game.foundation(h.idx).empty();
}

void Board::clearHint() {
    if (!m_hintOn) return;
    m_hintOn = false;
    m_sel.on = false;
    update();
}

bool Board::tryDestination(const Hit& dest) {
    Move m{m_sel.area, m_sel.idx, dest.area, dest.idx, m_sel.count, false};
    m_sel.on = false;
    update();
    if (m.from == dest.area && m.fromIdx == dest.idx) return true;  // deselect
    if (dest.area == Area::Foundation && m.count == 1 && m.from != Area::Foundation) {
        int card = m.from == Area::Cell ? m_game.cell(m.fromIdx) : m_game.cascade(m.fromIdx).back();
        int s = m_game.foundationSlotFor(card);
        if (s >= 0) m.toIdx = s;
    }
    if (m.from == Area::Cascade && dest.area == Area::Cascade) {
        // Move the run that fits: onto a card only one length can; onto an
        // empty cascade, the longest movable run.
        for (int k = int(m_game.cascade(m.fromIdx).size()); k >= 1; --k) {
            m.count = k;
            if (m_game.canMove(m)) return doMove(m);
        }
        return false;
    }
    m.count = 1;
    if (m_game.canMove(m)) return doMove(m);
    return false;
}

// ---- mouse ----

void Board::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    if (m_cascade) { stopCascade(); return; }
    if (m_flight.active) return;
    QPointF p = logical(e->position());
    clearHint();
    Hit h = hitTest(p);
    if (m_sel.on) {
        if (h.idx < 0) { clearSelection(); return; }
        if (!tryDestination(h) && selectable(h)) selectPile(h);
        m_pressing = false;
        return;
    }
    if (draggable(h)) {
        m_pressing = true;
        m_pressHit = h;
        m_pressPos = p;
        m_dragOrigin = cardPos(h.area, h.idx, h.cardIdx);
    } else if (selectable(h)) {
        selectPile(h);
    }
}

void Board::mouseMoveEvent(QMouseEvent* e) {
    QPointF p = logical(e->position());
    setKingLeft(p.x() < kLW / 2.0);
    if (!m_pressing) return;
    if (!m_dragging) {
        if ((p - m_pressPos).manhattanLength() <= dragSlop()) return;
        m_dragging = true;
        m_dragCards.clear();
        if (m_pressHit.area == Area::Cell) {
            m_dragCards.push_back(m_game.cell(m_pressHit.idx));
        } else if (m_pressHit.area == Area::Foundation) {
            m_dragCards.push_back(m_game.foundation(m_pressHit.idx).back());
        } else {
            const auto& c = m_game.cascade(m_pressHit.idx);
            m_dragCards.assign(c.begin() + m_pressHit.cardIdx, c.end());
        }
        m_hidden.clear();
        for (int c : m_dragCards) m_hidden.insert(c);
    }
    m_dragPos = p;
    update();
}

void Board::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    if (m_dragging) {
        m_dragging = false;
        m_pressing = false;
        QPointF tl = m_dragPos - (m_pressPos - m_dragOrigin);
        QRectF dr(tl, QSizeF(kCW, kCH));
        int count = int(m_dragCards.size());
        Move best;
        double bestArea = 0;
        auto consider = [&](Move m, const QRectF& pile) {
            QRectF in = dr.intersected(pile);
            double a = in.isEmpty() ? 0 : in.width() * in.height();
            if (a > bestArea && m_game.canMove(m)) { bestArea = a; best = m; }
        };
        for (int i = 0; i < kCascades; ++i)
            consider({m_pressHit.area, m_pressHit.idx, Area::Cascade, i, count, false},
                     QRectF(cascadePos(i, 0).x() - 4, kCY - 10, kCW + 8, logicalHeight()));
        if (count == 1) {
            for (int i = 0; i < kCells; ++i)
                consider({m_pressHit.area, m_pressHit.idx, Area::Cell, i, 1, false},
                         QRectF(cellPos(i), QSizeF(kCW, kCH)));
            for (int i = 0; i < kFounds; ++i)
                consider({m_pressHit.area, m_pressHit.idx, Area::Foundation, i, 1, false},
                         QRectF(foundPos(i), QSizeF(kCW, kCH)));
        }
        std::vector<QPointF> from;
        for (int k = 0; k < count; ++k) from.push_back(tl + QPointF(0, k * offset()));
        m_hidden.clear();
        if (bestArea > 0) doMove(best, &from);
        update();
        return;
    }
    if (m_pressing) {
        m_pressing = false;
        selectPile(m_pressHit);
    }
}

void Board::mouseDoubleClickEvent(QMouseEvent* e) {
#ifdef FREECELL_TOUCH_UI
    // Two quick taps (select a card, then its destination) arrive as a double
    // click in place of the second press. Handle it as a plain tap so the move
    // is made, not an auto-move of the card under the second tap.
    mousePressEvent(e);
    return;
#endif
    if (e->button() != Qt::LeftButton || m_cascade || m_flight.active) return;
    Hit h = hitTest(logical(e->position()));
    m_sel.on = false;
    m_pressing = false;
    update();
    if (h.idx < 0 || h.cardIdx < 0) return;
    int card;
    if (h.area == Area::Cell) card = m_game.cell(h.idx);
    else if (h.area == Area::Cascade && h.cardIdx == int(m_game.cascade(h.idx).size()) - 1)
        card = m_game.cascade(h.idx).back();
    else return;
    int s = m_game.foundationSlotFor(card);
    if (s >= 0) { doMove({h.area, h.idx, Area::Foundation, s, 1, false}); return; }
    if (h.area == Area::Cascade) {
        for (int i = 0; i < kCells; ++i)
            if (m_game.cell(i) < 0) { doMove({h.area, h.idx, Area::Cell, i, 1, false}); return; }
    } else {
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < kCascades; ++i) {
                bool empty = m_game.cascade(i).empty();
                if (empty != (pass == 1)) continue;
                Move m{Area::Cell, h.idx, Area::Cascade, i, 1, false};
                if (m_game.canMove(m)) { doMove(m); return; }
            }
    }
}

// ---- game flow ----

void Board::undo() {
    if (m_flight.active || m_cascade || !m_game.canUndo()) return;
    clearHint();
    m_sel.on = false;
    m_game.undo();
    emit stateChanged();
    update();
}

void Board::showHint() {
    if (m_flight.active || m_cascade || m_game.won()) return;
    clearHint();
    m_sel.on = false;
    Move h;
    if (!m_game.hint(h)) { emit noHint(); return; }
    m_hint = h;
    m_hintOn = true;
    m_sel.on = true;
    m_sel.area = h.from;
    m_sel.idx = h.fromIdx;
    m_sel.count = h.count;
    m_sel.first = h.from == Area::Cascade ? int(m_game.cascade(h.fromIdx).size()) - h.count : 0;
    update();
}

bool Board::doMove(Move m, const std::vector<QPointF>* fromOverride) {
    std::vector<int> cards;
    if (m.from == Area::Cascade) {
        const auto& c = m_game.cascade(m.fromIdx);
        if (m.count < 1 || m.count > int(c.size())) return false;
        cards.assign(c.end() - m.count, c.end());
    } else if (m.from == Area::Cell) {
        if (m_game.cell(m.fromIdx) < 0) return false;
        cards.push_back(m_game.cell(m.fromIdx));
    } else {
        if (m_game.foundation(m.fromIdx).empty()) return false;
        cards.push_back(m_game.foundation(m.fromIdx).back());
    }
    std::vector<QPointF> from;
    if (fromOverride) {
        from = *fromOverride;
    } else {
        int base = m.from == Area::Cascade ? int(m_game.cascade(m.fromIdx).size()) - m.count : 0;
        for (int k = 0; k < int(cards.size()); ++k) from.push_back(cardPos(m.from, m.fromIdx, base + k));
    }
    if (!m_game.apply(m)) return false;

    std::vector<QPointF> to;
    int base = m.to == Area::Cascade ? int(m_game.cascade(m.toIdx).size()) - m.count : 0;
    for (int k = 0; k < int(cards.size()); ++k) to.push_back(cardPos(m.to, m.toIdx, base + k));

    if (m_animate) {
        m_flight.cards = cards;
        m_flight.from = from;
        m_flight.to = to;
        m_flight.duration = m.chained ? 110 : 160;
        m_flight.active = true;
        m_hidden.clear();
        for (int c : cards) m_hidden.insert(c);
        m_clock.start();
        m_timer.start();
        update();
    } else {
        afterMove();
    }
    return true;
}

void Board::afterMove() {
    emit stateChanged();
    update();
    if (m_game.won()) {
        if (m_animate) startCascade();
        else emit gameWon();
        return;
    }
    Move a;
    if (m_game.nextAutoMove(a)) {
        doMove(a);
        return;
    }
    if (!m_game.hasMoves()) emit gameStuck();
}

void Board::tick() {
    if (m_cascade) {
        for (int step = 0; step < 3 && m_cascade; ++step) {
            if (!m_curActive) {
                if (m_queue.empty()) { stopCascade(); return; }
                m_cur = m_queue.back();
                m_queue.pop_back();
                m_curActive = true;
            }
            m_cur.x += m_cur.vx;
            m_cur.y += m_cur.vy;
            m_cur.vy += 0.5;
            QPointF tl = logical(QPointF(0, 0)), br = logical(QPointF(width(), height()));
            double floorY = br.y() - kCH;
            if (m_cur.y > floorY) {
                m_cur.y = floorY;
                m_cur.vy = -m_cur.vy * 0.8;
            }
            QPainter bp(&m_buf);
            bp.setRenderHint(QPainter::SmoothPixmapTransform);
            toLogical(bp);
            const QPixmap& pm = cardPixmap(m_cur.card);
            bp.drawPixmap(QRectF(m_cur.x, m_cur.y, kCW, kCH), pm, QRectF(pm.rect()));
            if (m_cur.x < tl.x() - kCW || m_cur.x > br.x()) m_curActive = false;
        }
        update();
        return;
    }
    if (m_flight.active) {
        if (m_clock.elapsed() >= m_flight.duration) {
            m_flight.active = false;
            m_hidden.clear();
            m_timer.stop();
            afterMove();
        } else {
            update();
        }
    }
}

void Board::startCascade() {
    double dpr = devicePixelRatioF();
    m_buf = QPixmap(size() * dpr);
    m_buf.setDevicePixelRatio(dpr);
    {
        QPainter bp(&m_buf);
        bp.setRenderHint(QPainter::SmoothPixmapTransform);
        if (m_letterbox) bp.fillRect(rect(), QColor(0, 127, 0));
        toLogical(bp);
        paintBoard(bp, false);
    }
    m_queue.clear();
    auto* rng = QRandomGenerator::global();
    for (int r = 12; r >= 0; --r)
        for (int s = 0; s < kFounds; ++s) {
            const auto& f = m_game.foundation(s);
            if (r >= int(f.size())) continue;
            Particle pt;
            pt.card = f[r];
            pt.x = foundPos(s).x();
            pt.y = foundPos(s).y();
            double sp = 2 + rng->bounded(5);
            pt.vx = rng->bounded(2) ? sp : -sp;
            pt.vy = -rng->bounded(7);
            m_queue.push_back(pt);
        }
    std::reverse(m_queue.begin(), m_queue.end());
    m_curActive = false;
    m_cascade = true;
    m_timer.start();
}

void Board::stopCascade() {
    if (!m_cascade) return;
    m_cascade = false;
    m_timer.stop();
    m_buf = QPixmap();
    update();
    emit gameWon();
}
