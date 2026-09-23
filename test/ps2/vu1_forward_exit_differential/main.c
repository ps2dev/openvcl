#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 96
#define OUTPUT_QW 32
#define OUTPUT_COUNT 4
#define FINAL_POINTER_QW 80
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmForwardExitCandidate_CodeStart;
extern u32 vsmForwardExitCandidate_CodeEnd;
extern u32 vsmForwardExitBaseline_CodeStart;
extern u32 vsmForwardExitBaseline_CodeEnd;

struct result {
    u32 output[OUTPUT_COUNT];
    u32 final_pointer;
};

static u32 memory_seed[MEMORY_QW][4] __attribute__((aligned(16)));
static struct result candidate_result;
static struct result baseline_result;

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

static void seed_memory(void)
{
    int i;
    memset(memory_seed, 0, sizeof(memory_seed));

    memory_seed[0][3] = 0x101u;
    memory_seed[1][3] = 0x202u;
    memory_seed[2][3] = 0x303u;
    memory_seed[3][3] = 0x020u;

    for (i = 0; i < OUTPUT_COUNT; ++i)
        memory_seed[OUTPUT_QW + i][3] = 0xfeed0000u + (u32)i;

    memory_seed[48][3] = 0x3f800000u;
    memory_seed[49][3] = 0x40000000u;
    memory_seed[50][3] = 0x40400000u;
}

static void run_program(void *start, void *end, struct result *result)
{
    int i;

    pdkVu1ListBegin();
    pdkVu1ListData(0, memory_seed, MEMORY_QW);
    pdkVu1ListEnd(-1);
    pdkDmaWait(1);

    pdkVu1UploadProg(0, start, end);
    pdkDmaWait(1);
    pdkVu1ListBegin();
    pdkVu1ListEnd(0);
    wait_vu1();

    for (i = 0; i < OUTPUT_COUNT; ++i)
        result->output[i] = VU1_DATA[(OUTPUT_QW + i) * 4 + 3];
    result->final_pointer = VU1_DATA[FINAL_POINTER_QW * 4 + 3];
}

int main(void)
{
    int i;

    seed_memory();
    run_program(&vsmForwardExitBaseline_CodeStart,
                &vsmForwardExitBaseline_CodeEnd,
                &baseline_result);
    run_program(&vsmForwardExitCandidate_CodeStart,
                &vsmForwardExitCandidate_CodeEnd,
                &candidate_result);

    if (memcmp(&candidate_result, &baseline_result, sizeof(candidate_result)) != 0) {
        printf("VU1_DIFFERENTIAL_FAIL forward-exit candidate differs from baseline\n");
        for (i = 0; i < OUTPUT_COUNT; ++i)
            printf("row %d candidate=%08x baseline=%08x\n",
                   i, candidate_result.output[i], baseline_result.output[i]);
        printf("pointer candidate=%08x baseline=%08x\n",
               candidate_result.final_pointer, baseline_result.final_pointer);
        return 1;
    }

    if (candidate_result.output[0] != 0x01u
        || candidate_result.output[1] != 0x02u
        || candidate_result.output[2] != 0x03u
        || candidate_result.output[3] != 0xfeed0003u
        || candidate_result.final_pointer != 3u) {
        printf("VU1_DIFFERENTIAL_FAIL forward-exit unexpected output\n");
        return 2;
    }

    printf("VU1_DIFFERENTIAL_PASS forward-exit rows=3 pointer=%u\n",
           candidate_result.final_pointer);
    return 0;
}
