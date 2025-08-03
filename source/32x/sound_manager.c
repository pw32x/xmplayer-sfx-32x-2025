// SoundManager for 32X by pw_32x
// based on sound.c from Chilly Willy's xmplayer library

#include "sound_manager.h"

#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <xmp.h>

#include "32x.h"
#include "hw_32x.h"
#include "mixer.h"

#define SAMPLE_RATE    22050
#define SAMPLE_MIN         2
#define SAMPLE_CENTER    517
#define SAMPLE_MAX      1032
#define MAX_NUM_SAMPLES 1024

#define NUM_SFX_MIXERS 4

int8_t g_masterVolume = XMP_MAX_VOLUME;
const int8_t g_maxMasterVolume = XMP_MAX_VOLUME;

song_t __attribute__((aligned(16))) *g_song = (song_t *)NULL;

int16_t __attribute__((aligned(16))) g_soundBuffer0[MAX_NUM_SAMPLES * 2];
int16_t __attribute__((aligned(16))) g_soundBuffer1[MAX_NUM_SAMPLES * 2];
int16_t* g_currentSoundBuffer;

uint16_t g_samplesCount = 441;

static mixer_t __attribute__((aligned(16))) sfx_mixers[NUM_SFX_MIXERS];

void update_sound_buffer(int16_t *soundBuffer);
void SoundManager_updateDma();


void SoundManager_init()
{
    uint16_t sample, ix;

    // init DMA
    SH2_DMA_SAR0 = 0;
    SH2_DMA_DAR0 = 0;
    SH2_DMA_TCR0 = 0;
    SH2_DMA_CHCR0 = 0;
    SH2_DMA_DRCR0 = 0;
    SH2_DMA_SAR1 = 0;
    SH2_DMA_DAR1 = 0x20004034; // storing a long here will set left and right
    SH2_DMA_TCR1 = 0;
    SH2_DMA_CHCR1 = 0;
    SH2_DMA_DRCR1 = 0;
    SH2_DMA_DMAOR = 1; // enable DMA

    SH2_DMA_VCR1 = 72; // set exception vector for DMA channel 1
    SH2_INT_IPRA = (SH2_INT_IPRA & 0xF0FF) | 0x0F00; // set DMA INT to priority 15

    // init the sound hardware
    MARS_PWM_MONO = 1;
    MARS_PWM_MONO = 1;
    MARS_PWM_MONO = 1;
    if (MARS_VDP_DISPMODE & MARS_NTSC_FORMAT)
        MARS_PWM_CYCLE = (((23011361 << 1)/SAMPLE_RATE + 1) >> 1) + 1; // for NTSC clock
    else
        MARS_PWM_CYCLE = (((22801467 << 1)/SAMPLE_RATE + 1) >> 1) + 1; // for PAL clock
    MARS_PWM_CTRL = 0x0185; // TM = 1, RTP, RMD = right, LMD = left

    sample = SAMPLE_MIN;
    /* ramp up to SAMPLE_CENTER to avoid click in audio (real 32X) */
    while (sample < SAMPLE_CENTER)
    {
        for (ix=0; ix<(SAMPLE_RATE*2)/(SAMPLE_CENTER - SAMPLE_MIN); ix++)
        {
            while (MARS_PWM_MONO & 0x8000) ; // wait while full
            MARS_PWM_MONO = sample;
        }
        sample++;
    }

    // initialize mixer
    MARS_SYS_COMM6 = MIXER_UNLOCKED; // sound subsystem running
    g_currentSoundBuffer = g_soundBuffer0;
    update_sound_buffer(g_currentSoundBuffer); // fill first buffer
    SoundManager_updateDma(); // start DMA

    memset(sfx_mixers, 0, sizeof(sfx_mixers));

    SetSH2SR(2);
}

void SoundManager_waitUntilInitialized()
{
    while (MARS_SYS_COMM6 != MIXER_UNLOCKED) ; // wait for sound subsystem to init
}

void SoundManager_updateMain()
{
    CacheClearLine(g_song); // just clear shared state variables
}

static inline void flush_mem(void *ptr, int32_t len)
{
    while (len > 0)
    {
        CacheClearLine(ptr);
        ptr += 16;
        len -= 16;
    }
}

static void update_soundeffects(int16_t *soundBuffer)
{
    uint8_t i;

    for (i = 0; i < NUM_SFX_MIXERS; i++)
    {
        CacheClearLine(&sfx_mixers[i].data);

        if (sfx_mixers[i].data)
        {
            MixSamples(&sfx_mixers[i], soundBuffer, g_samplesCount, 64);
        }
    }
}

