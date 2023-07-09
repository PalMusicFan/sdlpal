/* -*- mode: c; tab-width: 4; c-basic-offset: 4; c-file-style: "linux" -*- */
//
// Copyright (c) 2009-2011, Wei Mingzhi <whistler_wmz@users.sf.net>.
// Copyright (c) 2011-2023, SDLPAL development team.
// All rights reserved.
//
// This file is part of SDLPAL.
//
// SDLPAL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// opusplay.c: Player for opus files.
//   @Author: Soar Qin <soarchin@gmail.com>, 2019-12-13.
//

#include "util.h"
#include "global.h"
#include "palcfg.h"
#include "players.h"
#include "audio.h"
#include <math.h>

#if PAL_HAS_OPUS
#include "resampler.h"
#include "opusfile.h"

enum {
    opus_sample_buffer_count = 120*48*2,
};

typedef struct tagOPUSPLAYER
{
    AUDIOPLAYER_COMMONS;

    OggOpusFile     *fp;
    opus_int16       sBuffer[opus_sample_buffer_count];
    int              iBufPos, iBufLen;

    void            *resampler[2];
    int              iLink;
    int              fReady;
    int              fRewind;
    int              fUseResampler;

    INT                        iNextMusic; // the next music number to switch to
    DWORD                      dwStartFadeTime;
    INT                        iTotalFadeOutSamples;
    INT                        iTotalFadeInSamples;
    INT                        iRemainingFadeSamples;
    enum { NONE, FADE_IN, FADE_OUT } FadeType; // fade in or fade out ?
    BOOL                       fNextLoop;
} OPUSPLAYER, *LPOPUSPLAYER;

PAL_FORCE_INLINE opus_int16 OPUS_GetSample(float pcm)
{
    int val = (int)(floor(pcm * 32767.f + .5f));
    /* might as well guard against clipping */
    if (val > 32767) {
        val = 32767;
    }
    else if (val < -32768) {
        val = -32768;
    }
    return (opus_int16)val;
}

PAL_FORCE_INLINE void OPUS_FillResample(LPOPUSPLAYER player, opus_int16* stream, int count)
{
    int i;
    if (gConfig.iAudioChannels == 2) {
        for (i = count; i; --i) {
            *stream++ = resampler_get_and_remove_sample(player->resampler[0]);
            *stream++ = resampler_get_and_remove_sample(player->resampler[1]);
        }
    }
    else {
        for (i = count; i; --i) {
            *stream++ = (short)((int)(resampler_get_and_remove_sample(player->resampler[0])
                + resampler_get_and_remove_sample(player->resampler[1])) >> 1);
        }
    }
}

static void OPUS_Cleanup(LPOPUSPLAYER player)
{
    int i;
    for (i = 0; i < gConfig.iAudioChannels; i++) resampler_clear(player->resampler[0]);
    player->iBufPos = player->iBufLen = 0;
    player->iLink = -1;
    player->fReady = FALSE;
    player->fRewind = FALSE;
}


static BOOL OPUS_Rewind(LPOPUSPLAYER player)
{
    OPUS_Cleanup(player);
    op_raw_seek(player->fp, 0);
    player->iLink = op_current_link(player->fp);
    player->fUseResampler = 48000 != gConfig.iSampleRate;
    if (player->fUseResampler) {
        int i;
        double factor = 48000. / (double)gConfig.iSampleRate;
        for (i = 0; i < 2; i++)
        {
            resampler_set_quality(player->resampler[i], AUDIO_IsIntegerConversion(48000) ? RESAMPLER_QUALITY_MIN : gConfig.iResampleQuality);
            resampler_set_rate(player->resampler[i], factor);
            resampler_clear(player->resampler[i]);
        }
    }
    player->fRewind = FALSE;
    return (player->fReady = TRUE);
}

