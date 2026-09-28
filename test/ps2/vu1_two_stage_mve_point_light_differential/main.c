#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 320
#define MEMORY_CHUNK_QW 160
#define INPUT_QW 32
#define OUTPUT_QW 160
#define FINAL_STATE_QW 300
#define CONTROL_QW 8
#define MAX_ITERATIONS 8
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmTwoStageMvePointLightCandidate_CodeStart;
extern u32 vsmTwoStageMvePointLightCandidate_CodeEnd;
extern u32 vsmTwoStageMvePointLightBaseline_CodeStart;
extern u32 vsmTwoStageMvePointLightBaseline_CodeEnd;

struct result {
    u32 output[MAX_ITERATIONS][4];
    u32 final_input;
    u32 final_output;
};

struct test_case {
    int iterations;
    int variant;
};

static const struct test_case test_cases[] = {
    { 1, 0 },
    { 2, 1 },
    { 3, 2 },
    { 4, 3 },
    { 5, 4 },
    { 6, 5 },
    { 7, 6 },
    { 8, 7 },
};

static u32 memory_seed[MEMORY_QW][4] __attribute__((aligned(16)));
static struct result candidate_result;
static struct result baseline_result;

static u32 float_bits(float value)
{
    union {
        float f;
        u32 u;
    } bits;
    bits.f = value;
    return bits.u;
}

static void set_vec(int qw, float x, float y, float z, float w)
{
    memory_seed[qw][0] = float_bits(x);
    memory_seed[qw][1] = float_bits(y);
    memory_seed[qw][2] = float_bits(z);
    memory_seed[qw][3] = float_bits(w);
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
    int iteration;
    const float v = (float)test->variant * 0.0625f;

    for (q = 0; q < MEMORY_QW; ++q)
        for (lane = 0; lane < 4; ++lane)
            memory_seed[q][lane] = 0x51000000u | ((u32)q << 4) | (u32)lane;

    memory_seed[CONTROL_QW][0] = INPUT_QW + (u32)test->iterations * 3u;

    set_vec(16, 0.10f + v, 0.12f, 0.14f, 0.0f);
    set_vec(17, 0.80f, 0.70f + v, 0.60f, 0.0f);
    set_vec(18, 1.00f, 1.00f, 1.00f + v, 0.0f);
    set_vec(19, 0.35f, 0.30f, 0.25f + v, 0.0f);
    set_vec(20, 0.90f, 0.75f, 0.55f + v, 0.0f);
    set_vec(21, 0.08f + v * 0.25f, 0.04f, 0.015f, 0.0f);
    set_vec(22, 3.0f + v, 4.0f - v, 5.0f + v * 0.5f, 1.0f);

    for (iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        const int input = INPUT_QW + iteration * 3;
        const int output = OUTPUT_QW + iteration * 3;
        const float t = (float)iteration * 0.17f + v;

        set_vec(input,
                0.40f + t,
                0.65f - t * 0.20f,
                1.10f + t * 0.30f,
                1.0f);
        set_vec(input + 1,
                0.20f + t * 0.05f,
                0.45f + t * 0.03f,
                0.85f - t * 0.04f,
                0.0f);
        set_vec(output + 1,
                0.25f + t * 0.10f,
                0.50f + t * 0.08f,
                0.75f + t * 0.06f,
                1.0f);
    }

    memory_seed[FINAL_STATE_QW][0] = 0xdead0001u;
    memory_seed[FINAL_STATE_QW][1] = 0xdead0002u;
}

static void run_program(void *start, void *end, struct result *result)
{
    int iteration;
    int lane;

    pdkVu1ListBegin();
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

    for (iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        const int output = OUTPUT_QW + iteration * 3 + 1;
        for (lane = 0; lane < 4; ++lane)
            result->output[iteration][lane] = VU1_DATA[output * 4 + lane];
    }
    result->final_input = VU1_DATA[FINAL_STATE_QW * 4 + 0];
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
        const u32 expected_input = INPUT_QW + (u32)test->iterations * 3u;
        const u32 expected_output = OUTPUT_QW + (u32)test->iterations * 3u;

        seed_memory(test);
        if ((test_index & 1) == 0) {
            run_program(&vsmTwoStageMvePointLightCandidate_CodeStart,
                        &vsmTwoStageMvePointLightCandidate_CodeEnd,
                        &candidate_result);
            run_program(&vsmTwoStageMvePointLightBaseline_CodeStart,
                        &vsmTwoStageMvePointLightBaseline_CodeEnd,
                        &baseline_result);
        } else {
            run_program(&vsmTwoStageMvePointLightBaseline_CodeStart,
                        &vsmTwoStageMvePointLightBaseline_CodeEnd,
                        &baseline_result);
            run_program(&vsmTwoStageMvePointLightCandidate_CodeStart,
                        &vsmTwoStageMvePointLightCandidate_CodeEnd,
                        &candidate_result);
        }

        if (memcmp(&candidate_result, &baseline_result, sizeof(candidate_result)) != 0) {
            printf("VU1_DIFFERENTIAL_DETAIL two-stage-mve-point-light case=%d iterations=%d variant=%d differs from baseline\n",
                   test_index, test->iterations, test->variant);
            for (row = 0; row < MAX_ITERATIONS; ++row) {
                printf("row %d candidate=%08x %08x %08x %08x baseline=%08x %08x %08x %08x\n",
                       row,
                       candidate_result.output[row][0], candidate_result.output[row][1],
                       candidate_result.output[row][2], candidate_result.output[row][3],
                       baseline_result.output[row][0], baseline_result.output[row][1],
                       baseline_result.output[row][2], baseline_result.output[row][3]);
            }
            printf("state candidate input=%08x output=%08x baseline input=%08x output=%08x\n",
                   candidate_result.final_input, candidate_result.final_output,
                   baseline_result.final_input, baseline_result.final_output);
            printf("VU1_DIFFERENTIAL_FAIL two-stage-mve-point-light case=%d iterations=%d\n",
                   test_index, test->iterations);
            return 1;
        }

        if (candidate_result.final_input != expected_input
            || candidate_result.final_output != expected_output) {
            printf("VU1_DIFFERENTIAL_FAIL two-stage-mve-point-light case=%d iterations=%d final state input=%u output=%u wanted=%u,%u\n",
                   test_index, test->iterations,
                   candidate_result.final_input, candidate_result.final_output,
                   expected_input, expected_output);
            return 2;
        }
    }

    printf("VU1_DIFFERENTIAL_PASS two-stage-mve-point-light cases=%d max-iterations=%d\n",
           (int)(sizeof(test_cases) / sizeof(test_cases[0])), MAX_ITERATIONS);
    return 0;
}
