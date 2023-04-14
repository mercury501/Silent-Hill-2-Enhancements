/**
* Copyright (C) 2023 mercury501
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

#include "Include\FMVPlayer\bink.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Patches\Patches.h"
#include "Common\Utils.h"
#include "Logging\Logging.h"

void PatchFMVPlayer()
{
	Logging::Log() << __FUNCTION__ << "Patching FMV player...";

	DWORD* binkIAT = (DWORD*)0x024a6bac;
	
	UpdateMemoryAddress(binkIAT, BinkSetSoundOnOff, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkOpen, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkSetSoundTrack, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkSetSoundSystem, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkOpenDirectSound, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkClose, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkCopyToBuffer, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkNextFrame, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkDoFrame, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkWait, 0x04);
	binkIAT += 1;
	UpdateMemoryAddress(binkIAT, BinkPause, 0x04);

}