static int
OPUS_REAL_FillBuffer(
    VOID       *object,
    LPBYTE      stream,
    INT         len
)
{
    int total_bytes;
    int bytes_per_sample;
    LPOPUSPLAYER player = (LPOPUSPLAYER)object;
    if (!player->fReady) {
        return -1;
    }
    if (!player->fp) {
        return -1;
    }

    total_bytes = 0;
    bytes_per_sample = gConfig.iAudioChannels * sizeof(opus_int16);
    while (total_bytes < len) {
        opus_int16 *samples;
        if (!player->fRewind && player->iBufLen == 0) {
            int read_count = op_read_stereo(player->fp, player->sBuffer, opus_sample_buffer_count);
            if (read_count == 0) {
                if (player->fLoop)
                    player->fRewind = TRUE;
                else
                    return -2;
            } else if (read_count < 0) {
                if (read_count == OP_HOLE) {
                    /* Hole detected! Corrupt file segment? */
                    continue;
                }
                /* Stop playing on other errors */
                player->fReady = FALSE;
                return -1;
            } else {
                player->iBufLen = read_count * 2;
            }
        }
        if (player->fUseResampler) {
            int i;
            int resampler_count;
            int fill_count;
            int buf_count = player->iBufLen - player->iBufPos;
            int to_write = resampler_get_free_count(player->resampler[0])*2;
            if (buf_count > to_write) {
                buf_count = to_write;
            }
            samples = player->sBuffer + player->iBufPos;
            for (i = buf_count; i; i -= 2) {
                resampler_write_sample(player->resampler[0], *samples++);
                resampler_write_sample(player->resampler[1], *samples++);
            }
            player->iBufPos += buf_count;
            if (player->iBufPos == player->iBufLen) {
                player->iBufPos = player->iBufLen = 0;
            }
            resampler_count = resampler_get_sample_count(player->resampler[0]);
            if (resampler_count==0) {
                if (player->fRewind) {
                    OPUS_Rewind(player);
                }
                continue;
            }
            fill_count = (len - total_bytes) / bytes_per_sample;
            if (fill_count > resampler_count) fill_count = resampler_count;
            OPUS_FillResample(player, (opus_int16 *)(stream + total_bytes), fill_count);
            total_bytes += fill_count * bytes_per_sample;
        } else {
            if (player->fRewind) {
                OPUS_Rewind(player);
                continue;
            }
            int i;
            opus_int16 *ptr = (opus_int16 *)(stream + total_bytes);
            opus_int16 *inptr = player->sBuffer + player->iBufPos;
            int buf_count = (player->iBufLen - player->iBufPos) / 2;
            int out_count = (len - total_bytes) / bytes_per_sample;
            int stereo = gConfig.iAudioChannels > 1;
            if (out_count < buf_count) {
                player->iBufPos += out_count * 2;
            } else {
                out_count = buf_count;
                player->iBufPos = player->iBufLen = 0;
            }
            if (stereo) {
                memcpy(ptr, inptr, out_count * bytes_per_sample);
            } else {
                for (i = out_count; i; i--) {
                    *ptr++ = (opus_int16)(((int32_t)inptr[0] + (int32_t)inptr[1])) / 2;
                    inptr += 2;
                }
            }
            total_bytes += out_count * bytes_per_sample;
        }
    }
    return 0;
}

