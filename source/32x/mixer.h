#ifndef MIXER_INCLUDE_H
#define MIXER_INCLUDE_H

#include <xmp.h>

/* COMM6 - mixer status */
#define MIXER_INITIALIZE    0
#define MIXER_UNLOCKED      1
#define MIXER_LOCK_MSH2     2
#define MIXER_LOCK_SSH2     3

void MixSamples(mixer_t *mixer, int16_t *buffer, int32_t cnt, int32_t scale);

void LockMixer(int16_t id);
void UnlockMixer(void);

#endif
