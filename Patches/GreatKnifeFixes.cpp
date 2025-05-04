/**
* Copyright (C) 2025 BigManJapan, mercury501, Polymega
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

#include "Logging\Logging.h"
#include "Patches\Patches.h"
#include "Common\Utils.h"
#include "Common\Settings.h"

BYTE* GreatKnifeAddr = nullptr;
BYTE* InGameVoiceEventAddr = nullptr;

bool InhibitionActive = false;
uint8_t WeaponValue = NULL;

BYTE* WeaponRenderAddr = nullptr;
BYTE* WeaponGripRenderAddr = nullptr;

bool GetAddresses()
{
	if (GreatKnifeAddr && InGameVoiceEventAddr && WeaponRenderAddr && WeaponGripRenderAddr)
		return true;

	constexpr BYTE GreatKnifeSearchBytes[]{ 0xEB, 0x02, 0x32, 0xD2 };
	GreatKnifeAddr = (BYTE*)SearchAndGetAddresses(0x0052EA5A, 0x0052ED8A, 0x0052E6AA, GreatKnifeSearchBytes, sizeof(GreatKnifeSearchBytes), 0x0A, __FUNCTION__);

	if (!GreatKnifeAddr)
	{
		Logging::Log() << __FUNCTION__ << " Error: failed to find great knife index memory address!";
		return false;
	}

	constexpr BYTE InGameVoiceSearchBytes[]{ 0x85, 0xC0, 0x75, 0x07, 0xD9, 0x05 };
	InGameVoiceEventAddr = (BYTE*)ReadSearchedAddresses(0x004A0305, 0x004A05B5, 0x0049FE75, InGameVoiceSearchBytes, sizeof(InGameVoiceSearchBytes), -0x04, __FUNCTION__);
	if (!InGameVoiceEventAddr)
	{
		Logging::Log() << __FUNCTION__ " Error: failed to find in game voice memory address!";
		return false;
	}

	constexpr BYTE WeaponRenderSearchBytes[]{ 0x85, 0xC0, 0x75, 0x07, 0xD9, 0x05 };
	WeaponRenderAddr = (BYTE*)0x01F7DCC0; //TODO (BYTE*)ReadSearchedAddresses(0x004A0305, 0x004A05B5, 0x0049FE75, WeaponRenderSearchBytes, sizeof(WeaponRenderSearchBytes), -0x04, __FUNCTION__);
	if (!WeaponRenderAddr)
	{
		Logging::Log() << __FUNCTION__ " Error: failed to find weapon render memory address!";
		return false;
	}

	constexpr BYTE WeaponGripRenderSearchBytes[]{ 0x85, 0xC0, 0x75, 0x07, 0xD9, 0x05 };
	WeaponGripRenderAddr = (BYTE*)0x01FB810C; //TODO  (BYTE*)ReadSearchedAddresses(0x004A0305, 0x004A05B5, 0x0049FE75, WeaponGripRenderSearchBytes, sizeof(WeaponGripRenderSearchBytes), -0x04, __FUNCTION__);
	if (!WeaponGripRenderAddr)
	{
		Logging::Log() << __FUNCTION__ " Error: failed to find weapon grip render memory address!";
		return false;
	}

	return true;
}

void RunGreatKnifeFixes()
{
	if (!GetAddresses())
		return;

	DWORD CutsceneId = GetCutsceneID();
	DWORD RoomId = GetRoomID();

	bool BoatStageCondition = (GetBoatFlag() == 0x01 && RoomId == R_TOWN_LAKE) && RowboatAnimationFix;
	bool CutsceneCondition = (CutsceneId != CS_NONE) && true; //TODO
	bool AngelaScreamCondition = (RoomId == R_LAB_TOP_G && InGameVoiceEventAddr != 0x00) && true; //TODO

	bool InhibitGreatKnifeFlag = BoatStageCondition || CutsceneCondition || AngelaScreamCondition;

	if (InhibitGreatKnifeFlag && !InhibitionActive)
	{
		UpdateMemoryAddress(GreatKnifeAddr, "\xFF", 0x01);
		WeaponValue = *WeaponRenderAddr;
		*WeaponRenderAddr = 0;
		*WeaponGripRenderAddr = 0;

		InhibitionActive = true;
	}
	else if (!InhibitGreatKnifeFlag && InhibitionActive)
	{
		UpdateMemoryAddress(GreatKnifeAddr, "\x08", 0x01);
		*WeaponRenderAddr = WeaponValue;
		*WeaponGripRenderAddr = WeaponValue;
		WeaponValue = NULL;

		InhibitionActive = false;
	}
}