static VOID
OPUS_FillBuffer(
    VOID* object,
    LPBYTE     stream,
    INT        len
)
/*++
    Purpose:

    Fill the background music into the sound buffer. Called by the SDL sound
    callback function only (audio.c: AUDIO_FillBuffer).

    Parameters:

    [OUT] stream - pointer to the stream buffer.

    [IN]  len - Length of the buffer.

    Return value:

    None.

--*/
{
    LPOPUSPLAYER pOPUSPlayer = (LPOPUSPLAYER)object;

    if (pOPUSPlayer == NULL || !pOPUSPlayer->fReady)
    {
        //
        // Not initialized
        //
        return;
    }

    INT       volume, delta_samples = 0, vol_delta = 0;

    //
    // fading in or fading out
    //
    switch (pOPUSPlayer->FadeType)
    {
    case FADE_IN:
        if (pOPUSPlayer->iRemainingFadeSamples <= 0)
        {
            pOPUSPlayer->FadeType = NONE;
            volume = SDL_MIX_MAXVOLUME;
        }
        else
        {
            volume = (INT)(SDL_MIX_MAXVOLUME * (1.0 - (double)pOPUSPlayer->iRemainingFadeSamples / pOPUSPlayer->iTotalFadeInSamples));
            delta_samples = (pOPUSPlayer->iTotalFadeInSamples / SDL_MIX_MAXVOLUME) & ~(gConfig.iAudioChannels - 1); vol_delta = 1;
        }
        break;
    case FADE_OUT:
        if (pOPUSPlayer->iTotalFadeOutSamples == pOPUSPlayer->iRemainingFadeSamples && pOPUSPlayer->iTotalFadeOutSamples > 0)
        {
            UINT  now = SDL_GetTicks();
            INT   passed_samples = ((INT)(now - pOPUSPlayer->dwStartFadeTime) > 0) ? (INT)((now - pOPUSPlayer->dwStartFadeTime) * AUDIO_GetDeviceSpec()->freq / 1000) : 0;
            pOPUSPlayer->iRemainingFadeSamples -= passed_samples;
        }
        if (pOPUSPlayer->iMusic == -1 || pOPUSPlayer->iRemainingFadeSamples <= 0)
        {
            //
            // There is no current playing music, or fading time has passed.
            // Start playing the next one or stop playing.
            //
            if (pOPUSPlayer->iNextMusic > 0)
            {
                OPUS_REAL_Play(object, pOPUSPlayer->iNextMusic, pOPUSPlayer->fNextLoop, 0);
                pOPUSPlayer->iMusic = pOPUSPlayer->iNextMusic;
                pOPUSPlayer->iNextMusic = -1;
                pOPUSPlayer->fLoop = pOPUSPlayer->fNextLoop;
                pOPUSPlayer->FadeType = FADE_IN;
                if (pOPUSPlayer->iMusic > 0)
                    pOPUSPlayer->dwStartFadeTime += pOPUSPlayer->iTotalFadeOutSamples * 1000 / gConfig.iSampleRate;
                else
                    pOPUSPlayer->dwStartFadeTime = SDL_GetTicks();
                pOPUSPlayer->iTotalFadeOutSamples = 0;
                pOPUSPlayer->iRemainingFadeSamples = pOPUSPlayer->iTotalFadeInSamples;
                if (pOPUSPlayer->resampler[0]) resampler_clear(pOPUSPlayer->resampler[0]);
                if (pOPUSPlayer->resampler[1]) resampler_clear(pOPUSPlayer->resampler[1]);
                return;
            }
            else
            {
                OPUS_REAL_Play(object, pOPUSPlayer->iNextMusic, pOPUSPlayer->fNextLoop, 0);
                pOPUSPlayer->iMusic = -1;
                pOPUSPlayer->FadeType = NONE;
                return;
            }
        }
        else
        {
            volume = (INT)(SDL_MIX_MAXVOLUME * ((double)pOPUSPlayer->iRemainingFadeSamples / pOPUSPlayer->iTotalFadeOutSamples));
            delta_samples = (pOPUSPlayer->iTotalFadeOutSamples / SDL_MIX_MAXVOLUME) & ~(gConfig.iAudioChannels - 1); vol_delta = -1;
        }
        break;
    default:
        if (pOPUSPlayer->iMusic <= 0)
        {
            //
            // No current playing music
            //
            return;
        }
        else
        {
            volume = SDL_MIX_MAXVOLUME;
        }
    }

    //
    // Fill the buffer with sound data
    //
    LPBYTE local_stream = (LPBYTE)malloc(len);
    memset(local_stream, 0, len);
    int ret = OPUS_REAL_FillBuffer(object, local_stream, len);
    if (ret == -1)
        return;

    if (ret == -2)
    {
        //
        // Not loop, simply terminate the music
        //
        OPUS_REAL_Play(object, pOPUSPlayer->iMusic, pOPUSPlayer->fLoop, 0);
        pOPUSPlayer->iMusic = -1;
        if (pOPUSPlayer->FadeType != FADE_OUT && pOPUSPlayer->iNextMusic == -1)
            pOPUSPlayer->FadeType = NONE;
        return;
    }
    //
    // Put audio data into buffer and adjust volume
    //
    memcpy(stream, local_stream, len);
    if (pOPUSPlayer->FadeType != NONE)
    {
        short* ptr_l = (short*)local_stream;
        short* ptr_r = (short*)stream;
        for (int i = 0; i < len/2 && pOPUSPlayer->iRemainingFadeSamples > 0; volume += vol_delta)
        {
            int j = 0;
            volume = volume < 0 ? 0 : volume;
            volume = volume > SDL_MIX_MAXVOLUME ? SDL_MIX_MAXVOLUME : volume;
            for (j = 0; i < len/2 && j < delta_samples; i++, j++)
            {
                *ptr_r++ = *ptr_l++ * volume / SDL_MIX_MAXVOLUME;
            }
            pOPUSPlayer->iRemainingFadeSamples -= j;
        }
        while (ptr_r < stream+len)
            *ptr_r++ = volume;
    }

    free(local_stream);
}

