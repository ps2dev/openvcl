#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 320
#define MEMORY_CHUNK_QW 160
#define OUTPUT_QW 224
#define MAX_ITERATIONS 8
#define OUTPUT_COUNT (MAX_ITERATIONS * 6)
#define FINAL_STATE_QW 300
#define CONTROL_QW 301
#define MATRIX_QW 304
#define VU1_DATA ((volatile u32 *)0x1100c000)

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
    u32 packed_indices[MAX_ITERATIONS];
};

static const struct test_case test_cases[] = {
    { 1, { 0x3f800000u, 0, 0, 0 } },
    { 2, { 0x3f800201u, 0x3f800403u, 0, 0 } },
    { 3, { 0x3f800105u, 0x3f800602u, 0x3f800307u, 0 } },
    { 4, { 0x3f800908u, 0x3f800b0au, 0x3f800d0cu, 0x3f800f0eu } },
    { 6, { 0x3f80000fu, 0x3f80010eu, 0x3f80020du, 0x3f80030cu,
           0x3f80040bu, 0x3f80050au, 0, 0 } },
    { 8, { 0x3f800f01u, 0x3f800e02u, 0x3f800d03u, 0x3f800c04u,
           0x3f800b05u, 0x3f800a06u, 0x3f800907u, 0x3f800008u } },
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

    for (q = 0; q < MEMORY_QW; ++q)
        for (lane = 0; lane < 4; ++lane)
            memory_seed[q][lane] = 0x5a000000u | ((u32)q << 4) | (u32)lane;

    for (q = 0; q < test->iterations; ++q)
        memory_seed[16 + q][3] = test->packed_indices[q];

    for (vertex = 0; vertex < 16; ++vertex) {
        const int input = 64 + vertex * 3;
        const float bias = (float)vertex * 0.125f;
        memory_seed[input][0] = float_bits(0.25f + bias);
        memory_seed[input][1] = float_bits(0.50f + bias);
        memory_seed[input][2] = float_bits(0.75f + bias);
        memory_seed[input][3] = float_bits(1.00f);
        memory_seed[input + 2][0] = float_bits(0.20f + bias);
        memory_seed[input + 2][1] = float_bits(0.30f + bias);
        memory_seed[input + 2][2] = float_bits(0.40f + bias);
        memory_seed[input + 2][3] = float_bits(1.00f);

        memory_seed[160 + vertex][0] = float_bits(32.0f + vertex);
        memory_seed[160 + vertex][1] = float_bits(64.0f + vertex);
        memory_seed[160 + vertex][2] = float_bits(96.0f + vertex);
        memory_seed[160 + vertex][3] = float_bits(128.0f);
    }

    for (q = OUTPUT_QW; q < OUTPUT_QW + OUTPUT_COUNT; ++q)
        for (lane = 0; lane < 4; ++lane)
            memory_seed[q][lane] = 0x6b000000u | ((u32)q << 4) | (u32)lane;

    memory_seed[FINAL_STATE_QW][0] = 0xdead0001u;
    memory_seed[FINAL_STATE_QW][1] = 0xdead0002u;
    memory_seed[CONTROL_QW][0] = 16u + (u32)test->iterations;

    /* Row-major identity transform loaded by the VU setup code. */
    memory_seed[MATRIX_QW + 0][0] = float_bits(1.0f);
    memory_seed[MATRIX_QW + 0][1] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 0][2] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 0][3] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 1][0] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 1][1] = float_bits(1.0f);
    memory_seed[MATRIX_QW + 1][2] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 1][3] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 2][0] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 2][1] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 2][2] = float_bits(1.0f);
    memory_seed[MATRIX_QW + 2][3] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 3][0] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 3][1] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 3][2] = float_bits(0.0f);
    memory_seed[MATRIX_QW + 3][3] = float_bits(1.0f);
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
            printf("VU1_DIFFERENTIAL_DETAIL two-stage-mve case=%d iterations=%d differs from baseline\n",
                   test_index, test->iterations);
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
            printf("VU1_DIFFERENTIAL_FAIL two-stage-mve case=%d iterations=%d differs from baseline\n",
                   test_index, test->iterations);
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
