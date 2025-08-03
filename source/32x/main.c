// XM Test for 32X - original version by Chilly Willy
// This modified version by pw_32x

#include <sys/types.h>
#include <string.h>
#include <malloc.h>

#include "32x.h"
#include "hw_32x.h"
#include "music/modules.h"
#include "mods.h"
#include "sound_manager.h"
#include "files.h"

void play_song_helper(int index)
{
    if (!SoundManager_playSong(index))
    {
        Hw32xScreenPuts("  Error starting: ");
        Hw32xScreenPuts(titles[index]);
        Hw32xScreenPuts("\n");
    }
    else
    {
        char temp[80];
        sprintf(temp, "  %s\n", titles[index]);
        Hw32xScreenPuts(temp);
    }
}

unsigned short new_buttons = 0;
unsigned short curr_buttons = 0;
unsigned short buttons = 0;

void controller_update()
{
    // MARS_SYS_COMM8 holds the current button values: - - - - M X Y Z S A C B R L D U
    curr_buttons = MARS_SYS_COMM8;

    new_buttons = curr_buttons ^ buttons; // set if button changed
    buttons = curr_buttons;
}

unsigned char controller_button_released(unsigned short buttonFlag)
{
    return (new_buttons & buttonFlag) && (!(buttons & buttonFlag));
}

// note - we don't care about locking the mixer to just change the volume, pause, resume, or check if playing
int main ( void )
{
    unsigned short songPaused = 0;
    short curr_mod = 0;
    SoundHandle rainSoundHandle = INVALID_SOUND_HANDLE;
    SoundHandle hitSoundHandle = INVALID_SOUND_HANDLE;
    SoundHandle oofSoundHandle = INVALID_SOUND_HANDLE;

    Hw32xInit(MARS_VDP_MODE_256);
    Hw32xSetBGColor(1,0,0,0); // bg = black
    Hw32xScreenPuts("  XM Test\n");

    SoundManager_waitUntilInitialized();

    play_song_helper(0); // start playing the first song

    while (1)
    {
        Hw32xDelay(2);

        controller_update();

        if (controller_button_released(SEGA_CTRL_START))
        {
            if (songPaused)
            {
                Hw32xScreenPuts("  Resuming song\n");
            }
            else
            {
                Hw32xScreenPuts("  Pausing song\n");
            }
            
            songPaused = !songPaused;
            SoundManager_pauseSong(songPaused);
        }

        if (controller_button_released(SEGA_CTRL_DOWN))
        {
            curr_mod++;
            if (curr_mod >= NUM_MODS)
                curr_mod = 0;
            play_song_helper(curr_mod);
        }

        if (controller_button_released(SEGA_CTRL_UP))
        {
            curr_mod--;
            if (curr_mod < 0)
                curr_mod = NUM_MODS - 1;
            play_song_helper(curr_mod);
        }

        if (controller_button_released(SEGA_CTRL_RIGHT))
        {
            if (g_masterVolume < g_maxMasterVolume)
                g_masterVolume++;
        }

        if (controller_button_released(SEGA_CTRL_LEFT))
        {
            if (g_masterVolume > 0)
                g_masterVolume--;
        }

        if (controller_button_released(SEGA_CTRL_A))
        {
            if (SoundManager_isSoundEffectPlaying(rainSoundHandle))
            {
                SoundManager_stopSoundEffect(rainSoundHandle);
                rainSoundHandle = INVALID_SOUND_HANDLE;
            }
            else
            {
                rainSoundHandle = SoundManager_playSoundEffect((int8_t *)filePtr[0], 16000, fileSize[0] - 1, fileSize[0], 64, 128);
            }

            char temp[80];
            sprintf(temp, "rain %d\n", rainSoundHandle);
            Hw32xScreenPuts(temp);
        }

        if (controller_button_released(SEGA_CTRL_B))
        {
            hitSoundHandle = SoundManager_playSoundEffect((int8_t *)filePtr[1], 16000, 0, fileSize[1], 64, 128);            
            char temp[80];
            sprintf(temp, "hit %d\n", hitSoundHandle);
            Hw32xScreenPuts(temp);
        }

        if (controller_button_released(SEGA_CTRL_C))
        {
            oofSoundHandle = SoundManager_playSoundEffect((int8_t *)filePtr[2], 16000, 0, fileSize[2], 64, 128);
            char temp[80];
            sprintf(temp, "off %d\n", oofSoundHandle);
            Hw32xScreenPuts(temp);
        }

        SoundManager_updateMain();

        if (!songPaused && !SoundManager_isSongPlaying())
        {
            // start next module
            curr_mod++;
            if (curr_mod >= NUM_MODS)
                curr_mod = 0;
            play_song_helper(curr_mod);
        }
    }

    SoundManager_shutdown();

    return 0;
}