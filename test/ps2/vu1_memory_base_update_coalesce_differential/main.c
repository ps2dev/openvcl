#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 48
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmMemoryBaseUpdateCoalesceCandidate_CodeStart;
extern u32 vsmMemoryBaseUpdateCoalesceCandidate_CodeEnd;
extern u32 vsmMemoryBaseUpdateCoalesceBaseline_CodeStart;
extern u32 vsmMemoryBaseUpdateCoalesceBaseline_CodeEnd;

static u32 memory_seed[MEMORY_QW][4] __attribute__((aligned(16)));

typedef struct Result {
    u32 first[4];
    u32 second[4];
    u32 final_pointer;
} Result;

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

static Result run_program(void *start, void *end)
{
    Result result;
    unsigned int lane;

    pdkVu1ListBegin();
    pdkVu1ListData(0, memory_seed, MEMORY_QW);
    pdkVu1ListEnd(-1);
    pdkDmaWait(1);

    pdkVu1UploadProg(0, start, end);
    pdkDmaWait(1);
    pdkVu1ListBegin();
    pdkVu1ListEnd(0);
    wait_vu1();

    for (lane = 0; lane < 4; ++lane) {
        result.first[lane] = VU1_DATA[35 * 4 + lane];
        result.second[lane] = VU1_DATA[36 * 4 + lane];
    }
    result.final_pointer = VU1_DATA[40 * 4 + 3];
    return result;
}

int main(void)
{
    Result baseline;
    Result candidate;

    memset(memory_seed, 0, sizeof(memory_seed));
    memory_seed[16][0] = 0x3f800000;
    memory_seed[16][1] = 0x40000000;
    memory_seed[16][2] = 0x40400000;
    memory_seed[16][3] = 0x40800000;
    memory_seed[17][0] = 0x40a00000;
    memory_seed[17][1] = 0x40c00000;
    memory_seed[17][2] = 0x40e00000;
    memory_seed[17][3] = 0x41000000;

    baseline = run_program(&vsmMemoryBaseUpdateCoalesceBaseline_CodeStart,
                           &vsmMemoryBaseUpdateCoalesceBaseline_CodeEnd);
    candidate = run_program(&vsmMemoryBaseUpdateCoalesceCandidate_CodeStart,
                            &vsmMemoryBaseUpdateCoalesceCandidate_CodeEnd);

    if (memcmp(&candidate, &baseline, sizeof(candidate)) != 0) {
        printf("VU1_DIFFERENTIAL_FAIL memory-base-update-coalesce mismatch\n");
        fflush(stdout);
        SleepThread();
        return 1;
    }
    if (candidate.first[0] != memory_seed[16][0]
        || candidate.second[0] != memory_seed[17][0]
        || candidate.final_pointer != 38) {
        printf("VU1_DIFFERENTIAL_FAIL memory-base-update-coalesce first=%08x second=%08x pointer=%u\n",
               candidate.first[0], candidate.second[0], candidate.final_pointer);
        fflush(stdout);
        SleepThread();
        return 2;
    }

    printf("VU1_DIFFERENTIAL_PASS memory-base-update-coalesce pointer=%u\n",
           candidate.final_pointer);
    return 0;
}
