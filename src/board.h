#pragma once
#include <QElapsedTimer>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include <array>
#include <vector>

#include "game.h"

class QSvgRenderer;

class Board : public QWidget {
    Q_OBJECT
public:
    explicit Board(QWidget* parent = nullptr);

    void startGame(const std::vector<int>& deal);
    void restartGame();
    // Deal, then replay a saved move history (see Game::history).
    void restoreGame(const std::vector<int>& deal, const std::vector<fc::Move>& moves);
    void undo();
    void showHint();
    void setAnimations(bool on) { m_animate = on; }
    // Card face style: a folder under :/faces/ ("faithful" or "refined").
    void setCardStyle(const QString& style);
    const QString& cardStyle() const { return m_style; }

    bool gameInProgress() const;
    const fc::Game& game() const { return m_game; }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void stateChanged();
    void gameWon();
    void gameStuck();
    void noHint();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;

private:
    struct Hit {
        fc::Area area = fc::Area::Cascade;
        int idx = -1;      // pile index; -1 = nothing hit
        int cardIdx = -1;  // cascade card index, or 0 for occupied cell/foundation, -1 for empty space
    };
    struct Selection {
        bool on = false;
        fc::Area area = fc::Area::Cascade;
        int idx = 0;
        int first = 0;  // first card index (cascade)
        int count = 0;
    };
    struct Flight {
        std::vector<int> cards;
        std::vector<QPointF> from, to;
        qint64 duration = 150;
        bool active = false;
    };
    struct Particle {
        int card = 0;
        double x = 0, y = 0, vx = 0, vy = 0;
    };

    // geometry (logical units)
    double logicalHeight() const;
    double offset() const;
    double scale() const;
    QPointF cascadePos(int col, int k) const;
    QPointF cellPos(int i) const;
    QPointF foundPos(int i) const;
    QPointF cardPos(fc::Area a, int idx, int k) const;
    QPointF logical(const QPointF& p) const;
    Hit hitTest(const QPointF& p) const;

    // painting
    void paintBoard(QPainter& p, bool withOverlays);
    void drawKing(QPainter& p);
    const QPixmap& kingPixmap();
    void setKingLeft(bool left);
    const QPixmap& cardPixmap(int card, bool inverted = false);
    void drawCard(QPainter& p, int card, const QPointF& pos, bool selected);
    void drawSlot(QPainter& p, const QPointF& pos) const;

    // game flow
    bool doMove(fc::Move m, const std::vector<QPointF>* fromOverride = nullptr);
    void afterMove();
    void tick();
    void startCascade();
    void stopCascade();
    bool draggable(const Hit& h) const;
    bool selectable(const Hit& h) const;
    void selectPile(const Hit& h);
    bool tryDestination(const Hit& dest);
    void clearHint();
    void clearSelection();
    void resetView();

    fc::Game m_game;
    Selection m_sel;
    Flight m_flight;
    QElapsedTimer m_clock;
    QTimer m_timer;
    bool m_animate = true;

    // press / drag
    bool m_pressing = false;
    bool m_dragging = false;
    Hit m_pressHit;
    QPointF m_pressPos, m_dragPos;
    std::vector<int> m_dragCards;
    QPointF m_dragOrigin;  // top-left of first dragged card at press
    QSet<int> m_hidden;

    // hint
    bool m_hintOn = false;
    fc::Move m_hint;

    // win cascade
    bool m_cascade = false;
    QPixmap m_buf;
    std::vector<Particle> m_queue;
    Particle m_cur;
    bool m_curActive = false;

    // card cache
    std::array<QPixmap, 104> m_cache;  // 52 normal faces, then 52 inverted
    double m_cacheScale = 0;
    QString m_style = QStringLiteral("faithful");
    std::array<QSvgRenderer*, 52> m_svg{};  // parsed faces, owned by this widget

    // king between the free cells and the foundations
    QSvgRenderer* m_kingSvg = nullptr;
    std::array<QPixmap, 2> m_kingPm;  // looking right, looking left
    double m_kingScale = 0;
    bool m_kingLeft = false;  // looks toward the side the mouse is on
};
