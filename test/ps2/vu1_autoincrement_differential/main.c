#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 96
#define OUTPUT_QW 32
#define CASE_COUNT 4
#define FINAL_POINTER_QW 80
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmAutoIncrementCandidate_CodeStart;
extern u32 vsmAutoIncrementCandidate_CodeEnd;
extern u32 vsmAutoIncrementBaseline_CodeStart;
extern u32 vsmAutoIncrementBaseline_CodeEnd;

struct result {
    u32 output[CASE_COUNT][4];
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
    static const float normals[5][3] = {
        { 0.25f, 0.50f, 0.75f },
        { 0.80f, 0.10f, 0.20f },
        {-0.40f, 0.70f, 0.30f },
        { 0.60f,-0.20f, 0.90f },
        { 0.15f, 0.35f, 0.55f }
    };
    static const float totals[CASE_COUNT][3] = {
        {0.10f, 0.20f, 0.30f},
        {0.20f, 0.10f, 0.40f},
        {0.30f, 0.40f, 0.10f},
        {0.40f, 0.30f, 0.20f}
    };
    int i;

    memset(memory_seed, 0, sizeof(memory_seed));
    for (i = 0; i < 5; ++i) {
        memory_seed[i * 3 + 1][0] = normals[i][0];
        memory_seed[i * 3 + 1][1] = normals[i][1];
        memory_seed[i * 3 + 1][2] = normals[i][2];
    }
    for (i = 0; i < CASE_COUNT; ++i) {
        memory_seed[OUTPUT_QW + i][0] = totals[i][0];
        memory_seed[OUTPUT_QW + i][1] = totals[i][1];
        memory_seed[OUTPUT_QW + i][2] = totals[i][2];
        memory_seed[OUTPUT_QW + i][3] = 123.0f + (float)i;
    }

    memory_seed[64][0] = 0.30f; memory_seed[64][1] = 0.40f; memory_seed[64][2] = 0.50f;
    memory_seed[65][0] = 1.00f; memory_seed[65][1] = 1.00f; memory_seed[65][2] = 1.00f;
    memory_seed[66][0] = 0.70f; memory_seed[66][1] = 0.60f; memory_seed[66][2] = 0.50f;
    memory_seed[67][0] = 0.80f; memory_seed[67][1] = 0.90f; memory_seed[67][2] = 1.00f;
    memory_seed[68][0] = 0.20f; memory_seed[68][1] = 0.30f; memory_seed[68][2] = 0.40f;
    memory_seed[69][0] = 0.05f; memory_seed[69][1] = 0.07f; memory_seed[69][2] = 0.09f;
    memory_seed[70][0] = 0.11f; memory_seed[70][1] = 0.13f; memory_seed[70][2] = 0.17f;
    memory_seed[71][0] = 0.19f; memory_seed[71][1] = 0.23f; memory_seed[71][2] = 0.29f;
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

    for (row = 0; row < CASE_COUNT; ++row)
        for (lane = 0; lane < 4; ++lane)
            result->output[row][lane] = VU1_DATA[(OUTPUT_QW + row) * 4 + lane];

    result->final_pointer = VU1_DATA[FINAL_POINTER_QW * 4];
}

int main(void)
{
    int row;

    seed_memory();
    run_program(&vsmAutoIncrementBaseline_CodeStart,
                &vsmAutoIncrementBaseline_CodeEnd,
                &baseline_result);
    run_program(&vsmAutoIncrementCandidate_CodeStart,
                &vsmAutoIncrementCandidate_CodeEnd,
                &candidate_result);

    if (memcmp(&candidate_result, &baseline_result, sizeof(candidate_result)) != 0) {
        printf("VU1_DIFFERENTIAL_FAIL autoincrement candidate differs from baseline\n");
        for (row = 0; row < CASE_COUNT; ++row) {
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

    if (candidate_result.final_pointer != OUTPUT_QW + CASE_COUNT) {
        printf("VU1_DIFFERENTIAL_FAIL autoincrement pointer=%08x wanted=%08x\n",
               candidate_result.final_pointer, OUTPUT_QW + CASE_COUNT);
        return 2;
    }

    printf("VU1_DIFFERENTIAL_PASS autoincrement rows=%d pointer=%u\n",
           CASE_COUNT, candidate_result.final_pointer);
    return 0;
}
