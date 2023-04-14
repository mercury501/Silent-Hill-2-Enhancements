/**
* Copyright (C) 2023 Gemini
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

#pragma once

#ifdef _NVWA
#include "..\nvwa\debug_new.h"
#endif

#include <windows.h>
#include <dsound.h>
#include <vector>
#include "ffmpeg.h"
#include "bink_int.h"

typedef unsigned int U32;
typedef unsigned char U8;
typedef signed int S32;
typedef signed char S8;

typedef void(__stdcall *BINKSNDSYSOPEN)(S32);

typedef struct BINKRECT
{
	S32 Left;
	S32 Top;
	S32 Width;
	S32 Height;
} BINKRECT;

typedef struct BINK
{
	U32 Width;
	U32 Height;
	U32 Frames;
	U32 FrameNum;
	U32 LastFrameNum;
	U32 FrameRate;
	U32 FrameRateDiv;
	U32 ReadError;
	U32 OpenFlags;
	U32 BinkType;
	U32 Size;
	U32 FrameSize;
	U32 SndSize;
	BINKRECT FrameRects[8];
	S32 NumRects;
	U32 PlaneNum;
	void* YPlane[2];
	void* APlane[2];
	U32 YWidth;
	U32 YHeight;
	U32 UVWidth;
	U32 UVHeight;
	void* MaskPlane;
	U32 MaskPitch;
	U32 MaskLength;
	U32 LargestFrameSize;
	U32 InternalFrames;
	S32 NumTracks;
	U32 Highest1SecRate;
	U32 Highest1SecFrame;
	S32 Paused;
	U32 BackgroundThread;
	void* compframe;
	void* preloadptr;
	U32* frameoffsets;
	BinkMovie* player;
} BINK, *HBINK;

S32 WINAPI BinkSetSoundOnOff(HBINK bnk, int onoff);
HBINK WINAPI BinkOpen(const char* name, U32 flags);
void WINAPI BinkSetSoundTrack(int total_tracks, U32* tracks);
S32  WINAPI BinkSetSoundSystem(BINKSNDSYSOPEN open, U32 param);
void WINAPI BinkOpenDirectSound(U32 param);
void WINAPI BinkClose(HBINK bnk);
S32  WINAPI BinkCopyToBuffer(HBINK bnk, void* dest, U32 destpitch, U32 destheight, U32 destx, U32 desty, U32 flags);
void WINAPI BinkNextFrame(HBINK bnk);
S32  WINAPI BinkDoFrame(HBINK bnk);
S32  WINAPI BinkWait(HBINK bnk);
S32  WINAPI BinkPause(HBINK bnk, S32 pause);
