#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 320
#define MEMORY_CHUNK_QW 160
#define OUTPUT_QW 224
#define MAX_ITERATIONS 12
#define OUTPUT_COUNT (MAX_ITERATIONS * 6)
#define FINAL_STATE_QW 300
#define CONTROL_QW 301
#define MATRIX_QW 304
#define VU1_DATA ((volatile u32 *)0x1100c000)
#define PACK_INDEX(first, second) \
    (0x3f800000u | ((u32)(second) << 8) | (u32)(first))

extern u32 vsmGenericKernelRewriteCandidate_CodeStart;
extern u32 vsmGenericKernelRewriteCandidate_CodeEnd;
extern u32 vsmGenericKernelRewriteBaseline_CodeStart;
extern u32 vsmGenericKernelRewriteBaseline_CodeEnd;

struct result {
    u32 output[OUTPUT_COUNT][4];
    u32 final_index;
    u32 final_output;
};

static u32 memory_seed[MEMORY_QW][4] __attribute__((aligned(16)));
static struct result candidate_result;
static struct result baseline_result;

struct test_case {
    int iterations;
    int matrix_variant;
    int data_variant;
    u32 packed_indices[MAX_ITERATIONS];
};

static const struct test_case test_cases[] = {
    { 1, 0, 0, { PACK_INDEX(0, 0) } },
    { 2, 1, 1, { PACK_INDEX(1, 2), PACK_INDEX(3, 4) } },
    { 3, 2, 2, { PACK_INDEX(5, 1), PACK_INDEX(2, 6), PACK_INDEX(7, 3) } },
    { 4, 0, 3, { PACK_INDEX(8, 9), PACK_INDEX(10, 11),
                  PACK_INDEX(12, 13), PACK_INDEX(14, 15) } },
    { 5, 1, 4, { PACK_INDEX(15, 0), PACK_INDEX(14, 1), PACK_INDEX(13, 2),
                  PACK_INDEX(12, 3), PACK_INDEX(11, 4) } },
    { 6, 2, 5, { PACK_INDEX(0, 15), PACK_INDEX(1, 14), PACK_INDEX(2, 13),
                  PACK_INDEX(3, 12), PACK_INDEX(4, 11), PACK_INDEX(5, 10) } },
    { 7, 1, 6, { PACK_INDEX(2, 9), PACK_INDEX(4, 11), PACK_INDEX(6, 13),
                  PACK_INDEX(8, 15), PACK_INDEX(10, 1), PACK_INDEX(12, 3),
                  PACK_INDEX(14, 5) } },
    { 8, 0, 7, { PACK_INDEX(1, 15), PACK_INDEX(2, 14), PACK_INDEX(3, 13),
                  PACK_INDEX(4, 12), PACK_INDEX(5, 11), PACK_INDEX(6, 10),
                  PACK_INDEX(7, 9), PACK_INDEX(8, 0) } },
    { 9, 2, 8, { PACK_INDEX(0, 8), PACK_INDEX(1, 9), PACK_INDEX(2, 10),
                  PACK_INDEX(3, 11), PACK_INDEX(4, 12), PACK_INDEX(5, 13),
                  PACK_INDEX(6, 14), PACK_INDEX(7, 15), PACK_INDEX(8, 0) } },
    { 10, 1, 9, { PACK_INDEX(0, 3), PACK_INDEX(1, 8), PACK_INDEX(2, 13),
                   PACK_INDEX(3, 2), PACK_INDEX(4, 7), PACK_INDEX(5, 12),
                   PACK_INDEX(6, 1), PACK_INDEX(7, 6), PACK_INDEX(8, 11),
                   PACK_INDEX(9, 0) } },
    { 12, 2, 10, { PACK_INDEX(0, 15), PACK_INDEX(1, 14), PACK_INDEX(2, 13),
                    PACK_INDEX(3, 12), PACK_INDEX(4, 11), PACK_INDEX(5, 10),
                    PACK_INDEX(6, 9), PACK_INDEX(7, 8), PACK_INDEX(8, 7),
                    PACK_INDEX(9, 6), PACK_INDEX(10, 5), PACK_INDEX(11, 4) } },
};

static const float transform_matrices[3][4][4] = {
    {
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    },
    {
        { 1.5f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.75f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.25f, 0.0f },
        { 0.5f, -0.25f, 0.75f, 1.0f },
    },
    {
        { 1.0f, 0.20f, 0.0f, 0.0f },
        { -0.10f, 0.90f, 0.15f, 0.0f },
        { 0.05f, -0.20f, 1.10f, 0.0f },
        { 0.25f, 0.50f, -0.75f, 1.0f },
    },
};

