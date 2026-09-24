#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 48
#define INPUT_QW 16
#define OUTPUT_QW 32
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmDeadIntegerSourceReuseCandidate_CodeStart;
extern u32 vsmDeadIntegerSourceReuseCandidate_CodeEnd;
extern u32 vsmDeadIntegerSourceReuseBaseline_CodeStart;
extern u32 vsmDeadIntegerSourceReuseBaseline_CodeEnd;

static u32 memory_seed[MEMORY_QW][4] __attribute__((aligned(16)));

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
    memory_seed[INPUT_QW][3] = 35;

    baseline = run_program(&vsmDeadIntegerSourceReuseBaseline_CodeStart,
                           &vsmDeadIntegerSourceReuseBaseline_CodeEnd);
    candidate = run_program(&vsmDeadIntegerSourceReuseCandidate_CodeStart,
                            &vsmDeadIntegerSourceReuseCandidate_CodeEnd);

    if (candidate != baseline) {
        printf("VU1_DIFFERENTIAL_FAIL dead-integer-source-reuse candidate=%08x baseline=%08x\n",
               candidate, baseline);
        return 1;
    }

    if (candidate != 42) {
        printf("VU1_DIFFERENTIAL_FAIL dead-integer-source-reuse value=%08x wanted=%08x\n",
               candidate, 42u);
        return 2;
    }

    printf("VU1_DIFFERENTIAL_PASS dead-integer-source-reuse value=%08x\n", candidate);
    return 0;
}
