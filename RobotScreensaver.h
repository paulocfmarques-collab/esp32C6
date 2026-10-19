#pragma once
#include <Arduino_GFX_Library.h>
#include <math.h>

// Desenho vetorial inspirado nas dez expressoes da referencia.
// Coordenadas locais em uma area de 176 x 136 pixels.
namespace RobotScreensaver {
inline uint16_t dim(uint16_t color) { return (color & 0xE79C) >> 2; }

inline void curve(Arduino_GFX& g, int x, int y, int width, int bend, uint16_t color) {
    int previousX = x, previousY = y;
    for (int i = 1; i <= 16; ++i) {
        const float t = i / 16.0f;
        int nextX = x + int(width * t);
        int nextY = y + int(4 * bend * t * (1 - t));
        g.drawLine(previousX, previousY, nextX, nextY, color);
        g.drawLine(previousX, previousY + 1, nextX, nextY + 1, color);
        previousX = nextX; previousY = nextY;
    }
}

inline void heart(Arduino_GFX& g, int x, int y, uint16_t color) {
    g.fillCircle(x - 4, y - 3, 5, color);
    g.fillCircle(x + 4, y - 3, 5, color);
    g.fillTriangle(x - 9, y - 1, x + 9, y - 1, x, y + 11, color);
}

inline void draw(Arduino_GFX& g, uint32_t elapsed, int offsetX = 0, int offsetY = 0) {
    static const uint16_t palette[10] = {
        0x07FF, 0xB81F, 0x055F, 0xF828, 0xFE60,
        0xFAD7, 0x07FF, 0xAEDF, 0x07EF, 0x647F
    };
    const uint8_t expression = (elapsed / 4200) % 10;
    const uint16_t color = palette[expression];
    const int x = offsetX;
    const int y = offsetY + int(sinf(elapsed / 850.0f) * 2);
    const int gaze = int(sinf(elapsed / 1400.0f) * 2);
    const bool blink = elapsed % 3100 > 2940;

    // Sombra, fones e casco prateado, com profundidade e reflexos.
    g.fillEllipse(x + 88, offsetY + 129, 53, 4, 0x0821);
    g.fillRoundRect(x + 12, y + 52, 19, 42, 8, 0x52CB);
    g.fillRoundRect(x + 145, y + 52, 19, 42, 8, 0x52CB);
    g.fillRoundRect(x + 14, y + 55, 8, 34, 4, 0xADB5);
    g.fillRoundRect(x + 154, y + 55, 8, 34, 4, 0xADB5);
    g.fillRoundRect(x + 24, y + 59, 5, 25, 2, dim(color));
    g.fillRoundRect(x + 147, y + 59, 5, 25, 2, dim(color));
    g.fillRoundRect(x + 26, y + 17, 124, 108, 37, 0x6B6D);
    g.fillRoundRect(x + 26, y + 13, 124, 108, 37, 0xBDF7);
    g.fillRoundRect(x + 30, y + 16, 116, 99, 33, 0xE71C);
    g.fillRoundRect(x + 34, y + 29, 108, 85, 29, 0x5B4F);
    g.fillRoundRect(x + 37, y + 32, 102, 79, 26, 0x0823);
    g.fillRoundRect(x + 45, y + 37, 86, 21, 10, 0x1025);
    g.drawFastHLine(x + 66, y + 22, 44, 0xFFFF);

    const int left = x + 66 + gaze, right = x + 110 + gaze;
    const int eyeY = y + 67;
    auto eye = [&](int ex, bool closed) {
        if (closed) {
            g.fillRoundRect(ex - 8, eyeY - 1, 16, 3, 1, color);
        } else {
            g.fillEllipse(ex, eyeY, 8, 11, dim(color));
            g.fillEllipse(ex, eyeY, 5, 8, color);
            g.fillEllipse(ex - 1, eyeY - 3, 2, 3, 0xEFFF);
        }
    };

    if (expression == 5 && !blink) {
        heart(g, left, eyeY, color); heart(g, right, eyeY, color);
    } else if (expression == 8 && !blink) {
        curve(g, left - 8, eyeY, 16, -5, color);
        curve(g, right - 8, eyeY, 16, -5, color);
    } else {
        eye(left, blink || expression == 7 || expression == 1);
        eye(right, blink || expression == 7);
    }
    if (expression == 2) {
        curve(g, left - 10, eyeY - 17, 18, 5, color);
        curve(g, right - 8, eyeY - 17, 18, 5, color);
    } else if (expression == 3) {
        g.fillTriangle(left - 10, eyeY - 17, left + 9, eyeY - 9, left + 9, eyeY - 13, color);
        g.fillTriangle(right + 10, eyeY - 17, right - 9, eyeY - 9, right - 9, eyeY - 13, color);
    }
    const int mouthX = x + 77 + gaze, mouthY = y + 91;
    if (expression == 6) {
        g.fillEllipse(x + 88 + gaze, mouthY, 6, 8, dim(color));
        g.drawEllipse(x + 88 + gaze, mouthY, 4, 6, color);
        g.drawEllipse(x + 88 + gaze, mouthY, 3, 5, color);
    } else if (expression == 4 || expression == 7) {
        g.fillRoundRect(mouthX + 3, mouthY, 16, 3, 1, color);
    } else {
        const bool sad = expression == 2 || expression == 3 || expression == 9;
        curve(g, mouthX, mouthY + (sad ? 5 : -2), 22, sad ? -6 : 7, color);
    }
}
}
