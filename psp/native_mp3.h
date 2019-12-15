#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <pspaudio.h>
#include <pspmp3.h>
#include <psputility.h>

int playNativeMP3(const char* filename, int fLoop);
int stopNativeMP3(VOID);
int initNativeMP3(VOID);
int shutdownNativeMP3(VOID);