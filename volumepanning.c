#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "lv2/core/lv2.h"

#define PLUGIN_URI "http://fprice.pricemail.ca/plugins/volumepanning"

typedef enum {
    PORT_IN      = 0,
    PORT_OUT_L   = 1,
    PORT_OUT_R   = 2,
    PORT_PAN     = 3,
    PORT_VOLUME  = 4,
    PORT_MUTE         = 5,
    PORT_ENABLED      = 6,
    PORT_MUTE_INVERT  = 7,
} PortIndex;

typedef struct {
    const float* in;
    float*       out_l;
    float*       out_r;
    const float* pan;
    const float* volume;
    const float* mute;
    const float* enabled;
    const float* mute_invert;
} Plugin;

static LV2_Handle instantiate(
    const LV2_Descriptor*     descriptor,
    double                    rate,
    const char*               bundle_path,
    const LV2_Feature* const* features)
{
    (void)descriptor; (void)rate; (void)bundle_path; (void)features;
    return (LV2_Handle)calloc(1, sizeof(Plugin));
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Plugin* self = (Plugin*)instance;
    switch ((PortIndex)port) {
    case PORT_IN:      self->in      = (const float*)data; break;
    case PORT_OUT_L:   self->out_l   = (float*)data;       break;
    case PORT_OUT_R:   self->out_r   = (float*)data;       break;
    case PORT_PAN:     self->pan     = (const float*)data; break;
    case PORT_VOLUME:  self->volume  = (const float*)data; break;
    case PORT_MUTE:         self->mute         = (const float*)data; break;
    case PORT_ENABLED:      self->enabled      = (const float*)data; break;
    case PORT_MUTE_INVERT:  self->mute_invert  = (const float*)data; break;
    }
}

static void activate(LV2_Handle instance) { (void)instance; }

static void run(LV2_Handle instance, uint32_t n_samples)
{
    const Plugin* self    = (const Plugin*)instance;
    const float*  in      = self->in;
    float*        out_l   = self->out_l;
    float*        out_r   = self->out_r;

    /* lv2:enabled — when 0 the host may skip run() entirely, but we handle it
       here too so plugin-level bypass works without host support. */
    if (*self->enabled < 0.5f) {
        for (uint32_t i = 0; i < n_samples; ++i) {
            out_l[i] = in[i];
            out_r[i] = in[i];
        }
        return;
    }

    const int muted = (*self->mute >= 0.5f) ^ (*self->mute_invert >= 0.5f);
    if (muted) {
        for (uint32_t i = 0; i < n_samples; ++i) {
            out_l[i] = 0.0f;
            out_r[i] = 0.0f;
        }
        return;
    }

    /* Constant-power (equal-power) panning.
       pan=-1 → full left, pan=0 → centre, pan=+1 → full right.
       angle sweeps 0..π/2 so cos²+sin²=1 (total power preserved). */
    const float angle = (*self->pan + 1.0f) * (float)(M_PI / 4.0);
    const float vol   = *self->volume;
    const float gl    = vol * cosf(angle);
    const float gr    = vol * sinf(angle);

    for (uint32_t i = 0; i < n_samples; ++i) {
        out_l[i] = in[i] * gl;
        out_r[i] = in[i] * gr;
    }
}

static void deactivate(LV2_Handle instance) { (void)instance; }

static void cleanup(LV2_Handle instance)
{
    free(instance);
}

static const void* extension_data(const char* uri)
{
    (void)uri;
    return NULL;
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data,
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return index == 0 ? &descriptor : NULL;
}