static BOOL
OPUS_REAL_Play(
    VOID       *object,
    INT         iNum,
    BOOL        fLoop,
    FLOAT       flFadeTime
)
{
    LPOPUSPLAYER player = (LPOPUSPLAYER)object;
    static char internal_buffer[PAL_GLOBAL_BUFFER_SIZE];

    int ret;

    if (player == NULL)
    {
        return FALSE;
    }

    player->fLoop = fLoop;

    if (iNum == player->iMusic)
    {
        return TRUE;
    }

    player->fReady = FALSE;
    OPUS_Cleanup(player);
    if (player->fp)
    {
        op_free(player->fp);
        player->fp = NULL;
    }

    player->iMusic = iNum;

    if (iNum == -1)
    {
        return TRUE;
    }

    if (iNum == 0)
    {
        return FALSE;
    }

    const char* filename = UTIL_GetFullPathName(internal_buffer, PAL_GLOBAL_BUFFER_SIZE, gConfig.pszGamePath, PAL_va(2, "opus%s%.2d.opus", PAL_NATIVE_PATH_SEPARATOR, iNum));
    if (!filename)
        return FALSE;

    player->fp = op_open_file(filename, &ret);
    if (player->fp == NULL)
    {
        player->fReady = FALSE;
        return FALSE;
    }

    if (!OPUS_Rewind(player))
    {
        op_free(player->fp);
        player->fp = NULL;
        return FALSE;
    }

    return TRUE;
}

static BOOL
OPUS_Play(
    VOID* object,
    INT       iNum,
    BOOL      fLoop,
    FLOAT     flFadeTime
)
/*++
    Purpose:

    Start playing the specified music.

    Parameters:

    [IN]  iNum - number of the music. 0 to stop playing current music.

    [IN]  fLoop - Whether the music should be looped or not.

    [IN]  flFadeTime - the fade in/out time when switching music.

    Return value:

    None.

--*/
{
    LPOPUSPLAYER pOPUSPlayer = (LPOPUSPLAYER)object;

    //
    // Check for NULL pointer.
    //
    if (pOPUSPlayer == NULL)
    {
        return FALSE;
    }

    if (iNum == pOPUSPlayer->iMusic && pOPUSPlayer->iNextMusic == -1)
    {
        /* Will play the same music without any pending play changes,
           just change the loop attribute */
        pOPUSPlayer->fLoop = fLoop;
        return TRUE;
    }

    if (pOPUSPlayer->FadeType != FADE_OUT)
    {
        if (pOPUSPlayer->FadeType == FADE_IN && pOPUSPlayer->iTotalFadeInSamples > 0 && pOPUSPlayer->iRemainingFadeSamples > 0)
        {
            pOPUSPlayer->dwStartFadeTime = SDL_GetTicks() - (int)((float)pOPUSPlayer->iRemainingFadeSamples / pOPUSPlayer->iTotalFadeInSamples * flFadeTime * (1000 / 2));
        }
        else
        {
            pOPUSPlayer->dwStartFadeTime = SDL_GetTicks();
        }
        pOPUSPlayer->iTotalFadeOutSamples = (int)round(flFadeTime / 2.0f * gConfig.iSampleRate) * gConfig.iAudioChannels;
        pOPUSPlayer->iRemainingFadeSamples = pOPUSPlayer->iTotalFadeOutSamples;
        pOPUSPlayer->iTotalFadeInSamples = pOPUSPlayer->iTotalFadeOutSamples;
    }
    else
    {
        pOPUSPlayer->iTotalFadeInSamples = (int)round(flFadeTime / 2.0f * gConfig.iSampleRate) * gConfig.iAudioChannels;
    }

    pOPUSPlayer->iNextMusic = iNum;
    pOPUSPlayer->FadeType = FADE_OUT;
    pOPUSPlayer->fNextLoop = fLoop;
    pOPUSPlayer->fReady = TRUE;

    return TRUE;
}