static void update_music(int16_t *soundBuffer)
{
    int32_t scale;
    uint16_t bpm;
    uint8_t nch, i;
    static song_t *last_song = (song_t *)NULL;
    song_t **usong = (song_t **)((int32_t)&g_song | 0x20000000);
    song_t *lsong;
    int8_t *umvolume = (int8_t *)((int32_t)&g_masterVolume | 0x20000000);

    lsong = (song_t *)*usong; // get local song pointer from uncached song handle
    if (((int32_t)lsong & 0x1FFFFFFF) == 0)
    {
        last_song = (song_t *)NULL;
        return;
    }

    if (lsong != last_song)
    {
        // song changed - flush entire struct
        flush_mem(lsong, sizeof(song_t) - sizeof(channel_t)); // flush song variables
        nch = xmp_num_channels(lsong);
        for (i = 0; i < nch; i++)
            flush_mem(xmp_get_channel(lsong, i), sizeof(channel_t));
        last_song = lsong;
    }
    else
    {
        CacheClearLine(lsong); // just clear shared state variables
    }

    if (!xmp_is_playing(lsong) || xmp_is_paused(lsong))
        return;

    // process music
    xmp_update(lsong);
    if (!xmp_is_playing(lsong))
        return; // just stopped

    bpm = xmp_get_tempo(lsong);
    g_samplesCount = (SAMPLE_RATE * 5)/(bpm * 2);
    if (g_samplesCount > MAX_NUM_SAMPLES)
        g_samplesCount = MAX_NUM_SAMPLES;

    // mix music samples
    scale = xmp_get_scale(g_song, *umvolume);
    if (scale)
    {
        nch = xmp_num_channels(lsong);
        for (i = 0; i < nch; i++)
        {
            channel_t *chan = xmp_get_channel(lsong, i);
            mixer_t *mix = (mixer_t *)chan;
            // process envelope
            xmp_proc_vol_envelope(chan);
            // mix channel to sound buffer
            if (mix->data && mix->volume)
            {
                int32_t env = xmp_get_vol_envelope(chan);
                int8_t cscale = xmp_get_vol_fadeout(chan, scale);
                if (env == -1)
                {
                    // no envelope - just use scale
                    MixSamples(mix, soundBuffer, g_samplesCount, cscale);
                }
                else
                {
                    // envelope enabled - use envelope value * scale / 64
                    MixSamples(mix, soundBuffer, g_samplesCount, (int8_t)((uint16_t)(cscale * env) >> 6));
                }
            }
        }
    }
}

void update_sound_buffer(int16_t *soundBuffer)
{
    int16_t i;

    memset(soundBuffer, 0, g_samplesCount * 4);

    LockMixer(MIXER_LOCK_SSH2);
    update_music(soundBuffer);
    update_soundeffects(soundBuffer);
    UnlockMixer();

    // convert buffer from s16 pcm samples to u16 pwm samples
    for (i = 0; i < g_samplesCount * 2; i++)
    {
        int16_t s = *soundBuffer + SAMPLE_CENTER;
        *soundBuffer++ = (s < 0) ? SAMPLE_MIN : (s > SAMPLE_MAX) ? SAMPLE_MAX : s;
    }
}


SoundHandle SoundManager_playSoundEffect(int8_t *data, uint16_t srate, uint32_t loop_length, uint32_t length, uint8_t volume, uint8_t pan)
{
    SoundHandle i;

    LockMixer(MIXER_LOCK_MSH2);

    // look for free mixer
    for (i = 0; i < NUM_SFX_MIXERS; i++)
    {
        CacheClearLine(&sfx_mixers[i].data);
        if (!sfx_mixers[i].data)
        {
            // found free mixer
            sfx_mixers[i].data = (const int8_t*)((uint32_t)data | 0x20000000);
            sfx_mixers[i].position = 0;
            sfx_mixers[i].increment = (srate << 14) / SAMPLE_RATE;
            sfx_mixers[i].length = length << 14;
            sfx_mixers[i].loop_length = loop_length << 14;
            sfx_mixers[i].volume = volume;
            sfx_mixers[i].pan = pan;
            UnlockMixer();

            return i;
        }
    }

    // didn't find a free mixer. Maybe the sound
    // was already playing. If so, restart it.
    for (i = 0; i < NUM_SFX_MIXERS; i++)
    {
        if (sfx_mixers[i].data == (const int8_t*)((uint32_t)data | 0x20000000))
        {
            // found same sfx - restart
            sfx_mixers[i].position = 0;
            sfx_mixers[i].increment = (srate << 14) / SAMPLE_RATE;
            sfx_mixers[i].length = length << 14;
            sfx_mixers[i].loop_length = loop_length << 14;
            sfx_mixers[i].volume = volume;
            sfx_mixers[i].pan = pan;
            UnlockMixer();

            return i;
        }
    }

    UnlockMixer();
    return INVALID_SOUND_HANDLE; // failed
}

