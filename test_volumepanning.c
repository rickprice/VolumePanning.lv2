#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

#include "lv2/core/lv2.h"

extern const LV2_Descriptor* lv2_descriptor(uint32_t index);

enum {
    PORT_IN      = 0,
    PORT_OUT_L   = 1,
    PORT_OUT_R   = 2,
    PORT_PAN     = 3,
    PORT_VOLUME  = 4,
    PORT_MUTE    = 5,
    PORT_ENABLED = 6,
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
} Fixture;

static const LV2_Descriptor* desc = NULL;

static Fixture* fx_create(void)
{
    Fixture* f = (Fixture*)calloc(1, sizeof(Fixture));
    f->handle = desc->instantiate(desc, 48000.0, "", NULL);
    if (!f->handle) { free(f); return NULL; }

    desc->connect_port(f->handle, PORT_IN,      f->in);
    desc->connect_port(f->handle, PORT_OUT_L,   f->out_l);
    desc->connect_port(f->handle, PORT_OUT_R,   f->out_r);
    desc->connect_port(f->handle, PORT_PAN,     &f->pan);
    desc->connect_port(f->handle, PORT_VOLUME,  &f->volume);
    desc->connect_port(f->handle, PORT_MUTE,    &f->mute);
    desc->connect_port(f->handle, PORT_ENABLED, &f->enabled);

    /* Default state: unity gain, centre pan, active */
    f->pan     = 0.0f;
    f->volume  = 1.0f;
    f->mute    = 0.0f;
    f->enabled = 1.0f;

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

    float expected = cosf((float)(M_PI / 4.0));  /* √2/2 ≈ 0.7071 */
    CHECK_NEAR(f->out_l[0], expected, EPSILON,
               "centre pan: left gain = cos(π/4)");
    CHECK_NEAR(f->out_r[0], expected, EPSILON,
               "centre pan: right gain = sin(π/4)");
    CHECK_NEAR(f->out_l[0], f->out_r[0], EPSILON,
               "centre pan: L and R are equal");

    fx_destroy(f);
}

static void test_center_pan_power_preserved(void)
{
    printf("test_center_pan_power_preserved\n");
    Fixture* f = fx_create();
    f->pan    = 0.0f;
    f->volume = 1.0f;
    fx_run(f);

    float power = f->out_l[0]*f->out_l[0] + f->out_r[0]*f->out_r[0];
    CHECK_NEAR(power, 1.0f, EPSILON,
               "centre pan: gl²+gr² = vol² (power preserved)");

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
    f->volume = 1.0f;

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
    f->volume     = 1.0f;
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

static void test_volume_zero(void)
{
    printf("test_volume_zero\n");
    Fixture* f = fx_create();
    f->volume  = 0.0f;
    fx_run(f);

    CHECK(all_near(f->out_l, 0.0f, EPSILON), "volume 0: all left samples = 0");
    CHECK(all_near(f->out_r, 0.0f, EPSILON), "volume 0: all right samples = 0");

    fx_destroy(f);
}

static void test_volume_double(void)
{
    printf("test_volume_double\n");
    Fixture* f = fx_create();
    f->pan    = 0.0f;
    f->volume = 2.0f;
    fx_run(f);

    float expected = 2.0f * cosf((float)(M_PI / 4.0));
    CHECK_NEAR(f->out_l[0], expected, EPSILON, "volume 2.0: left = 2·cos(π/4)");
    CHECK_NEAR(f->out_r[0], expected, EPSILON, "volume 2.0: right = 2·sin(π/4)");

    fx_destroy(f);
}

static void test_volume_scales_linearly(void)
{
    printf("test_volume_scales_linearly\n");
    Fixture* f = fx_create();
    f->pan     = 0.25f;

    f->volume = 1.0f;
    fx_run(f);
    float l1 = f->out_l[0];
    float r1 = f->out_r[0];

    f->volume = 0.5f;
    fx_run(f);
    float l2 = f->out_l[0];
    float r2 = f->out_r[0];

    CHECK_NEAR(l2, l1 * 0.5f, EPSILON, "volume scales left linearly");
    CHECK_NEAR(r2, r1 * 0.5f, EPSILON, "volume scales right linearly");

    fx_destroy(f);
}

static void test_unity_gain_at_centre_with_sqrt2(void)
{
    printf("test_unity_gain_at_centre_with_sqrt2\n");
    /* vol=√2, pan=0 → each channel amplitude = 1.0 */
    Fixture* f = fx_create();
    f->pan    = 0.0f;
    f->volume = (float)M_SQRT2;
    fx_run(f);

    CHECK_NEAR(f->out_l[0], 1.0f, EPSILON, "volume=√2 centre: left = 1.0");
    CHECK_NEAR(f->out_r[0], 1.0f, EPSILON, "volume=√2 centre: right = 1.0");

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
    f->volume  = 2.0f;
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

static void test_bypass_passes_input_through(void)
{
    printf("test_bypass_passes_input_through\n");
    Fixture* f = fx_create();
    f->enabled = 0.0f;
    f->volume  = 0.5f;  /* would attenuate if active */
    f->pan     = 1.0f;  /* would silence left if active */

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

static void test_re_enable_restores_processing(void)
{
    printf("test_re_enable_restores_processing\n");
    Fixture* f = fx_create();
    f->enabled = 0.0f;
    fx_run(f);

    f->enabled = 1.0f;
    f->pan     = -1.0f;
    f->volume  = 1.0f;
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
    f->volume = 1.0f;
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
    f->volume = 0.8f;

    float angle      = (f->pan + 1.0f) * (float)(M_PI / 4.0);
    float expected_l = f->volume * cosf(angle);
    float expected_r = f->volume * sinf(angle);

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
    static const struct { float pan; float vol; } cases[] = {
        { -0.5f, 0.7f },
        {  0.3f, 1.5f },
        {  0.0f, 0.0f },
        {  1.0f, 1.0f },
        { -1.0f, 2.0f },
    };

    Fixture* f = fx_create();

    for (int i = 0; i < (int)(sizeof(cases)/sizeof(*cases)); ++i) {
        f->pan    = cases[i].pan;
        f->volume = cases[i].vol;
        fx_run(f);

        float angle = (cases[i].pan + 1.0f) * (float)(M_PI / 4.0);
        float el    = cases[i].vol * cosf(angle);
        float er    = cases[i].vol * sinf(angle);

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
    test_volume_zero();
    test_volume_double();
    test_volume_scales_linearly();
    test_unity_gain_at_centre_with_sqrt2();
    test_mute_silences_output();
    test_mute_overrides_volume();
    test_unmute_restores_signal();
    test_bypass_passes_input_through();
    test_bypass_overrides_mute();
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
