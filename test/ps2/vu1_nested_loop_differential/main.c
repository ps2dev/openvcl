#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 96
#define OUTPUT_QW 32
#define OUTPUT_COUNT 2
#define FINAL_POINTER_QW 80
#define FINAL_LOAD_QW 81
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmNestedLoopCandidate_CodeStart;
extern u32 vsmNestedLoopCandidate_CodeEnd;
extern u32 vsmNestedLoopBaseline_CodeStart;
extern u32 vsmNestedLoopBaseline_CodeEnd;

struct result {
    u32 output[OUTPUT_COUNT];
    u32 final_pointer;
    u32 final_load;
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
    memset(memory_seed, 0, sizeof(memory_seed));
    memory_seed[1][0] = 0x111u;
    memory_seed[2][0] = 0x222u;
    memory_seed[3][0] = 0x333u;
    memory_seed[OUTPUT_QW][3] = 0xfeed0000u;
    memory_seed[OUTPUT_QW + 1][3] = 0xfeed0001u;
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
    result->final_load = VU1_DATA[FINAL_LOAD_QW * 4 + 3];
}

int main(void)
{
    seed_memory();
    run_program(&vsmNestedLoopBaseline_CodeStart,
                &vsmNestedLoopBaseline_CodeEnd,
                &baseline_result);

    seed_memory();
    run_program(&vsmNestedLoopCandidate_CodeStart,
                &vsmNestedLoopCandidate_CodeEnd,
                &candidate_result);

    if (memcmp(&candidate_result, &baseline_result, sizeof(candidate_result)) != 0) {
        printf("VU1_DIFFERENTIAL_FAIL nested-loop candidate differs from baseline\n");
        printf("out0 candidate=%08x baseline=%08x\n",
               candidate_result.output[0], baseline_result.output[0]);
        printf("out1 candidate=%08x baseline=%08x\n",
               candidate_result.output[1], baseline_result.output[1]);
        printf("pointer candidate=%08x baseline=%08x\n",
               candidate_result.final_pointer, baseline_result.final_pointer);
        printf("load candidate=%08x baseline=%08x\n",
               candidate_result.final_load, baseline_result.final_load);
        return 1;
    }

    if (candidate_result.output[0] != 0x111u
        || candidate_result.output[1] != 0x222u
        || candidate_result.final_pointer != 3u
        || candidate_result.final_load != 0x333u) {
        printf("VU1_DIFFERENTIAL_FAIL nested-loop unexpected output\n");
        return 2;
    }

    printf("VU1_DIFFERENTIAL_PASS nested-loop outputs=2 pointer=%u load=%03x\n",
           candidate_result.final_pointer, candidate_result.final_load);
    return 0;
}
