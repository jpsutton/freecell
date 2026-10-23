#include "game.h"

#include <algorithm>

namespace fc {

std::vector<int> msDeal(uint32_t gameNumber) {
    std::vector<int> deck(52);
    for (int i = 0; i < 52; ++i) deck[i] = i;
    std::vector<int> out;
    out.reserve(52);
    uint32_t seed = gameNumber;
    int left = 52;
    for (int i = 0; i < 52; ++i) {
        seed = seed * 214013u + 2531011u;
        int j = (int)((seed >> 16) & 0x7fff) % left;
        out.push_back(deck[j]);
        deck[j] = deck[--left];
    }
    return out;
}

Game::Game() {
    m_cells.fill(-1);
}

void Game::deal(const std::vector<int>& order) {
    m_deal = order;
    restart();
}

void Game::restart() {
    for (auto& c : m_casc) c.clear();
    for (auto& f : m_found) f.clear();
    m_cells.fill(-1);
    m_undo.clear();
    m_noAuto = 0;
    for (size_t i = 0; i < m_deal.size(); ++i) m_casc[i % kCascades].push_back(m_deal[i]);
}

int Game::emptyCells() const {
    int n = 0;
    for (int c : m_cells) n += (c < 0);
    return n;
}

int Game::emptyCascades() const {
    int n = 0;
    for (auto& c : m_casc) n += c.empty();
    return n;
}

int Game::maxMovable(bool toEmptyCascade) const {
    int e = emptyCascades();
    if (toEmptyCascade && e > 0) --e;
    return (emptyCells() + 1) * (1 << e);
}

bool Game::isSequence(int ci, int count) const {
    const auto& c = m_casc[ci];
    int n = (int)c.size();
    if (count < 1 || count > n) return false;
    for (int i = n - count; i + 1 < n; ++i) {
        int a = c[i], b = c[i + 1];
        if (rankOf(a) != rankOf(b) + 1 || isRed(a) == isRed(b)) return false;
    }
    return true;
}

bool Game::take(Area a, int idx, int count, std::vector<int>& out) const {
    out.clear();
    if (a == Area::Cascade) {
        if (idx < 0 || idx >= kCascades) return false;
        const auto& c = m_casc[idx];
        if (count < 1 || count > (int)c.size()) return false;
        out.assign(c.end() - count, c.end());
        return true;
    }
    if (a == Area::Cell) {
        if (idx < 0 || idx >= kCells || count != 1 || m_cells[idx] < 0) return false;
        out.push_back(m_cells[idx]);
        return true;
    }
    if (idx < 0 || idx >= kFounds || count != 1 || m_found[idx].empty()) return false;
    out.push_back(m_found[idx].back());
    return true;
}

void Game::put(Area a, int idx, const std::vector<int>& cards) {
    if (a == Area::Cascade) m_casc[idx].insert(m_casc[idx].end(), cards.begin(), cards.end());
    else if (a == Area::Cell) m_cells[idx] = cards[0];
    else m_found[idx].push_back(cards[0]);
}

void Game::remove(Area a, int idx, int count) {
    if (a == Area::Cascade) m_casc[idx].resize(m_casc[idx].size() - count);
    else if (a == Area::Cell) m_cells[idx] = -1;
    else m_found[idx].pop_back();
}

bool Game::canMove(const Move& m) const {
    if (m.from == Area::Foundation && m.to == Area::Foundation) return false;
    if (m.from == Area::Cascade && !isSequenceSafe(m)) return false;
    std::vector<int> cards;
    if (!take(m.from, m.fromIdx, m.count, cards)) return false;
    if (m.from == m.to && m.fromIdx == m.toIdx) return false;
    switch (m.to) {
    case Area::Cell:
        return m.count == 1 && m.toIdx >= 0 && m.toIdx < kCells && m_cells[m.toIdx] < 0;
    case Area::Foundation: {
        if (m.count != 1 || m.toIdx < 0 || m.toIdx >= kFounds) return false;
        const auto& f = m_found[m.toIdx];
        int c = cards[0];
        if (f.empty()) return rankOf(c) == 0;
        return suitOf(f.back()) == suitOf(c) && rankOf(c) == rankOf(f.back()) + 1;
    }
    case Area::Cascade: {
        if (m.toIdx < 0 || m.toIdx >= kCascades) return false;
        const auto& d = m_casc[m.toIdx];
        if (d.empty()) return m.count <= maxMovable(true);
        if (m.count > maxMovable(false)) return false;
        int top = d.back(), c = cards[0];
        return rankOf(top) == rankOf(c) + 1 && isRed(top) != isRed(c);
    }
    }
    return false;
}

bool Game::isSequenceSafe(const Move& m) const {
    return m.fromIdx >= 0 && m.fromIdx < kCascades && isSequence(m.fromIdx, m.count);
}

bool Game::apply(Move m) {
    if (!canMove(m)) return false;
    std::vector<int> cards;
    take(m.from, m.fromIdx, m.count, cards);
    remove(m.from, m.fromIdx, m.count);
    put(m.to, m.toIdx, cards);
    // A card the player takes back off a foundation must not autoplay straight
    // back; it is eligible again once the player returns it there.
    m.noAutoBefore = m_noAuto;
    if (m.from == Area::Foundation) m_noAuto |= uint64_t(1) << cards[0];
    if (m.to == Area::Foundation) m_noAuto &= ~(uint64_t(1) << cards[0]);
    m_undo.push_back(m);
    return true;
}

bool Game::undo() {
    while (!m_undo.empty()) {
        Move m = m_undo.back();
        m_undo.pop_back();
        std::vector<int> cards;
        take(m.to, m.toIdx, m.count, cards);
        remove(m.to, m.toIdx, m.count);
        put(m.from, m.fromIdx, cards);
        m_noAuto = m.noAutoBefore;
        if (!m.chained) break;
    }
    return true;
}

int Game::foundCountOfSuit(int suit) const {
    for (auto& f : m_found)
        if (!f.empty() && suitOf(f.back()) == suit) return (int)f.size();
    return 0;
}

int Game::foundationSlotFor(int c) const {
    if (rankOf(c) == 0) {
        for (int i = 0; i < kFounds; ++i)
            if (m_found[i].empty()) return i;
        return -1;
    }
    for (int i = 0; i < kFounds; ++i)
        if (!m_found[i].empty() && suitOf(m_found[i].back()) == suitOf(c) &&
            rankOf(m_found[i].back()) == rankOf(c) - 1)
            return i;
    return -1;
}

bool Game::nextAutoMove(Move& out) const {
    auto safe = [&](int c) {
        if (m_noAuto & (uint64_t(1) << c)) return false;
        int r = rankOf(c);
        if (r <= 1) return true;
        int s1 = isRed(c) ? 0 : 1, s2 = isRed(c) ? 3 : 2;
        return foundCountOfSuit(s1) >= r && foundCountOfSuit(s2) >= r;
    };
    for (int i = 0; i < kCells; ++i) {
        int c = m_cells[i];
        if (c < 0) continue;
        int s = foundationSlotFor(c);
        if (s >= 0 && safe(c)) { out = {Area::Cell, i, Area::Foundation, s, 1, true}; return true; }
    }
    for (int i = 0; i < kCascades; ++i) {
        if (m_casc[i].empty()) continue;
        int c = m_casc[i].back();
        int s = foundationSlotFor(c);
        if (s >= 0 && safe(c)) { out = {Area::Cascade, i, Area::Foundation, s, 1, true}; return true; }
    }
    return false;
}

std::vector<Move> Game::legalMoves() const {
    std::vector<Move> res;
    int firstEmptyCell = -1, firstEmptyCasc = -1;
    for (int i = 0; i < kCells; ++i) if (m_cells[i] < 0) { firstEmptyCell = i; break; }
    for (int i = 0; i < kCascades; ++i) if (m_casc[i].empty()) { firstEmptyCasc = i; break; }

    auto tryDests = [&](Area fa, int fi, int count, int card) {
        int fs = (count == 1) ? foundationSlotFor(card) : -1;
        if (fs >= 0) {
            Move m{fa, fi, Area::Foundation, fs, 1, false};
            if (canMove(m)) res.push_back(m);
        }
        for (int d = 0; d < kCascades; ++d) {
            if (m_casc[d].empty() && d != firstEmptyCasc) continue;
            if (m_casc[d].empty() && fa == Area::Cascade && (int)m_casc[fi].size() == count) continue;
            Move m{fa, fi, Area::Cascade, d, count, false};
            if (canMove(m)) res.push_back(m);
        }
        if (fa == Area::Cascade && count == 1 && firstEmptyCell >= 0) {
            Move m{fa, fi, Area::Cell, firstEmptyCell, 1, false};
            if (canMove(m)) res.push_back(m);
        }
    };
    for (int i = 0; i < kCells; ++i)
        if (m_cells[i] >= 0) tryDests(Area::Cell, i, 1, m_cells[i]);
    for (int i = 0; i < kCascades; ++i) {
        int n = (int)m_casc[i].size();
        for (int k = 1; k <= n && isSequence(i, k); ++k) tryDests(Area::Cascade, i, k, m_casc[i][n - k]);
    }
    return res;
}

bool Game::hasMoves() const { return !legalMoves().empty(); }

bool Game::hint(Move& out) const {
    auto moves = legalMoves();
    int best = -1;
    for (auto& m : moves) {
        if (!m_undo.empty()) {
            const Move& l = m_undo.back();
            if (m.from == l.to && m.fromIdx == l.toIdx && m.to == l.from && m.toIdx == l.fromIdx) continue;
        }
        int score = 0;
        if (m.to == Area::Foundation) score = 100;
        else if (m.to == Area::Cascade) {
            bool destEmpty = m_casc[m.toIdx].empty();
            if (!destEmpty) {
                score = (m.from == Area::Cell) ? 60 : 50;
                if (m.from == Area::Cascade) {
                    const auto& s = m_casc[m.fromIdx];
                    if ((int)s.size() == m.count) score += 15;
                    else {
                        int under = s[s.size() - m.count - 1];
                        if (foundationSlotFor(under) >= 0) score += 10;
                    }
                }
            } else {
                score = (m.from == Area::Cell) ? 20 : 5;
            }
        } else {
            score = 10;
        }
        if (score > best) { best = score; out = m; }
    }
    return best >= 0;
}

bool Game::won() const {
    for (auto& f : m_found) if (f.size() != 13) return false;
    return true;
}

int Game::cardsLeft() const {
    int n = 52;
    for (auto& f : m_found) n -= (int)f.size();
    return n;
}

}  // namespace fc
