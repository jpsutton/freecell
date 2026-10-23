#include "game.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace fc;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static std::string name(int c) {
    const char* r = "A23456789TJQK";
    const char* s = "CDHS";
    return std::string(1, r[rankOf(c)]) + s[suitOf(c)];
}

int main() {
    // Game #1 first row (well-known): JD 2D 9H JC 5D 7H 7C 5H
    auto d = msDeal(1);
    std::string row;
    for (int i = 0; i < 8; ++i) row += name(d[i]) + " ";
    std::printf("game1 row0: %s\n", row.c_str());
    CHECK(row == "JD 2D 9H JC 5D 7H 7C 5H ");

    // Game #617 first row: 7D AD 5C 3S 5S 8C 2D AH
    d = msDeal(617);
    row.clear();
    for (int i = 0; i < 8; ++i) row += name(d[i]) + " ";
    std::printf("game617 row0: %s\n", row.c_str());
    CHECK(row == "7D AD 5C 3S 5S 8C 2D AH ");

    // deal is a permutation
    for (uint32_t g : {1u, 2u, 11982u, 32000u, 1000000u}) {
        auto x = msDeal(g);
        bool seen[52] = {};
        for (int c : x) seen[c] = true;
        for (bool s : seen) CHECK(s);
    }

    // basic play + undo
    Game g;
    g.deal(msDeal(1));
    CHECK(g.cardsLeft() == 52);
    CHECK(g.cascade(0).size() == 7 && g.cascade(4).size() == 6);
    CHECK(g.maxMovable(false) == 5);
    auto moves = g.legalMoves();
    CHECK(!moves.empty());
    Move h;
    CHECK(g.hint(h));
    CHECK(g.apply(h));
    CHECK(g.canUndo());
    g.undo();
    CHECK(!g.canUndo());
    CHECK(g.cascade(0).size() == 7);

    // hint-driven random playthrough doesn't corrupt state
    for (uint32_t n = 1; n <= 200; ++n) {
        g.deal(msDeal(n));
        for (int step = 0; step < 300 && !g.won(); ++step) {
            Move m;
            if (!g.hint(m)) break;
            CHECK(g.apply(m));
            Move a;
            while (g.nextAutoMove(a)) CHECK(g.apply(a));
        }
        int total = g.cardsLeft();
        for (int i = 0; i < kCascades; ++i) total -= 0;
        int onTable = 0;
        for (int i = 0; i < kCascades; ++i) onTable += (int)g.cascade(i).size();
        for (int i = 0; i < kCells; ++i) onTable += g.cell(i) >= 0;
        CHECK(onTable == total);
    }

    // taking a card back off a foundation: no immediate autoplay, undo restores
    g.deal(msDeal(1));
    for (int step = 0; step < 300 && g.cardsLeft() == 52; ++step) {
        Move m;
        if (!g.hint(m)) break;
        CHECK(g.apply(m));
        Move a;
        while (g.nextAutoMove(a)) CHECK(g.apply(a));
    }
    int fi = -1, cl = -1;
    for (int i = 0; i < kFounds; ++i) if (!g.foundation(i).empty()) fi = i;
    for (int i = 0; i < kCells; ++i) if (g.cell(i) < 0) cl = i;
    CHECK(fi >= 0 && cl >= 0);
    if (fi >= 0 && cl >= 0) {
        int card = g.foundation(fi).back();
        int left = g.cardsLeft();
        CHECK(!g.canMove({Area::Foundation, fi, Area::Foundation, (fi + 1) % kFounds, 1, false}));
        CHECK(g.apply({Area::Foundation, fi, Area::Cell, cl, 1, false}));
        CHECK(g.cell(cl) == card && g.cardsLeft() == left + 1);
        Move a;
        CHECK(!(g.nextAutoMove(a) && a.from == Area::Cell && a.fromIdx == cl));

        // replaying the history reproduces the same position
        Game r;
        r.deal(g.dealOrder());
        for (const Move& m : g.history()) CHECK(r.apply(m));
        CHECK(r.cell(cl) == card && r.cardsLeft() == g.cardsLeft());

        g.undo();
        CHECK(g.cell(cl) < 0 && g.foundation(fi).back() == card);
    }
    std::printf(failures ? "FAILED\n" : "OK\n");
    return failures ? 1 : 0;
}
