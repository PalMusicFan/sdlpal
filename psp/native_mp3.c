/*
 * PSP Software Development Kit - http://www.pspdev.org
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * main.c - Sample to demonstrate proper use of the mp3 library
 *
 * Copyright (c) 2008 Alexander Berl <raphael@fx-world.org>
 *
 * $Id: $
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <pspaudio.h>
#include <pspmp3.h>
#include <psputility.h>

#include "native_mp3.h"

#include "../util.h"
#include "../audio.h"

int handle = -1;
int status = -1;
int isrunning = 0;
int monitor_isrunning = 0;
int monitor_lock = 0;

int paused = 0;

int isfLoop = -1;
int volume = PSP_AUDIO_VOLUME_MAX;

int fd = -1;
SceUID thid = 0;
SceUID monitor_thid;

int channel = -1;
int pos = 0;
int ioread = 0;
int iowrite = 0;

// ioread_err = 1 means fillStreamBuffer ioread error. 
int ioread_err = 0;

char MP3filename[PAL_MAX_PATH];
char monitor_MP3filename[PAL_MAX_PATH];
int current_floop;
int current_iMusicVolume;

char cwd_buff[PAL_MAX_PATH];

// Input and Output buffers
char	mp3Buf[16*1024]  __attribute__((aligned(64)));
short	pcmBuf[16*(1152/2)]  __attribute__((aligned(64)));


// Macro to allow formatted input without having to use stdargs.h
#define ERRORMSG(...) { char msg[128]; sprintf(msg,__VA_ARGS__); error(msg); }

// Print out an error message and quit after user input
void error( char* msg )
{
	pspDebugScreenClear();
	pspDebugScreenSetXY(0, 0);
	printf(msg);
}

void clearFileNameCache(void)
{
	memset(MP3filename, 0, PAL_MAX_PATH);
	memset(monitor_MP3filename, 0, PAL_MAX_PATH);
	UTIL_LogOutput(LOGLEVEL_DEBUG, "clearFileNameCache. Now monitor_MP3filename = %s \n", monitor_MP3filename);
}

int fillStreamBuffer( int fd, int handle )
{
	SceUChar8* dst;

	// Get Info on the stream (where to fill to, how much to fill, where to fill from)
	status = sceMp3GetInfoToAddStreamData( handle, &dst, &iowrite, &pos);
	if (status<0)
	{
		UTIL_LogOutput(LOGLEVEL_DEBUG, "sceMp3GetInfoToAddStreamData returned =  0x%x \n", status);
	}

	// Seek file to position requested
	status = sceIoLseek32( fd, pos, SEEK_SET );
	if (status > 0x80000000)
	{
		sceKernelDelayThread(1000000);
		UTIL_LogOutput(LOGLEVEL_WARNING, "sceIoLseek32 returned =  0x%x \n", status);
	}
	
	// Read the amount of data
	ioread = sceIoRead( fd, dst, iowrite );
	if (ioread > 0x80000000)
	{
		sceKernelDelayThread(1000000);
		ioread_err = 1;
		UTIL_LogOutput(LOGLEVEL_WARNING, "sceIoRead returned =  0x%x \n", status);
	}
	
	if (ioread == 0)
	{
		// End of file?
		return 0;
	}
	
	// Notify mp3 library about how much we really wrote to the stream buffer
	status = sceMp3NotifyAddStreamData( handle, ioread );
	if (status < 0)
	{
		UTIL_LogOutput(LOGLEVEL_WARNING, "sceMp3NotifyAddStreamData returned =  0x%x \n", status);
	}
	
	return (pos>0);
}

int MP3MonitorCallbackThread(void)
{
	while (monitor_isrunning)
	{
		if (ioread_err == 1 && monitor_lock == 0) {

			monitor_lock = 1;

			UTIL_LogOutput(LOGLEVEL_WARNING, "ioread_err DETECTED! monitor_lock = %d\n", monitor_lock);

			getcwd(cwd_buff, PAL_MAX_PATH);
			UTIL_LogOutput(LOGLEVEL_DEBUG, "getcwd returned =  %s \n", cwd_buff);
			sceIoChdir(cwd_buff);

			sceKernelDelayThread(50000);

			stopNativeMP3();

			UTIL_LogOutput(LOGLEVEL_DEBUG, "stopNativeMP3 \n");

			sceKernelDelayThread(50000);

			ioread_err = 0;

			// Clear the filename cache. 
			memset(monitor_MP3filename, 0, PAL_MAX_PATH);
			// Cache the filename. 
			strcpy(monitor_MP3filename, MP3filename);

			playNativeMP3(monitor_MP3filename, current_floop, current_iMusicVolume);

			UTIL_LogOutput(LOGLEVEL_DEBUG, "playNativeMP3 \n");

			monitor_lock = 0;
		}
		sceKernelDelayThread(1000000);
	}

	return 0;
}

int MP3DecodeCallbackThread(void)
{
	
	int samplingRate = sceMp3GetSamplingRate(handle);
	int numChannels = sceMp3GetMp3ChannelNum(handle);
	int lastDecoded = 0;
	int numPlayed = 0;
	
	channel = -1;

	while (isrunning)
	{
		/*
		sceDisplayWaitVblankStart();
		pspDebugScreenSetXY(0, 0);
		printf("PSP Mp3 Sample v1.0 by Raphael\n\n");
		printf("Playing '%s'...\n", MP3FILE);
		printf(" %i Hz\n", samplingRate);
		printf(" %i kbit/s\n", sceMp3GetBitRate(handle));
		printf(" %s\n", numChannels == 2 ? "Stereo" : "Mono");
		printf(" %s\n\n", loop == 0 ? "No loop" : "Loop");
		int playTime = samplingRate > 0 ? numPlayed / samplingRate : 0;
		printf(" Playtime: %02i:%02i\n", playTime / 60, playTime % 60);
		printf("\n\n\nPress CIRCLE to Pause/Resume playback\nPress TRIANGLE to reset playback\nPress CROSS to switch loop mode\nPress SQUARE to stop playback and quit\n");
		*/

		if (!paused)
		{
			// Check if we need to fill our stream buffer
			if (sceMp3CheckStreamDataNeeded(handle) > 0)
			{
				fillStreamBuffer(fd, handle);
			}

			// Decode some samples
			short* buf;
			int bytesDecoded;
			int retries = 0;
			// We retry in case it's just that we reached the end of the stream and need to loop
			for (; retries < 1; retries++)
			{
				bytesDecoded = sceMp3Decode(handle, &buf);
				if (bytesDecoded > 0)
					break;

				if (sceMp3CheckStreamDataNeeded(handle) <= 0)
					break;

				if (!fillStreamBuffer(fd, handle))
				{
					numPlayed = 0;
				}
			}
			if (bytesDecoded < 0 && bytesDecoded != 0x80671402)
			{
				// UTIL_LogOutput(LOGLEVEL_WARNING, "bytesDecoded =  0x%x \n", bytesDecoded);
				break;
			}

			// Nothing more to decode? Must have reached end of input buffer
			if (bytesDecoded == 0 || bytesDecoded == 0x80671402)
			{
				if (isfLoop == -1) {
					paused = 1;
					sceMp3ResetPlayPosition(handle);
					numPlayed = 0;
					paused = 0;
				}else {
					isrunning = 0;
				}
			}
			else
			{
				// Reserve the Audio channel for our output if not yet done
				if (channel < 0 || lastDecoded != bytesDecoded)
				{
					if (channel >= 0)
						sceAudioSRCChRelease();

					channel = sceAudioSRCChReserve(bytesDecoded / (2 * numChannels), samplingRate, numChannels);
				}
				if (AUDIO_MusicEnabled()){
					// Output the decoded samples and accumulate the number of played samples to get the playtime
					numPlayed += sceAudioSRCOutputBlocking(volume, buf);
				}
				else {
					numPlayed += sceAudioSRCOutputBlocking(0, buf);
				}
			}
		}
		sceKernelDelayThread(10000);
	}
	return 1;
}

