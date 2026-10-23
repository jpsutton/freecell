#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace fc {

constexpr int kCascades = 8;
constexpr int kCells = 4;
constexpr int kFounds = 4;

// Card value 0..51: rank = v / 4 (0 = Ace .. 12 = King), suit = v % 4
// (0 clubs, 1 diamonds, 2 hearts, 3 spades). Same encoding as MS FreeCell.
inline int rankOf(int c) { return c / 4; }
inline int suitOf(int c) { return c % 4; }
inline bool isRed(int c) { int s = suitOf(c); return s == 1 || s == 2; }

enum class Area { Cascade, Cell, Foundation };

struct Move {
    Area from = Area::Cascade;
    int fromIdx = 0;
    Area to = Area::Cascade;
    int toIdx = 0;
    int count = 1;
    bool chained = false;  // auto-play move; undone together with previous move
    uint64_t noAutoBefore = 0;  // set by apply: Game's no-autoplay mask before this move
};

// Deal from MS FreeCell game number (1..1000000 typical). Returns 52 cards,
// card k goes to cascade k % 8.
std::vector<int> msDeal(uint32_t gameNumber);

class Game {
public:
    Game();
    void deal(const std::vector<int>& order);  // order: 52 cards, dealt round-robin
    void restart();

    const std::vector<int>& cascade(int i) const { return m_casc[i]; }
    int cell(int i) const { return m_cells[i]; }  // -1 empty
    const std::vector<int>& foundation(int i) const { return m_found[i]; }

    int emptyCells() const;
    int emptyCascades() const;
    int maxMovable(bool toEmptyCascade) const;
    bool isSequence(int cascadeIdx, int count) const;

    bool canMove(const Move& m) const;
    bool apply(Move m);       // validates; records undo
    bool undo();              // undoes one user move (plus chained auto moves)
    bool canUndo() const { return !m_undo.empty(); }
    int moveCount() const { return (int)m_undo.size(); }
    const std::vector<int>& dealOrder() const { return m_deal; }
    const std::vector<Move>& history() const { return m_undo; }

    bool nextAutoMove(Move& out) const;  // safe move to foundation
    std::vector<Move> legalMoves() const;
    bool hasMoves() const;
    bool hint(Move& out) const;

    bool won() const;
    int cardsLeft() const;  // not yet on foundations
    int foundationSlotFor(int card) const;  // -1 if none

private:
    bool take(Area a, int idx, int count, std::vector<int>& out) const;
    void put(Area a, int idx, const std::vector<int>& cards);
    void remove(Area a, int idx, int count);
    int foundCountOfSuit(int suit) const;

    std::array<std::vector<int>, kCascades> m_casc;
    std::array<int, kCells> m_cells;
    std::array<std::vector<int>, kFounds> m_found;
    std::vector<Move> m_undo;
    std::vector<int> m_deal;
    uint64_t m_noAuto = 0;  // bit per card taken back off a foundation; autoplay skips these
    bool isSequenceSafe(const Move& m) const;
};

}  // namespace fc
