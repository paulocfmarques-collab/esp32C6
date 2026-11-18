#pragma once
#include <stdint.h>
#include <math.h>

// FFT radix-2 in-place. size deve ser potencia de dois.
inline void audioFft(float* real, float* imag, uint16_t size) {
    for (uint16_t i = 1, j = 0; i < size; ++i) {
        uint16_t bit = size >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float swap = real[i]; real[i] = real[j]; real[j] = swap;
            swap = imag[i]; imag[i] = imag[j]; imag[j] = swap;
        }
    }
    for (uint16_t length = 2; length <= size; length <<= 1) {
        const float angle = -2.0f * 3.14159265358979323846f / length;
        const float stepR = cosf(angle), stepI = sinf(angle);
        for (uint16_t start = 0; start < size; start += length) {
            float wr = 1, wi = 0;
            for (uint16_t j = 0; j < length / 2; ++j) {
                const uint16_t a = start + j, b = a + length / 2;
                const float vr = real[b] * wr - imag[b] * wi;
                const float vi = real[b] * wi + imag[b] * wr;
                const float ar = real[a], ai = imag[a];
                real[a] = ar + vr; imag[a] = ai + vi;
                real[b] = ar - vr; imag[b] = ai - vi;
                const float nextR = wr * stepR - wi * stepI;
                wi = wr * stepI + wi * stepR; wr = nextR;
            }
        }
    }
}