static u32 float_bits(float value)
{
    union {
        float f;
        u32 u;
    } bits;
    bits.f = value;
    return bits.u;
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

static void seed_memory(const struct test_case *test)
{
    int q;
    int lane;
    int vertex;
    const float case_bias = (float)test->data_variant * 0.03125f;
    const float case_scale = 1.0f + (float)(test->data_variant & 3) * 0.0625f;

    for (q = 0; q < MEMORY_QW; ++q)
        for (lane = 0; lane < 4; ++lane)
            memory_seed[q][lane] = 0x5a000000u | ((u32)q << 4) | (u32)lane;

    for (q = 0; q < test->iterations; ++q)
        memory_seed[16 + q][3] = test->packed_indices[q];

    for (vertex = 0; vertex < 16; ++vertex) {
        const int input = 64 + vertex * 3;
        const float bias = (float)vertex * 0.125f + case_bias;
        memory_seed[input][0] = float_bits((0.25f + bias) * case_scale);
        memory_seed[input][1] = float_bits(0.50f + bias * 0.50f + case_bias);
        memory_seed[input][2] = float_bits(0.75f - bias * 0.25f + case_bias * 0.50f);
        memory_seed[input][3] = float_bits(1.00f);
        memory_seed[input + 2][0] = float_bits(0.20f + bias * 0.50f);
        memory_seed[input + 2][1] = float_bits(0.30f + bias * 0.25f + case_bias);
        memory_seed[input + 2][2] = float_bits(0.40f + bias * 0.75f);
        memory_seed[input + 2][3] = float_bits(1.00f);

        memory_seed[160 + vertex][0] = float_bits(32.0f + vertex + test->data_variant * 2.0f);
        memory_seed[160 + vertex][1] = float_bits(64.0f + vertex + test->data_variant * 1.5f);
        memory_seed[160 + vertex][2] = float_bits(96.0f + vertex + test->data_variant * 1.0f);
        memory_seed[160 + vertex][3] = float_bits(128.0f);
    }

    for (q = OUTPUT_QW; q < OUTPUT_QW + OUTPUT_COUNT; ++q)
        for (lane = 0; lane < 4; ++lane)
            memory_seed[q][lane] = 0x6b000000u | ((u32)q << 4) | (u32)lane;

    memory_seed[FINAL_STATE_QW][0] = 0xdead0001u;
    memory_seed[FINAL_STATE_QW][1] = 0xdead0002u;
    memory_seed[CONTROL_QW][0] = 16u + (u32)test->iterations;

    for (q = 0; q < 4; ++q)
        for (lane = 0; lane < 4; ++lane)
            memory_seed[MATRIX_QW + q][lane] =
                float_bits(transform_matrices[test->matrix_variant % 3][q][lane]);
}

static void run_program(void *start, void *end, struct result *result)
{
    int row;
    int lane;

    pdkVu1ListBegin();
    /* VIF UNPACK NUM is 8-bit; keep each transfer below 256 qwords. */
    pdkVu1ListData(0, memory_seed, MEMORY_CHUNK_QW);
    pdkVu1ListData(MEMORY_CHUNK_QW,
                   &memory_seed[MEMORY_CHUNK_QW],
                   MEMORY_QW - MEMORY_CHUNK_QW);
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

    result->final_index = VU1_DATA[FINAL_STATE_QW * 4 + 0];
    result->final_output = VU1_DATA[FINAL_STATE_QW * 4 + 1];
}

int main(void)
{
    int test_index;
    int row;

    for (test_index = 0;
         test_index < (int)(sizeof(test_cases) / sizeof(test_cases[0]));
         ++test_index) {
        const struct test_case *test = &test_cases[test_index];
        const u32 expected_index = 16u + (u32)test->iterations;
        const u32 expected_output = OUTPUT_QW + (u32)test->iterations * 6u;

        seed_memory(test);
        if ((test_index & 1) == 0) {
            run_program(&vsmGenericKernelRewriteCandidate_CodeStart,
                        &vsmGenericKernelRewriteCandidate_CodeEnd,
                        &candidate_result);
            run_program(&vsmGenericKernelRewriteBaseline_CodeStart,
                        &vsmGenericKernelRewriteBaseline_CodeEnd,
                        &baseline_result);
        } else {
            run_program(&vsmGenericKernelRewriteBaseline_CodeStart,
                        &vsmGenericKernelRewriteBaseline_CodeEnd,
                        &baseline_result);
            run_program(&vsmGenericKernelRewriteCandidate_CodeStart,
                        &vsmGenericKernelRewriteCandidate_CodeEnd,
                        &candidate_result);
        }

        if (memcmp(&candidate_result, &baseline_result, sizeof(candidate_result)) != 0) {
            printf("VU1_DIFFERENTIAL_DETAIL two-stage-mve case=%d iterations=%d matrix=%d data=%d differs from baseline\n",
                   test_index, test->iterations, test->matrix_variant, test->data_variant);
            for (row = 0; row < OUTPUT_COUNT; ++row) {
                printf("row %d candidate=%08x %08x %08x %08x baseline=%08x %08x %08x %08x\n",
                       row,
                       candidate_result.output[row][0], candidate_result.output[row][1],
                       candidate_result.output[row][2], candidate_result.output[row][3],
                       baseline_result.output[row][0], baseline_result.output[row][1],
                       baseline_result.output[row][2], baseline_result.output[row][3]);
            }
            printf("state candidate index=%08x output=%08x baseline index=%08x output=%08x\n",
                   candidate_result.final_index, candidate_result.final_output,
                   baseline_result.final_index, baseline_result.final_output);
            printf("VU1_DIFFERENTIAL_FAIL two-stage-mve case=%d iterations=%d matrix=%d data=%d differs from baseline\n",
                   test_index, test->iterations, test->matrix_variant, test->data_variant);
            return 1;
        }

        if (candidate_result.final_index != expected_index
            || candidate_result.final_output != expected_output) {
            printf("VU1_DIFFERENTIAL_FAIL case=%d iterations=%d final state index=%u output=%u wanted=%u,%u\n",
                   test_index, test->iterations,
                   candidate_result.final_index, candidate_result.final_output,
                   expected_index, expected_output);
            return 2;
        }
    }

    printf("VU1_DIFFERENTIAL_PASS two-stage-mve cases=%d max-iterations=%d\n",
           (int)(sizeof(test_cases) / sizeof(test_cases[0])), MAX_ITERATIONS);
    return 0;
}