/* main routine */
int initNativeMP3(void)
{
	monitor_thid = sceKernelCreateThread("monitor_thread", (SceKernelThreadEntry)MP3MonitorCallbackThread, 0x11, 0xFA0, 0, 0);
	if (monitor_thid > 0)
	{
		monitor_isrunning = 1;
		sceKernelStartThread(monitor_thid, 0, 0);
		return 1;
	}
	else {
		return 0;
	}
}

int playNativeMP3(const char* filename, int fLoop, int iMusicVolume)
{
	// Open the input file

	UTIL_LogOutput(LOGLEVEL_DEBUG, "filename =  %s \n", filename);

	current_floop = fLoop;
	current_iMusicVolume = iMusicVolume;

	if (filename) {
		// Clear the filename cache. 
		memset(MP3filename, 0, PAL_MAX_PATH);
		// Cache the filename. 
		strcpy(MP3filename, filename);
	}

	UTIL_LogOutput(LOGLEVEL_DEBUG, "MP3filename =  %s \n", MP3filename);

	fd = sceIoOpen(MP3filename, PSP_O_RDONLY, 0777);

	UTIL_LogOutput(LOGLEVEL_DEBUG, "sceIoOpen returns 0x%x \n", fd);

	if (fd <= 0 || fd > 63)
	{
		// sceIoOpen returns 0x80010002 if no such file found. 
		//ERRORMSG("ERROR: Could not open file '%s' - 0x%08X\n", MP3filename, fd);
		return 0;
	}

	// Init mp3 resources
	status = sceMp3InitResource();
	if (status < 0)
	{
		//ERRORMSG("ERROR: sceMp3InitResource returned 0x%08X\n", status);
		return 0;
	}
	// Reserve a mp3 handle for our playback
	SceMp3InitArg mp3Init;
	mp3Init.mp3StreamStart = 0;
	mp3Init.mp3StreamEnd = sceIoLseek32(fd, 0, SEEK_END);
	mp3Init.unk1 = 0;
	mp3Init.unk2 = 0;
	mp3Init.mp3Buf = mp3Buf;
	mp3Init.mp3BufSize = sizeof(mp3Buf);
	mp3Init.pcmBuf = pcmBuf;
	mp3Init.pcmBufSize = sizeof(pcmBuf);

	handle = sceMp3ReserveMp3Handle(&mp3Init);
	if (handle < 0)
	{
		// ERRORMSG("ERROR: sceMp3ReserveMp3Handle returned 0x%08X\n", handle);
		UTIL_LogOutput(LOGLEVEL_WARNING, "sceMp3ReserveMp3Handle returns 0x%x \n", status);
		return 0;
	}

	// Loop?
	if (fLoop == 1)
	{
		isfLoop = -1;
	}
	else {
		isfLoop = 0;
	}
	status = sceMp3SetLoopNum(handle, isfLoop);
	if (status < 0)
	{
		// ERRORMSG("ERROR: sceMp3SetLoopNum returned 0x%08X\n", status);
		UTIL_LogOutput(LOGLEVEL_WARNING, "sceMp3ReserveMp3Handle returns 0x%x \n", handle);
	}

	UTIL_LogOutput(LOGLEVEL_VERBOSE, "iMusicVolume =  %d \n", iMusicVolume);
	volume = PSP_AUDIO_VOLUME_MAX / 100 * iMusicVolume;

	// Fill the stream buffer with some data so that sceMp3Init has something to work with
	fillStreamBuffer(fd, handle);

	status = sceMp3Init(handle);
	if (status < 0)
	{
		// ERRORMSG("ERROR: sceMp3Init returned 0x%08X\n", status);
		UTIL_LogOutput(LOGLEVEL_WARNING, "sceMp3Init returns 0x%x \n", status);
	}

	thid = sceKernelCreateThread("update_thread", (SceKernelThreadEntry)MP3DecodeCallbackThread, 0x11, 0xFA0, 0, 0);
	if (thid > 0)
	{
		isrunning = 1;
		sceKernelStartThread(thid, 0, 0);
		return fd;
	}
	else {
		return 0;
	}

}

int stopNativeMP3(void)
{
	isrunning = 0;

	sceKernelWaitThreadEnd(thid, NULL);
	sceKernelDeleteThread(thid);

	// Cleanup time...
	if (channel >= 0)
		sceAudioSRCChRelease();

	status = sceMp3ReleaseMp3Handle(handle);
	if (status < 0)
	{
		ERRORMSG("ERROR: sceMp3ReleaseMp3Handle returned 0x%08X\n", status);
	}

	status = sceMp3TermResource();
	if (status < 0)
	{
		ERRORMSG("ERROR: sceMp3TermResource returned 0x%08X\n", status);
	}
	status = sceIoClose(fd);
	if (status < 0)
	{
		ERRORMSG("ERROR: sceIoClose returned 0x%08X\n", status);
	}
	return 1;
	
}


int shutdownNativeMP3(void)
{
	monitor_isrunning = 0;

	sceKernelWaitThreadEnd(monitor_thid, NULL);
	sceKernelDeleteThread(monitor_thid);

	return 1;
}
