#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 80
#define STORE_QW 16
#define LOAD_QW 20
#define OUTPUT_QW 32
#define VALUE_QW 64
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmAffineAliasCandidate_CodeStart;
extern u32 vsmAffineAliasCandidate_CodeEnd;
extern u32 vsmAffineAliasBaseline_CodeStart;
extern u32 vsmAffineAliasBaseline_CodeEnd;

static u32 memory_seed[MEMORY_QW][4] __attribute__((aligned(16)));

static u32 float_bits(float value)
{
    u32 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void wait_vu1(void)
{
    pdkDmaWait(1);
    __asm__ __volatile__(
        "nop\n"
        "nop\n"
        "1:\n"
        "bc2t 1b\n"
        "nop\n"
        "nop\n");
}

static u32 run_program(void *start, void *end)
{
    pdkVu1ListBegin();
    pdkVu1ListData(0, memory_seed, MEMORY_QW);
    pdkVu1ListEnd(-1);
    pdkDmaWait(1);

    pdkVu1UploadProg(0, start, end);
    pdkDmaWait(1);
    pdkVu1ListBegin();
    pdkVu1ListEnd(0);
    wait_vu1();

    return VU1_DATA[OUTPUT_QW * 4 + 3];
}

int main(void)
{
    u32 baseline;
    u32 candidate;

    memset(memory_seed, 0, sizeof(memory_seed));
    memory_seed[STORE_QW][3] = float_bits(-1.0f);
    memory_seed[LOAD_QW][3] = float_bits(7.0f);
    memory_seed[VALUE_QW][3] = float_bits(42.0f);

    baseline = run_program(&vsmAffineAliasBaseline_CodeStart,
                           &vsmAffineAliasBaseline_CodeEnd);
    candidate = run_program(&vsmAffineAliasCandidate_CodeStart,
                            &vsmAffineAliasCandidate_CodeEnd);

    if (candidate != baseline) {
        printf("VU1_DIFFERENTIAL_FAIL affine-alias candidate=%08x baseline=%08x\n",
               candidate, baseline);
        return 1;
    }

    if (candidate != float_bits(7.0f)) {
        printf("VU1_DIFFERENTIAL_FAIL affine-alias value=%08x wanted=%08x\n",
               candidate, float_bits(7.0f));
        return 2;
    }

    printf("VU1_DIFFERENTIAL_PASS affine-alias value=%08x\n", candidate);
    return 0;
}
