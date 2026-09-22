#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 96
#define INPUT_QW 0
#define AUX_QW 8
#define OUTPUT_QW 32
#define OUTPUT_COUNT 8
#define ITERATION_COUNT 4
#define FINAL_POINTER_QW 80
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmAliasInductionCandidate_CodeStart;
extern u32 vsmAliasInductionCandidate_CodeEnd;
extern u32 vsmAliasInductionBaseline_CodeStart;
extern u32 vsmAliasInductionBaseline_CodeEnd;

struct result {
    u32 output[OUTPUT_COUNT][4];
    u32 final_pointer;
};

static float memory_seed[MEMORY_QW][4] __attribute__((aligned(16)));
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

    for (i = 0; i < ITERATION_COUNT; ++i) {
        memory_seed[INPUT_QW + i][0] = 1.0f + (float)i;
        memory_seed[INPUT_QW + i][1] = 2.0f + (float)i * 0.5f;
        memory_seed[INPUT_QW + i][2] = 3.0f + (float)i * 0.25f;
        memory_seed[INPUT_QW + i][3] = 1.0f;

        memory_seed[AUX_QW + i][0] = 10.0f + (float)i;
        memory_seed[AUX_QW + i][1] = 20.0f + (float)i;
        memory_seed[AUX_QW + i][2] = 30.0f + (float)i;
        memory_seed[AUX_QW + i][3] = 1.0f;
    }

    for (i = 0; i < OUTPUT_COUNT; ++i) {
        memory_seed[OUTPUT_QW + i][0] = -100.0f - (float)i;
        memory_seed[OUTPUT_QW + i][1] = -200.0f - (float)i;
        memory_seed[OUTPUT_QW + i][2] = -300.0f - (float)i;
        memory_seed[OUTPUT_QW + i][3] = 1000.0f + (float)i;
    }

    memory_seed[64][0] = 2.0f;
    memory_seed[64][1] = 4.0f;
    memory_seed[64][2] = 8.0f;
    memory_seed[64][3] = 1.0f;
}

static void run_program(void *start, void *end, struct result *result)
{
    int row;
    int lane;

    pdkVu1ListBegin();
    pdkVu1ListData(0, memory_seed, MEMORY_QW);
    pdkVu1ListEnd(-1);
    pdkDmaWait(1);

    pdkVu1UploadProg(0, start, end);
    pdkDmaWait(1);
    pdkVu1ListBegin();
    pdkVu1ListEnd(0);
    wait_vu1();

    for (row = 0; row < OUTPUT_COUNT; ++row)
        for (lane = 0; lane < 4; ++lane)
            result->output[row][lane] = VU1_DATA[(OUTPUT_QW + row) * 4 + lane];

    result->final_pointer = VU1_DATA[FINAL_POINTER_QW * 4];
}

int main(void)
{
    int row;

    seed_memory();
    run_program(&vsmAliasInductionBaseline_CodeStart,
                &vsmAliasInductionBaseline_CodeEnd,
                &baseline_result);
    run_program(&vsmAliasInductionCandidate_CodeStart,
                &vsmAliasInductionCandidate_CodeEnd,
                &candidate_result);

    if (memcmp(&candidate_result, &baseline_result, sizeof(candidate_result)) != 0) {
        printf("VU1_DIFFERENTIAL_FAIL alias-induction candidate differs from baseline\n");
        for (row = 0; row < OUTPUT_COUNT; ++row) {
            printf("row %d candidate=%08x %08x %08x %08x baseline=%08x %08x %08x %08x\n",
                   row,
                   candidate_result.output[row][0], candidate_result.output[row][1],
                   candidate_result.output[row][2], candidate_result.output[row][3],
                   baseline_result.output[row][0], baseline_result.output[row][1],
                   baseline_result.output[row][2], baseline_result.output[row][3]);
        }
        printf("pointer candidate=%08x baseline=%08x\n",
               candidate_result.final_pointer, baseline_result.final_pointer);
        return 1;
    }

    if (candidate_result.final_pointer != OUTPUT_QW + OUTPUT_COUNT) {
        printf("VU1_DIFFERENTIAL_FAIL alias-induction pointer=%08x wanted=%08x\n",
               candidate_result.final_pointer, OUTPUT_QW + OUTPUT_COUNT);
        return 2;
    }

    printf("VU1_DIFFERENTIAL_PASS alias-induction rows=%d pointer=%u\n",
           OUTPUT_COUNT, candidate_result.final_pointer);
    return 0;
}
