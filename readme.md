This is my version of Chilly Willy's xmplayer for the 32X. 
As an exercise in understanding the project, I reworked it 
to match my current style of working.

It's based off of xmplayer-sfx, which is a cut down version of
the interrupt-based xmplayer-intr, with fewer music tracks.

- moved 32x and MD code to a source folder
- consolidated sound code to SoundManager.c/.h
- converted the mixer code to C and moved to mixer.c/.h
- moved the sound dma processing out of hw_32x.c and into SoundManager
- moved the secondary main function out of hw_32x.c
- moved the compilation artifacts to the out folder
- moved the 32x and MD makefiles to the build folder
- added helper batch files for buliding, cleaning, rebuilding, and deploying
- added the required libxmp library
- fixed bugs related to sound effect playback
- includes visual studio 2022 solution

