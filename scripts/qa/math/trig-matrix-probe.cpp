// GeneralsX @build Codex 09/10/2026 Compare production math with local hexadecimal inputs.
// Diagnostic only: uses actual production math headers; no engine or gameplay edits.
#include "matrix3d.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

static float FromBits(uint32_t bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static void Emit(float value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    std::printf("%08x ", static_cast<unsigned int>(bits));
}

int main(int argc, char** argv)
{
    // One line: 12 row-major matrix floats, Z/X/Y rotation angles, direction X/Y.
    // All input and output values are raw IEEE-754 hexadecimal bits.
    uint32_t bits[17];
    const bool topple = argc == 2 && std::strcmp(argv[1], "--topple") == 0;
    while (std::scanf("%x", &bits[0]) == 1)
    {
        for (int i = 1; i < 17; ++i)
            if (std::scanf("%x", &bits[i]) != 1)
                return 2;
        Matrix3D matrix;
        for (int i = 0; i < 12; ++i)
            matrix[i / 4][i % 4] = FromBits(bits[i]);
        // --topple: matrix12, delta, actual clipped curvel, dirX, dirY, count (hex integer).
        const float dirX = FromBits(bits[topple ? 14 : 15]);
        const float dirY = FromBits(bits[topple ? 15 : 16]);
        const float z = topple && bits[16] == 0 ? 0.0f : FromBits(bits[12]);
        const float x = topple ? -FromBits(bits[13]) * dirY : FromBits(bits[13]);
        const float y = topple ? FromBits(bits[13]) * dirX : FromBits(bits[14]);
        Emit(std::atan2(dirY, dirX));
        double (*volatile atan2Double)(double, double) = static_cast<double (*)(double, double)>(std::atan2);
        Emit(static_cast<float>(atan2Double(static_cast<double>(dirY), static_cast<double>(dirX))));
        for (float angle : {z, x, y})
        {
            Emit(WWMath::Sin(angle));
            Emit(WWMath::Cos(angle));
        }
        if (!topple || bits[16] > 0)
            matrix.In_Place_Pre_Rotate_Z(z);
        matrix.In_Place_Pre_Rotate_X(x);
        matrix.In_Place_Pre_Rotate_Y(y);
        for (int i = 0; i < 12; ++i)
            Emit(matrix[i / 4][i % 4]);
        std::putchar('\n');
    }
    return 0;
}
