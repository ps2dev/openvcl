#include <kernel.h>
#include <sio.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>

#include "../../../examples/pdk/src/dma.h"
#include "../../../examples/pdk/src/vu1.h"

#define INPUT_QW 0
#define OUTPUT_QW 32
#define CASE_COUNT 4
#define INPUT_STORAGE_QW (CASE_COUNT + 1)
#define VU1_DATA ((volatile u32 *)0x1100c000)

extern u32 vsmDifferentialCandidate_CodeStart;
extern u32 vsmDifferentialCandidate_CodeEnd;
extern u32 vsmDifferentialBaseline_CodeStart;
extern u32 vsmDifferentialBaseline_CodeEnd;

static float input[INPUT_STORAGE_QW][4] __attribute__((aligned(16))) = {
    {  2.0f,  4.0f,   6.0f, 2.0f },
    {  8.0f,  4.0f,   2.0f, 4.0f },
    { -8.0f, 16.0f, -24.0f, 8.0f },
    { 16.0f, -8.0f,   4.0f, 4.0f },
    { 32.0f, 64.0f,  96.0f, 8.0f },
};

static u32 output_seed[CASE_COUNT][4] __attribute__((aligned(16)));
static u32 candidate_output[CASE_COUNT][4] __attribute__((aligned(16)));
static u32 baseline_output[CASE_COUNT][4] __attribute__((aligned(16)));

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

static void reset_data_memory(void)
{
    pdkVu1ListBegin();
    /*
     * The current pipelined form prefetches the next VU-memory qword before
     * its final branch resolves. Keep that speculative qword deterministic;
     * it is never consumed or stored by the source-visible four iterations.
     */
    pdkVu1ListData(INPUT_QW, input, INPUT_STORAGE_QW);
    pdkVu1ListData(OUTPUT_QW, output_seed, CASE_COUNT);
    pdkVu1ListEnd(-1);
    pdkDmaWait(1);
}

static void run_program(void *start, void *end, u32 output[CASE_COUNT][4])
{
    int word;

    reset_data_memory();
    pdkVu1UploadProg(0, start, end);
    pdkDmaWait(1);

    pdkVu1ListBegin();
    pdkVu1ListEnd(0);
    wait_vu1();

    for (word = 0; word < CASE_COUNT * 4; ++word)
        ((u32 *)output)[word] = VU1_DATA[OUTPUT_QW * 4 + word];
}

static int check_expected(const u32 output[CASE_COUNT][4])
{
    static const float expected[CASE_COUNT][3] = {
        {  1.0f,  2.0f,  3.0f },
        {  2.0f,  1.0f,  0.5f },
        { -1.0f,  2.0f, -3.0f },
        {  4.0f, -2.0f,  1.0f },
    };
    int row;
    int lane;

    for (row = 0; row < CASE_COUNT; ++row) {
        for (lane = 0; lane < 3; ++lane) {
            const u32 wanted = float_bits(expected[row][lane]);
            if (output[row][lane] != wanted) {
                printf("EXPECTED mismatch row=%d lane=%d got=%08x wanted=%08x\n",
                       row, lane, output[row][lane], wanted);
                return 0;
            }
        }

        if (output[row][3] != output_seed[row][3]) {
            printf("EXPECTED W clobber row=%d got=%08x wanted=%08x\n",
                   row, output[row][3], output_seed[row][3]);
            return 0;
        }
    }

    return 1;
}

int main(void)
{
    int row;
    int lane;

    for (row = 0; row < CASE_COUNT; ++row) {
        for (lane = 0; lane < 4; ++lane)
            output_seed[row][lane] = 0x5a000000u | ((u32)row << 8) | (u32)lane;
    }

    run_program(&vsmDifferentialBaseline_CodeStart,
                &vsmDifferentialBaseline_CodeEnd,
                baseline_output);
    run_program(&vsmDifferentialCandidate_CodeStart,
                &vsmDifferentialCandidate_CodeEnd,
                candidate_output);

    if (memcmp(candidate_output, baseline_output, sizeof(candidate_output)) != 0) {
        sio_puts("VU1_DIFFERENTIAL_FAIL candidate differs from baseline\n");
        printf("VU1_DIFFERENTIAL_FAIL candidate differs from baseline\n");
        for (row = 0; row < CASE_COUNT; ++row) {
            printf("row %d candidate=%08x %08x %08x %08x baseline=%08x %08x %08x %08x\n",
                   row,
                   candidate_output[row][0], candidate_output[row][1],
                   candidate_output[row][2], candidate_output[row][3],
                   baseline_output[row][0], baseline_output[row][1],
                   baseline_output[row][2], baseline_output[row][3]);
        }
        return 1;
    }

    if (!check_expected(candidate_output)) {
        sio_puts("VU1_DIFFERENTIAL_FAIL shared output is not the expected result\n");
        printf("VU1_DIFFERENTIAL_FAIL shared output is not the expected result\n");
        return 2;
    }

    sio_puts("VU1_DIFFERENTIAL_PASS\n");
    printf("VU1_DIFFERENTIAL_PASS rows=%d\n", CASE_COUNT);
    return 0;
}
