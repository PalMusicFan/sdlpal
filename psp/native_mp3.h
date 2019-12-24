#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <pspaudio.h>
#include <pspmp3.h>
#include <psputility.h>

int ioread_err;

int playNativeMP3(const char* filename, int fLoop, int iMusicVolume);
int stopNativeMP3(void);
int initNativeMP3(void);
int shutdownNativeMP3(void);