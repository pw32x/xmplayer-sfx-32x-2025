#include "mixer.h"

#include "32x.h"

#include <limits.h> // For INT16_MIN/MAX
#include <stdint.h>
#include <stddef.h>

// Pre-calculated tables for Equal Distance Crossfade
#ifdef EQUAL_DISTANCE_CROSSFADE
extern const uint8_t edc_left[];
extern const uint8_t edc_right[];
#endif

static inline int16_t clamp16(int32_t val)
{
    if (val < INT16_MIN) return INT16_MIN;
    if (val > INT16_MAX) return INT16_MAX;
    return (int16_t)val;
}

void MixSamples(mixer_t *mixer, int16_t *buffer, int32_t count, int32_t scale)
{
    if (!mixer || !mixer->data)
        return;

    const int8_t *data = mixer->data;
    uint32_t pos = mixer->position;
    uint32_t inc = mixer->increment;
    uint32_t len = mixer->length;
    uint32_t loop_len = mixer->loop_length;
    int32_t vol = mixer->volume & 0xFF; // promote to int32
    uint8_t pan = mixer->pan;

    // Calculate left and right volumes based on crossfade mode
    int32_t left_vol = 0;
    int32_t right_vol = 0;

#ifdef INTENSITY_CROSSFADE
    {
        int32_t p = pan;
        int32_t inv_p = 255 - pan;

        // pan^2 * vol * scale >> 16 (approximate)
        int32_t pan_sq = (p * p) >> 8;
        int32_t inv_p_sq = (inv_p * inv_p) >> 8;

        right_vol = (pan_sq * vol * scale) >> 16;
        left_vol = (inv_p_sq * vol * scale) >> 16;
    }
#elif defined(LINEAR_CROSSFADE)
    {
        int32_t p = pan;
        int32_t inv_p = 255 - pan;

        right_vol = (p * vol * scale) >> 12;      // >>6*2
        left_vol  = (inv_p * vol * scale) >> 12;
    }
#elif defined(EQUAL_DISTANCE_CROSSFADE)
    {
        // Lookup values in EDC tables
        int32_t epan_left = edc_left[pan];
        int32_t epan_right = edc_right[pan];

        left_vol  = (epan_left * vol * scale) >> 12;
        right_vol = (epan_right * vol * scale) >> 12;
    }
#else
    #error "No crossfade mode defined"
#endif

    for (int32_t i = 0; i < count; ++i)
    {
        uint32_t sample_idx = pos >> 14;  // fixed point 18.14 -> integer index

        // Clamp sample index to length range to be safe
        if (sample_idx >= (len >> 14))
            sample_idx = (len >> 14) - 1;

        int8_t sample = data[sample_idx];

        // Mix left channel
        int32_t mixed_l = buffer[0] + ((sample * left_vol) >> 8);
        buffer[0] = clamp16(mixed_l);

        // Mix right channel
        int32_t mixed_r = buffer[1] + ((sample * right_vol) >> 8);
        buffer[1] = clamp16(mixed_r);

        buffer += 2;
        pos += inc;

        if (pos >= len)
        {
            if (loop_len != 0)
            {
                pos -= loop_len;
            }
            else
            {
                mixer->data = NULL;  // mark as done
                break;
            }
        }
    }

    mixer->position = pos;
}



void LockMixer(int16_t id) 
{
    int16_t current_state;

    do 
    {
        while (MARS_SYS_COMM6 != MIXER_UNLOCKED) 
        {
            // wait until it's unlocked
        }

        // set the given id
        MARS_SYS_COMM6 = id;

        // check if the id was actually written. the other CPU might
        // have gone first.
        current_state = MARS_SYS_COMM6;


    } while (current_state != id); // loop until we have the id written
}

void UnlockMixer(void)
{
    MARS_SYS_COMM6 = MIXER_UNLOCKED;
}