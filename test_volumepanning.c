#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "lv2/core/lv2.h"

extern const LV2_Descriptor* lv2_descriptor(uint32_t index);

enum {
    PORT_IN          = 0,
    PORT_OUT_L       = 1,
    PORT_OUT_R       = 2,
    PORT_ENABLED     = 3,
    PORT_VOLUME      = 4,
    PORT_PAN         = 5,
    PORT_MUTE        = 6,
    PORT_MUTE_INVERT = 7,
};

#define NSAMPLES 64
#define EPSILON  1e-5f

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do {                                              \
    tests_run++;                                                           \
    if (cond) {                                                            \
        tests_passed++;                                                    \
    } else {                                                               \
        tests_failed++;                                                    \
        fprintf(stderr, "  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); \
    }                                                                      \
} while (0)

#define CHECK_NEAR(a, b, eps, msg) \
    CHECK(fabsf((float)(a) - (float)(b)) < (eps), msg)

/* ---- Fixture ---- */

typedef struct {
    LV2_Handle handle;
    float in[NSAMPLES];
    float out_l[NSAMPLES];
    float out_r[NSAMPLES];
    float pan;
    float volume;
    float mute;
    float enabled;
    float mute_invert;
} Fixture;

static const LV2_Descriptor* desc = NULL;

static Fixture* fx_create(void)
{
    Fixture* f = (Fixture*)calloc(1, sizeof(Fixture));
    f->handle = desc->instantiate(desc, 48000.0, "", NULL);
    if (!f->handle) { free(f); return NULL; }

    desc->connect_port(f->handle, PORT_IN,          f->in);
    desc->connect_port(f->handle, PORT_OUT_L,       f->out_l);
    desc->connect_port(f->handle, PORT_OUT_R,       f->out_r);
    desc->connect_port(f->handle, PORT_PAN,         &f->pan);
    desc->connect_port(f->handle, PORT_VOLUME,      &f->volume);
    desc->connect_port(f->handle, PORT_MUTE,        &f->mute);
    desc->connect_port(f->handle, PORT_ENABLED,     &f->enabled);
    desc->connect_port(f->handle, PORT_MUTE_INVERT, &f->mute_invert);

    /* Default state: 0 dB (unity gain), centre pan, active */
    f->pan         = 0.0f;
    f->volume      = 0.0f;
    f->mute        = 0.0f;
    f->enabled     = 1.0f;
    f->mute_invert = 0.0f;

    for (int i = 0; i < NSAMPLES; ++i)
        f->in[i] = 1.0f;

    desc->activate(f->handle);
    return f;
}

static void fx_run(Fixture* f)
{
    desc->run(f->handle, NSAMPLES);
}

static void fx_destroy(Fixture* f)
{
    desc->deactivate(f->handle);
    desc->cleanup(f->handle);
    free(f);
}

/* Check that every sample in buf equals expected, report as one result. */
static int all_near(const float* buf, float expected, float eps)
{
    for (int i = 0; i < NSAMPLES; ++i)
        if (fabsf(buf[i] - expected) >= eps)
            return 0;
    return 1;
}

/* ---- Test functions ---- */

static void test_descriptor(void)
{
    printf("test_descriptor\n");
    CHECK(desc != NULL,
          "lv2_descriptor(0) is non-NULL");
    CHECK(strcmp(desc->URI,
                 "http://fprice.pricemail.ca/plugins/volumepanning") == 0,
          "plugin URI matches");
    CHECK(lv2_descriptor(1) == NULL,
          "lv2_descriptor(1) returns NULL (only one plugin)");
}

static void test_instantiate_cleanup(void)
{
    printf("test_instantiate_cleanup\n");
    LV2_Handle h = desc->instantiate(desc, 44100.0, "", NULL);
    CHECK(h != NULL, "instantiate at 44100 Hz returns non-NULL");
    desc->cleanup(h);

    h = desc->instantiate(desc, 96000.0, "", NULL);
    CHECK(h != NULL, "instantiate at 96000 Hz returns non-NULL");
    desc->cleanup(h);
}

static void test_activate_deactivate_cycle(void)
{
    printf("test_activate_deactivate_cycle\n");
    LV2_Handle h = desc->instantiate(desc, 48000.0, "", NULL);
    desc->activate(h);
    desc->deactivate(h);
    desc->activate(h);
    desc->deactivate(h);
    desc->cleanup(h);
    CHECK(1, "multiple activate/deactivate cycles do not crash");
}

static void test_center_pan_equal_gain(void)
{
    printf("test_center_pan_equal_gain\n");
    Fixture* f = fx_create();
    f->pan = 0.0f;
    fx_run(f);

    float expected = cosf((float)(M_PI / 4.0));  /* √2/2 ≈ 0.7071 at 0 dB */
    CHECK_NEAR(f->out_l[0], expected, EPSILON,
               "centre pan 0 dB: left = cos(π/4)");
    CHECK_NEAR(f->out_r[0], expected, EPSILON,
               "centre pan 0 dB: right = sin(π/4)");
    CHECK_NEAR(f->out_l[0], f->out_r[0], EPSILON,
               "centre pan: L and R are equal");

    fx_destroy(f);
}

static void test_center_pan_power_preserved(void)
{
    printf("test_center_pan_power_preserved\n");
    Fixture* f = fx_create();
    f->pan    = 0.0f;
    f->volume = 0.0f;   /* 0 dB = unity gain */
    fx_run(f);

    float power = f->out_l[0]*f->out_l[0] + f->out_r[0]*f->out_r[0];
    CHECK_NEAR(power, 1.0f, EPSILON,
               "centre pan 0 dB: gl²+gr² = 1 (power preserved)");

    fx_destroy(f);
}

static void test_full_left_pan(void)
{
    printf("test_full_left_pan\n");
    Fixture* f = fx_create();
    f->pan = -1.0f;
    fx_run(f);

    CHECK_NEAR(f->out_l[0], 1.0f, EPSILON, "full-left pan: left = 1.0");
    CHECK_NEAR(f->out_r[0], 0.0f, EPSILON, "full-left pan: right = 0.0");

    fx_destroy(f);
}

static void test_full_right_pan(void)
{
    printf("test_full_right_pan\n");
    Fixture* f = fx_create();
    f->pan = 1.0f;
    fx_run(f);

    CHECK_NEAR(f->out_l[0], 0.0f, EPSILON, "full-right pan: left = 0.0");
    CHECK_NEAR(f->out_r[0], 1.0f, EPSILON, "full-right pan: right = 1.0");

    fx_destroy(f);
}

static void test_constant_power_law(void)
{
    printf("test_constant_power_law\n");
    static const float pans[] = { -1.0f, -0.75f, -0.5f, -0.25f, 0.0f,
                                    0.25f,  0.5f,  0.75f,  1.0f };
    Fixture* f = fx_create();
    f->volume = 0.0f;   /* 0 dB = unity gain */

    for (int i = 0; i < (int)(sizeof(pans)/sizeof(*pans)); ++i) {
        f->pan = pans[i];
        fx_run(f);
        float power = f->out_l[0]*f->out_l[0] + f->out_r[0]*f->out_r[0];
        CHECK_NEAR(power, 1.0f, EPSILON,
                   "constant-power law: gl²+gr² = 1 at all pan positions");
    }

    fx_destroy(f);
}

static void test_pan_monotonic(void)
{
    printf("test_pan_monotonic\n");
    /* As pan increases, left gain should decrease and right gain increase */
    Fixture* f    = fx_create();
    f->volume     = 0.0f;   /* 0 dB = unity gain */
    float prev_l  = 2.0f;
    float prev_r  = -1.0f;

    static const float pans[] = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
    for (int i = 0; i < (int)(sizeof(pans)/sizeof(*pans)); ++i) {
        f->pan = pans[i];
        fx_run(f);
        CHECK(f->out_l[0] <= prev_l + EPSILON,
              "left gain is non-increasing as pan sweeps right");
        CHECK(f->out_r[0] >= prev_r - EPSILON,
              "right gain is non-decreasing as pan sweeps right");
        prev_l = f->out_l[0];
        prev_r = f->out_r[0];
    }

    fx_destroy(f);
}

static void test_volume_min(void)
{
    printf("test_volume_min\n");
    /* -60 dB → gain = 10^(-3) = 0.001; full-left pan avoids cos/sin factor */
    Fixture* f = fx_create();
    f->pan    = -1.0f;
    f->volume = -60.0f;
    fx_run(f);

    CHECK_NEAR(f->out_l[0], 0.001f, EPSILON, "volume -60 dB: left = 0.001");
    CHECK_NEAR(f->out_r[0], 0.0f,   EPSILON, "volume -60 dB: right = 0 (full-left pan)");

    fx_destroy(f);
}

static void test_volume_max(void)
{
    printf("test_volume_max\n");
    /* +20 dB → gain = 10^1 = 10; full-left pan avoids cos/sin factor */
    Fixture* f = fx_create();
    f->pan    = -1.0f;
    f->volume = 20.0f;
    fx_run(f);

    CHECK_NEAR(f->out_l[0], 10.0f, EPSILON, "volume +20 dB: left = 10.0");
    CHECK_NEAR(f->out_r[0], 0.0f,  EPSILON, "volume +20 dB: right = 0 (full-left pan)");

    fx_destroy(f);
}

static void test_volume_6dB_doubles(void)
{
    printf("test_volume_6dB_doubles\n");
    /* +6 dB should multiply amplitude by 10^(6/20) ≈ 1.995 */
    Fixture* f = fx_create();
    f->pan     = -1.0f;   /* full left for clean comparison */

    f->volume = 0.0f;
    fx_run(f);
    float at_unity = f->out_l[0];

    f->volume = 6.0f;
    fx_run(f);
    float at_6db = f->out_l[0];

    CHECK_NEAR(at_6db / at_unity, powf(10.0f, 6.0f / 20.0f), EPSILON,
               "+6 dB increases amplitude by 10^(6/20)");

    fx_destroy(f);
}

static void test_volume_unity_centre_pan(void)
{
    printf("test_volume_unity_centre_pan\n");
    /* 0 dB at centre pan: each channel = cos(π/4) ≈ 0.7071 */
    Fixture* f = fx_create();
    f->pan    = 0.0f;
    f->volume = 0.0f;
    fx_run(f);

    float expected = cosf((float)(M_PI / 4.0));
    CHECK_NEAR(f->out_l[0], expected, EPSILON,
               "0 dB centre pan: left = cos(π/4)");
    CHECK_NEAR(f->out_r[0], expected, EPSILON,
               "0 dB centre pan: right = sin(π/4)");

    fx_destroy(f);
}

static void test_mute_silences_output(void)
{
    printf("test_mute_silences_output\n");
    Fixture* f = fx_create();
    f->mute    = 1.0f;
    fx_run(f);

    CHECK(all_near(f->out_l, 0.0f, EPSILON), "mute: all left samples = 0");
    CHECK(all_near(f->out_r, 0.0f, EPSILON), "mute: all right samples = 0");

    fx_destroy(f);
}

static void test_mute_overrides_volume(void)
{
    printf("test_mute_overrides_volume\n");
    Fixture* f = fx_create();
    f->volume  = 20.0f;  /* +20 dB */
    f->mute    = 1.0f;
    fx_run(f);

    CHECK(all_near(f->out_l, 0.0f, EPSILON), "mute+volume: left = 0");
    CHECK(all_near(f->out_r, 0.0f, EPSILON), "mute+volume: right = 0");

    fx_destroy(f);
}

static void test_unmute_restores_signal(void)
{
    printf("test_unmute_restores_signal\n");
    Fixture* f = fx_create();
    f->mute    = 1.0f;
    fx_run(f);

    f->mute = 0.0f;
    fx_run(f);

    float expected = cosf((float)(M_PI / 4.0));
    CHECK_NEAR(f->out_l[0], expected, EPSILON, "unmute restores left signal");
    CHECK_NEAR(f->out_r[0], expected, EPSILON, "unmute restores right signal");

    fx_destroy(f);
}

static void test_mute_invert_alone_silences(void)
{
    printf("test_mute_invert_alone_silences\n");
    /* mute=0, mute_invert=1 → effective mute = 0 XOR 1 = 1 → silence */
    Fixture* f      = fx_create();
    f->mute_invert  = 1.0f;
    fx_run(f);

    CHECK(all_near(f->out_l, 0.0f, EPSILON),
          "mute_invert alone: left = 0");
    CHECK(all_near(f->out_r, 0.0f, EPSILON),
          "mute_invert alone: right = 0");

    fx_destroy(f);
}

static void test_mute_invert_cancels_mute(void)
{
    printf("test_mute_invert_cancels_mute\n");
    /* mute=1, mute_invert=1 → effective mute = 1 XOR 1 = 0 → audio passes */
    Fixture* f      = fx_create();
    f->mute         = 1.0f;
    f->mute_invert  = 1.0f;
    fx_run(f);

    float expected = cosf((float)(M_PI / 4.0));
    CHECK_NEAR(f->out_l[0], expected, EPSILON,
               "mute_invert cancels mute: left signal passes");
    CHECK_NEAR(f->out_r[0], expected, EPSILON,
               "mute_invert cancels mute: right signal passes");

    fx_destroy(f);
}

static void test_mute_invert_off_no_effect(void)
{
    printf("test_mute_invert_off_no_effect\n");
    /* mute=0, mute_invert=0 → behaves identically to no mute at all */
    Fixture* f = fx_create();
    fx_run(f);

    float expected = cosf((float)(M_PI / 4.0));
    CHECK_NEAR(f->out_l[0], expected, EPSILON,
               "mute_invert=0: left signal unaffected");
    CHECK_NEAR(f->out_r[0], expected, EPSILON,
               "mute_invert=0: right signal unaffected");

    fx_destroy(f);
}

static void test_bypass_passes_input_through(void)
{
    printf("test_bypass_passes_input_through\n");
    Fixture* f = fx_create();
    f->enabled = 0.0f;
    f->volume  = -20.0f;  /* would attenuate if active */
    f->pan     = 1.0f;    /* would silence left if active */

    for (int i = 0; i < NSAMPLES; ++i)
        f->in[i] = (float)i / (float)NSAMPLES;

    fx_run(f);

    int l_ok = 1, r_ok = 1;
    for (int i = 0; i < NSAMPLES; ++i) {
        float expected = (float)i / (float)NSAMPLES;
        if (fabsf(f->out_l[i] - expected) >= EPSILON) l_ok = 0;
        if (fabsf(f->out_r[i] - expected) >= EPSILON) r_ok = 0;
    }
    CHECK(l_ok, "bypass: all left samples equal input");
    CHECK(r_ok, "bypass: all right samples equal input");

    fx_destroy(f);
}

static void test_bypass_overrides_mute(void)
{
    printf("test_bypass_overrides_mute\n");
    Fixture* f = fx_create();
    f->enabled = 0.0f;
    f->mute    = 1.0f;
    fx_run(f);

    /* Bypass takes priority: signal passes through regardless of mute */
    CHECK(all_near(f->out_l, 1.0f, EPSILON),
          "bypass+mute: left = input (bypass wins)");
    CHECK(all_near(f->out_r, 1.0f, EPSILON),
          "bypass+mute: right = input (bypass wins)");

    fx_destroy(f);
}

static void test_bypass_overrides_mute_invert(void)
{
    printf("test_bypass_overrides_mute_invert\n");
    /* bypass takes priority over mute_invert: input passes through unchanged */
    Fixture* f      = fx_create();
    f->enabled      = 0.0f;
    f->mute_invert  = 1.0f;  /* would silence if bypass weren't active */
    fx_run(f);

    CHECK(all_near(f->out_l, 1.0f, EPSILON),
          "bypass+mute_invert: left = input (bypass wins)");
    CHECK(all_near(f->out_r, 1.0f, EPSILON),
          "bypass+mute_invert: right = input (bypass wins)");

    fx_destroy(f);
}

static void test_re_enable_restores_processing(void)
{
    printf("test_re_enable_restores_processing\n");
    Fixture* f = fx_create();
    f->enabled = 0.0f;
    fx_run(f);

    f->enabled = 1.0f;
    f->pan     = -1.0f;
    f->volume  = 0.0f;   /* 0 dB = unity gain */
    fx_run(f);

    CHECK_NEAR(f->out_l[0], 1.0f, EPSILON, "re-enable: processing resumes (left)");
    CHECK_NEAR(f->out_r[0], 0.0f, EPSILON, "re-enable: processing resumes (right)");

    fx_destroy(f);
}

static void test_silent_input(void)
{
    printf("test_silent_input\n");
    Fixture* f = fx_create();
    for (int i = 0; i < NSAMPLES; ++i)
        f->in[i] = 0.0f;
    fx_run(f);

    CHECK(all_near(f->out_l, 0.0f, EPSILON), "silent input: left = 0");
    CHECK(all_near(f->out_r, 0.0f, EPSILON), "silent input: right = 0");

    fx_destroy(f);
}

static void test_negative_input_sign_preserved(void)
{
    printf("test_negative_input_sign_preserved\n");
    Fixture* f = fx_create();
    for (int i = 0; i < NSAMPLES; ++i)
        f->in[i] = -1.0f;
    f->pan    = 0.0f;
    f->volume = 0.0f;   /* 0 dB = unity gain */
    fx_run(f);

    float expected = -cosf((float)(M_PI / 4.0));
    CHECK_NEAR(f->out_l[0], expected, EPSILON, "negative input: left sign preserved");
    CHECK_NEAR(f->out_r[0], expected, EPSILON, "negative input: right sign preserved");

    fx_destroy(f);
}

static void test_all_samples_receive_same_gain(void)
{
    printf("test_all_samples_receive_same_gain\n");
    Fixture* f = fx_create();
    f->pan    = 0.25f;
    f->volume = -3.0f;  /* -3 dB */

    float angle      = (f->pan + 1.0f) * (float)(M_PI / 4.0);
    float gain       = powf(10.0f, f->volume / 20.0f);
    float expected_l = gain * cosf(angle);
    float expected_r = gain * sinf(angle);

    fx_run(f);

    CHECK(all_near(f->out_l, expected_l, EPSILON),
          "all 64 left samples equal expected gain");
    CHECK(all_near(f->out_r, expected_r, EPSILON),
          "all 64 right samples equal expected gain");

    fx_destroy(f);
}

static void test_output_gain_formula(void)
{
    printf("test_output_gain_formula\n");
    static const struct { float pan; float vol_db; } cases[] = {
        { -0.5f, -10.0f },
        {  0.3f,   6.0f },
        {  0.0f, -60.0f },
        {  1.0f,   0.0f },
        { -1.0f,  20.0f },
    };

    Fixture* f = fx_create();

    for (int i = 0; i < (int)(sizeof(cases)/sizeof(*cases)); ++i) {
        f->pan    = cases[i].pan;
        f->volume = cases[i].vol_db;
        fx_run(f);

        float angle = (cases[i].pan + 1.0f) * (float)(M_PI / 4.0);
        float gain  = powf(10.0f, cases[i].vol_db / 20.0f);
        float el    = gain * cosf(angle);
        float er    = gain * sinf(angle);

        CHECK_NEAR(f->out_l[0], el, EPSILON, "output matches gain formula (left)");
        CHECK_NEAR(f->out_r[0], er, EPSILON, "output matches gain formula (right)");
    }

    fx_destroy(f);
}

/* ---- main ---- */

int main(void)
{
    desc = lv2_descriptor(0);

    test_descriptor();
    test_instantiate_cleanup();
    test_activate_deactivate_cycle();
    test_center_pan_equal_gain();
    test_center_pan_power_preserved();
    test_full_left_pan();
    test_full_right_pan();
    test_constant_power_law();
    test_pan_monotonic();
    test_volume_min();
    test_volume_max();
    test_volume_6dB_doubles();
    test_volume_unity_centre_pan();
    test_mute_silences_output();
    test_mute_overrides_volume();
    test_unmute_restores_signal();
    test_mute_invert_alone_silences();
    test_mute_invert_cancels_mute();
    test_mute_invert_off_no_effect();
    test_bypass_passes_input_through();
    test_bypass_overrides_mute();
    test_bypass_overrides_mute_invert();
    test_re_enable_restores_processing();
    test_silent_input();
    test_negative_input_sign_preserved();
    test_all_samples_receive_same_gain();
    test_output_gain_formula();

    printf("\n%d/%d tests passed", tests_passed, tests_run);
    if (tests_failed)
        printf(", %d FAILED", tests_failed);
    printf("\n");

    return tests_failed ? 1 : 0;
}
