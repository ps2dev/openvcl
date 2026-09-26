#include <kernel.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define MEMORY_QW 320
#define OUTPUT_QW 224
#define OUTPUT_COUNT 12
#define FINAL_STATE_QW 300
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

static void seed_memory(void)
{
    int q;
    int lane;

    for (q = 0; q < MEMORY_QW; ++q)
        for (lane = 0; lane < 4; ++lane)
            memory_seed[q][lane] = 0x5a000000u | ((u32)q << 4) | (u32)lane;

    /* 0x3f800000 encodes compressed first=0, second=0 for the indexed loop. */
    memory_seed[16][3] = 0x3f800000u;
    memory_seed[17][3] = 0x3f800000u;

    memory_seed[64][0] = float_bits(0.25f);
    memory_seed[64][1] = float_bits(0.50f);
    memory_seed[64][2] = float_bits(0.75f);
    memory_seed[64][3] = float_bits(1.00f);

    memory_seed[66][0] = float_bits(0.20f);
    memory_seed[66][1] = float_bits(0.30f);
    memory_seed[66][2] = float_bits(0.40f);
    memory_seed[66][3] = float_bits(1.00f);

    memory_seed[160][0] = float_bits(32.0f);
    memory_seed[160][1] = float_bits(64.0f);
    memory_seed[160][2] = float_bits(96.0f);
    memory_seed[160][3] = float_bits(128.0f);

    for (q = OUTPUT_QW; q < OUTPUT_QW + OUTPUT_COUNT; ++q)
        for (lane = 0; lane < 4; ++lane)
            memory_seed[q][lane] = 0x6b000000u | ((u32)q << 4) | (u32)lane;

    memory_seed[FINAL_STATE_QW][0] = 0xdead0001u;
    memory_seed[FINAL_STATE_QW][1] = 0xdead0002u;
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

    result->final_index = VU1_DATA[FINAL_STATE_QW * 4 + 0];
    result->final_output = VU1_DATA[FINAL_STATE_QW * 4 + 1];
}

int main(void)
{
    int row;

    seed_memory();
    run_program(&vsmGenericKernelRewriteBaseline_CodeStart,
                &vsmGenericKernelRewriteBaseline_CodeEnd,
                &baseline_result);
    run_program(&vsmGenericKernelRewriteCandidate_CodeStart,
                &vsmGenericKernelRewriteCandidate_CodeEnd,
                &candidate_result);

    if (memcmp(&candidate_result, &baseline_result, sizeof(candidate_result)) != 0) {
        printf("VU1_DIFFERENTIAL_FAIL generic-kernel direct refit differs from baseline\n");
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
        return 1;
    }

    if (candidate_result.final_index != 18u || candidate_result.final_output != 236u) {
        printf("VU1_DIFFERENTIAL_FAIL final state index=%u output=%u wanted=18,236\n",
               candidate_result.final_index, candidate_result.final_output);
        return 2;
    }

    printf("VU1_DIFFERENTIAL_PASS generic-kernel-direct rows=%d index=%u output=%u\n",
           OUTPUT_COUNT, candidate_result.final_index, candidate_result.final_output);
    return 0;
}
