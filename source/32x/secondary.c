/*
 * Licensed under the BSD license
 *
 * secondary.c - functions for the second SH2.
 *
 */

#include "32x.h"
#include "sound_manager.h"


void secondary(void)
{
    SoundManager_init();

    while (1)
    {
        if (MARS_SYS_COMM4 == SSH2_WAITING)
            continue; // wait for command

        // do command in COMM4

        // done
        MARS_SYS_COMM4 = SSH2_WAITING;
    }
}
