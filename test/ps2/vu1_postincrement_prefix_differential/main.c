#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 96
#define INPUT_QW 16
#define OUTPUT_QW 32
#define OUTPUT_QW_COUNT 8
#define LIMIT_QW 66
#define FINAL_POINTER_QW 80
#define CASE_COUNT 3
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmPostIncrementPrefixCandidate_CodeStart;
extern u32 vsmPostIncrementPrefixCandidate_CodeEnd;
extern u32 vsmPostIncrementPrefixBaseline_CodeStart;
extern u32 vsmPostIncrementPrefixBaseline_CodeEnd;

struct result {
    u32 output[OUTPUT_QW_COUNT][4];
    u32 final_pointer;
};

static u32 memory_seed[MEMORY_QW][4] __attribute__((aligned(16)));
static struct result candidate_result;
static struct result baseline_result;

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

static void seed_memory(unsigned int iterations)
{
    unsigned int i;
    unsigned int lane;

    memset(memory_seed, 0, sizeof(memory_seed));

    for (i = 0; i < 4; ++i) {
        memory_seed[INPUT_QW + i][3] = float_bits(2.0f + (float)i * 1.25f);
    }

    for (i = 0; i < OUTPUT_QW_COUNT; ++i) {
        for (lane = 0; lane < 4; ++lane)
            memory_seed[OUTPUT_QW + i][lane] = 0x51000000u + i * 0x100u + lane;
    }

    memory_seed[64][3] = float_bits(2.0f);
    memory_seed[65][3] = float_bits(4.0f);
    memory_seed[LIMIT_QW][0] = INPUT_QW + iterations;
}

static void run_program(void *start, void *end, struct result *result)
{
    unsigned int row;
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

    for (row = 0; row < OUTPUT_QW_COUNT; ++row)
        for (lane = 0; lane < 4; ++lane)
            result->output[row][lane] = VU1_DATA[(OUTPUT_QW + row) * 4 + lane];

    result->final_pointer = VU1_DATA[FINAL_POINTER_QW * 4];
}

int main(void)
{
    static const unsigned int cases[CASE_COUNT] = {1, 2, 4};
    unsigned int c;
    unsigned int row;

    for (c = 0; c < CASE_COUNT; ++c) {
        const unsigned int iterations = cases[c];

        seed_memory(iterations);
        run_program(&vsmPostIncrementPrefixBaseline_CodeStart,
                    &vsmPostIncrementPrefixBaseline_CodeEnd,
                    &baseline_result);

        seed_memory(iterations);
        run_program(&vsmPostIncrementPrefixCandidate_CodeStart,
                    &vsmPostIncrementPrefixCandidate_CodeEnd,
                    &candidate_result);

        if (memcmp(&candidate_result, &baseline_result, sizeof(candidate_result)) != 0) {
            printf("VU1_DIFFERENTIAL_FAIL postincrement-prefix iterations=%u\n", iterations);
            for (row = 0; row < OUTPUT_QW_COUNT; ++row) {
                printf("row %u candidate=%08x %08x %08x %08x baseline=%08x %08x %08x %08x\n",
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

        if (candidate_result.final_pointer != INPUT_QW + iterations) {
            printf("VU1_DIFFERENTIAL_FAIL postincrement-prefix pointer=%u wanted=%u iterations=%u\n",
                   candidate_result.final_pointer, INPUT_QW + iterations, iterations);
            return 2;
        }
    }

    printf("VU1_DIFFERENTIAL_PASS postincrement-prefix cases=1,2,4\n");
    return 0;
}
