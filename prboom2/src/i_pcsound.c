// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// DESCRIPTION:
//	System interface for PC speaker sound.
//
//-----------------------------------------------------------------------------

#include "SDL.h"

#include "doomdef.h"
#include "doomtype.h"

#include "i_pcsound.h"
#include "i_sound.h"
#include "sounds.h"

#include "w_wad.h"

#include "PCSOUND/pcsound.h"

static dboolean pcs_initialised = false;

static SDL_mutex *sound_lock;

static const uint8_t *current_sound_lump = NULL;
static const uint8_t *current_sound_pos = NULL;
static unsigned int current_sound_remaining = 0;
static int current_sound_handle = 0;

static float frequencies[] = {
    0.0f, 175.00f, 180.02f, 185.01f, 190.02f, 196.02f, 202.02f, 208.01f, 214.02f, 220.02f,
    226.02f, 233.04f, 240.02f, 247.03f, 254.03f, 262.00f, 269.03f, 277.03f, 285.04f,
    294.03f, 302.07f, 311.04f, 320.05f, 330.06f, 339.06f, 349.08f, 359.06f, 370.09f,
    381.08f, 392.10f, 403.10f, 415.01f, 427.05f, 440.12f, 453.16f, 466.08f, 480.15f,
    494.07f, 508.16f, 523.09f, 539.16f, 554.19f, 571.17f, 587.19f, 604.14f, 622.09f,
    640.11f, 659.21f, 679.10f, 698.17f, 719.21f, 740.18f, 762.41f, 784.47f, 807.29f,
    831.48f, 855.32f, 880.57f, 906.67f, 932.17f, 960.69f, 988.55f, 1017.20f, 1046.64f,
    1077.85f, 1109.93f, 1141.79f, 1175.54f, 1210.12f, 1244.19f, 1281.61f, 1318.43f,
    1357.42f, 1397.16f, 1439.30f, 1480.37f, 1523.85f, 1569.97f, 1614.58f, 1661.81f,
    1711.87f, 1762.45f, 1813.34f, 1864.34f, 1921.38f, 1975.46f, 2036.14f, 2093.29f,
    2157.64f, 2217.80f, 2285.78f, 2353.41f, 2420.24f, 2490.98f, 2565.97f, 2639.77f,
};

#define NUM_FREQUENCIES (sizeof(frequencies) / sizeof(*frequencies))

void PCSCallbackFunc(int *duration, int *freq)
{
    int tone;

    *duration = 1000 / 140;

    if (SDL_LockMutex(sound_lock) < 0)
    {
        *freq = 0;
        return;
    }

    if (current_sound_lump != NULL && current_sound_remaining > 0)
    {
        // Read the next tone

        tone = *current_sound_pos;

        // Use the tone -> frequency lookup table.  See pcspkr10.zip
        // for a full discussion of this.
        // Check we don't overflow the frequency table.

        if (tone < (int)NUM_FREQUENCIES)
        {
            *freq = (int) frequencies[tone];
        }
        else
        {
            *freq = 0;
        }

        ++current_sound_pos;
        --current_sound_remaining;
    }
    else
    {
        *freq = 0;
    }

    SDL_UnlockMutex(sound_lock);
}

static dboolean CachePCSLump(int sound_id)
{
    int lumplen;
    int headerlen;

    // Load from WAD

    current_sound_lump = W_LumpByNum(S_sfx[sound_id].lumpnum);
    lumplen = W_LumpLength(S_sfx[sound_id].lumpnum);

    // Read header

    if (current_sound_lump[0] != 0x00 || current_sound_lump[1] != 0x00)
    {
        return false;
    }

    headerlen = (current_sound_lump[3] << 8) | current_sound_lump[2];

    if (headerlen > lumplen - 4)
    {
        return false;
    }

    // Header checks out ok

    current_sound_remaining = headerlen;
    current_sound_pos = current_sound_lump + 4;

    return true;
}

int I_PCS_StartSound(int id,
                     int channel,
                     int vol,
                     int sep,
                     int pitch,
                     int priority)
{
    int result;

    if (!pcs_initialised)
    {
        return -1;
    }

    // These PC speaker sounds are not played - this can be seen in the
    // Heretic source code, where there are remnants of this left over
    // from Doom.

    if (id == sfx_posact || id == sfx_bgact || id == sfx_dmact
     || id == sfx_dmpain || id == sfx_popain || id == sfx_sawidl)
    {
        return -1;
    }

    if (SDL_LockMutex(sound_lock) < 0)
    {
        return -1;
    }

    result = CachePCSLump(id);

    if (result)
    {
        current_sound_handle = channel;
    }

    SDL_UnlockMutex(sound_lock);

    if (result)
    {
        return channel;
    }
    else
    {
        return -1;
    }
}

void I_PCS_StopSound(int handle)
{
    if (!pcs_initialised)
    {
        return;
    }

    if (SDL_LockMutex(sound_lock) < 0)
    {
        return;
    }

    // If this is the channel currently playing, immediately end it.

    if (current_sound_handle == handle)
    {
        current_sound_remaining = 0;
    }

    SDL_UnlockMutex(sound_lock);
}

int I_PCS_SoundIsPlaying(int handle)
{
    if (!pcs_initialised)
    {
        return false;
    }

    if (handle != current_sound_handle)
    {
        return false;
    }

    return current_sound_lump != NULL && current_sound_remaining > 0;
}

void I_PCS_InitSound(void)
{
    pcs_initialised = PCSound_Init(PCSCallbackFunc);

    sound_lock = SDL_CreateMutex();
}
