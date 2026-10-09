// GeneralsX @build Codex 09/10/2026 Compare production math with local hexadecimal inputs.
// Diagnostic only: uses actual production math headers; no engine or gameplay edits.
#include "matrix3d.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fenv.h>
#include <float.h>
#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

// GeneralsX @build Codex 09/10/2026 Optional control reproduces GameLogic's production floating-point setup.
static void ConfigureGameFPU()
{
#ifdef _WIN32
    _fpreset();
    unsigned int value = _statusfp();
    value = (value & ~_MCW_RC) | (_RC_NEAR & _MCW_RC);
    value = (value & ~_MCW_PC) | (_PC_24 & _MCW_PC);
    _controlfp(value, _MCW_PC | _MCW_RC);
    unsigned int control;
    _controlfp_s(&control, _MCW_EM, _MCW_EM);
#else
    fesetenv(FE_DFL_ENV);
    feclearexcept(FE_ALL_EXCEPT);
    fesetround(FE_TONEAREST);
#if defined(__i386__) || defined(__x86_64__)
    unsigned short control = 0;
    __asm__ __volatile__("fnstcw %0" : "=m" (control));
    control = static_cast<unsigned short>(control & ~0x0F00u);
    __asm__ __volatile__("fldcw %0" : : "m" (control));
#endif
#if defined(__SSE__) || defined(__x86_64__)
    unsigned int controlSSE = static_cast<unsigned int>(_mm_getcsr());
    controlSSE = (controlSSE & ~static_cast<unsigned int>(_MM_ROUND_MASK)) | static_cast<unsigned int>(_MM_ROUND_NEAREST);
    _mm_setcsr(controlSSE);
#endif
#endif
}

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
    bool topple = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--topple") == 0)
            topple = true;
        else if (std::strcmp(argv[i], "--game-fpu") == 0)
            ConfigureGameFPU();
        else
            return 3;
    }
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