void SoundManager_changeSoundEffectParams(SoundHandle soundHandle, int32_t srate, int32_t volume, int32_t pan)
{
    if (soundHandle > NUM_SFX_MIXERS - 1)
    {
        return;
    }

    LockMixer(MIXER_LOCK_MSH2);

    CacheClearLine(&sfx_mixers[soundHandle].data);
    if (sfx_mixers[soundHandle].data)
    {
        // still playing - update parameters
        if (srate != -1)
            sfx_mixers[soundHandle].increment = (srate << 14) / SAMPLE_RATE;
        if (volume != -1)
            sfx_mixers[soundHandle].volume = volume;
        if (volume != -1)
            sfx_mixers[soundHandle].pan = pan;
    }

    UnlockMixer();
}

void SoundManager_stopSoundEffect(SoundHandle soundHandle)
{
    if (soundHandle < NUM_SFX_MIXERS)
    {
        LockMixer(MIXER_LOCK_MSH2);

        CacheClearLine(&sfx_mixers[soundHandle].data);
        sfx_mixers[soundHandle].data = 0;

        UnlockMixer();
    }
}

uint8_t SoundManager_isSoundEffectPlaying(SoundHandle soundHandle)
{
    uint8_t res = 0;

    if (soundHandle < NUM_SFX_MIXERS)
    {
        LockMixer(MIXER_LOCK_MSH2);

        CacheClearLine(&sfx_mixers[soundHandle].data);
        if (sfx_mixers[soundHandle].data)
            res = 1;

        UnlockMixer();
    }

    return res;
}

uint8_t SoundManager_playSong(int songIndex)
{
    // lock the mixer while killing the old music
    LockMixer(MIXER_LOCK_MSH2);
    if (g_song)
    {
        xmp_stop_song(g_song);
        free(g_song);
        g_song = (song_t *)NULL;
    }
    UnlockMixer();

    Hw32xDelay(2); // allow the mixer to see the music is dead

    // lock the mixer while starting the new music
    LockMixer(MIXER_LOCK_MSH2);
    g_song = xmp_start_song(songIndex, 0);

    UnlockMixer();
    return g_song ? 1 : 0;
}

// pause: 0 - resume
// pause: 1 - pause
void SoundManager_pauseSong(uint8_t pause)
{
    pause ? xmp_pause(g_song, XMP_PAUSED) : xmp_pause(g_song, 0);
}

uint8_t SoundManager_isSongPlaying()
{
    return xmp_is_playing(g_song);
}

void SoundManager_shutdown()
{
    LockMixer(MIXER_LOCK_MSH2); // locked - stop playing
}

void SoundManager_updateDma()
{
    while (MARS_SYS_COMM6 == MIXER_LOCK_MSH2) ; // locked by MSH2

    SH2_DMA_CHCR1; // read TE
    SH2_DMA_CHCR1 = 0; // clear TE

    // send the current buffer to DMA and switch
    // to the other buffer to fill.
    if (g_currentSoundBuffer == g_soundBuffer0)
    {
        SH2_DMA_SAR1 = ((uint32_t)g_soundBuffer0) | 0x20000000;
        g_currentSoundBuffer = g_soundBuffer1;
    }
    else
    {
        SH2_DMA_SAR1 = ((uint32_t)g_soundBuffer1) | 0x20000000;
        g_currentSoundBuffer = g_soundBuffer0;
    }

    SH2_DMA_TCR1 = g_samplesCount; // number longs
    SH2_DMA_CHCR1 = 0x18E5; // dest fixed, src incr, size long, ext req, dack mem to dev, dack hi, dack edge, dreq rising edge, cycle-steal, dual addr, intr enabled, clear TE, dma enabled        

    update_sound_buffer(g_currentSoundBuffer);
}

// helper function for libxmp
void* xmp_malloc(int32_t size)
{
    // make sure cache line size aligned
    return (void*)memalign(16, size);
}