// Dub player HERE!
static BOOL
Dub_Play(
	VOID* object,
	INT       iSid,
	INT       iEid,
	INT       iSeg,
	FLOAT     flFadeTime
)
/*++
	Purpose:

	Start playing the specified dub.

	Parameters:

	[IN]  iSid - Start ID of the dub file.

	[IN]  iEid - End ID of the dub file.

	[IN]  iSeg - Sub segment ID of the dub file.

	[IN]  flFadeTime - the fade in/out time when switching dub.

	Return value:

	None.

--*/
{
	LPOPUSPLAYER player = (LPOPUSPLAYER)object;
	static char internal_buffer[PAL_GLOBAL_BUFFER_SIZE];
	UTIL_LogOutput(LOGLEVEL_DEBUG, "[DUB] Dub_Play =  iSid-%.5d, iEid-%.5d, iSeg-%.5d\n", iSid, iEid, iSeg);

	// Do NOTHING if there is NO corresponding dub file.
	if (access(UTIL_GetFullPathName(internal_buffer, PAL_GLOBAL_BUFFER_SIZE, gConfig.pszGamePath, PAL_va(2, "opus%s%.5d-%.5d-%.2d.opus", iSid, iEid, iSeg)), 0) < 0)
	{
		UTIL_LogOutput(LOGLEVEL_DEBUG, "[DUB] FILE NOT FOUND\n");
		return FALSE;
	}

	int ret;

	if (player == NULL)
	{
		return FALSE;
	}

	player->fReady = FALSE;
	OPUS_Cleanup(player);
	if (player->fp)
	{
		op_free(player->fp);
		player->fp = NULL;
	}

	player->iMusic = iSid;

	if (iSid == -1)
	{
		return TRUE;
	}


	const char* filename = UTIL_GetFullPathName(internal_buffer, PAL_GLOBAL_BUFFER_SIZE, gConfig.pszGamePath, PAL_va(2, "opus%s%.5d-%.5d-%.2d.opus", iSid, iEid, iSeg));
	if (!filename)
		return FALSE;

	player->fp = op_open_file(filename, &ret);
	UTIL_LogOutput(LOGLEVEL_DEBUG, "[DUB] op_open_file =  iSid-%.5d, iEid-%.5d, iSeg-%.5d\n", iSid, iEid, iSeg);
	if (player->fp == NULL)
	{
		player->fReady = FALSE;
		return FALSE;
	}

	if (!OPUS_Rewind(player))
	{
		op_free(player->fp);
		player->fp = NULL;
		return FALSE;
	}

	return TRUE;
}

static VOID
OPUS_Shutdown(
    VOID       *object
)
{
    if (object)
    {
        LPOPUSPLAYER player = (LPOPUSPLAYER)object;
        OPUS_Cleanup(player);
        resampler_delete(player->resampler[0]);
        resampler_delete(player->resampler[1]);
        op_free(player->fp);
        free(player);
    }
}

LPAUDIOPLAYER
OPUS_Init(
    VOID
)
{
    LPOPUSPLAYER player;
    if ((player = (LPOPUSPLAYER)malloc(sizeof(OPUSPLAYER))) != NULL)
    {
        memset(player, 0, sizeof(OPUSPLAYER));

        player->FillBuffer = OPUS_FillBuffer;
        player->Play = OPUS_Play;
        player->Shutdown = OPUS_Shutdown;

        player->iLink = -1;
        player->iMusic = -1;

        player->resampler[0] = resampler_create();
        if (player->resampler[0])
        {
            player->resampler[1] = resampler_create();
            if (player->resampler[1] == NULL)
            {
                resampler_delete(player->resampler[0]);
                player->resampler[0] = NULL;
            }
        }
    }
    return (LPAUDIOPLAYER)player;
}

#else

LPAUDIOPLAYER
OPUS_Init(
	VOID
)
{
	return NULL;
}

#endif
