#ifndef SOUND_MANAGER_INCLUDE_H
#define SOUND_MANAGER_INCLUDE_H

#include <stdint.h>

extern int8_t g_masterVolume;
extern const int8_t g_maxMasterVolume;

#define INVALID_SOUND_HANDLE 0xFF
typedef uint8_t SoundHandle;

// call on the secondary CPU
void SoundManager_init();
void SoundManager_waitUntilInitialized();

// call on the primary CPU
void SoundManager_updateMain();
void SoundManager_shutdown();

// sound effect functions
SoundHandle SoundManager_playSoundEffect(int8_t *data, uint16_t srate, uint32_t loop_length, uint32_t length, uint8_t volume, uint8_t pan);
void SoundManager_stopSoundEffect(SoundHandle soundHandle);
void SoundManager_changeSoundEffectParams(SoundHandle soundHandle, int32_t srate, int32_t volume, int32_t pan);
uint8_t SoundManager_isSoundEffectPlaying(SoundHandle soundHandle);

// song functions
uint8_t SoundManager_playSong(int index);
void SoundManager_pauseSong(uint8_t pause);
uint8_t SoundManager_isSongPlaying();

#endif
