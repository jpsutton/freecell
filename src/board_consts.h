#pragma once

// Logical layout units: XP FreeCell's client-area pixels (board is scaled to widget width).
constexpr int kCW = 71;    // card width
constexpr int kCH = 96;    // card height
constexpr int kLW = 632;   // logical board width (XP client width at 640x480)
constexpr int kTopY = 0;   // free cell / foundation row y
constexpr int kCY = 106;   // cascade start y
constexpr int kGap = (kLW - 8 * kCW) / 9;  // gap around and between cascades (7)
constexpr int kFoundX = kLW - 4 * kCW;     // first foundation x (348)
constexpr int kKingBox = 38;               // raised box around the king
constexpr int kKingX = kLW / 2 - kKingBox / 2;  // 297
constexpr int kKingY = 18;
constexpr int kKingIcon = 32;  // king art, inset 3 units in the box
