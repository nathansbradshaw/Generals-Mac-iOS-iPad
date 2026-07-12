/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
//////////////////////////////////////////////////////////////////////////////// 

// FILE: ExtrasMenu.cpp ////////////////////////////////////////////////////////////////////////////
// Desc:   Extras/QoL settings menu for SagePatch (camera, scroll, terrain, graphics)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "GameClient/GameClient.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/Shell.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetSlider.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/MessageBox.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/WindowLayout.h"
#include "GameClient/Display.h"
#if defined(SAGE_GENERALS_ONLINE)
#include "GameNetwork/GeneralsOnline/OnlineServices_Init.h"
#include "GameNetwork/GeneralsOnline/HTTP/HTTPManager.h"
#include "GameNetwork/GameSpyOverlay.h"
#endif

// Widget IDs and pointers
static NameKeyType sliderMaxCameraHeightID = NAMEKEY_INVALID;
static NameKeyType sliderMinCameraHeightID = NAMEKEY_INVALID;
static NameKeyType sliderCameraPitchID = NAMEKEY_INVALID;
static NameKeyType sliderScrollSpeedID = NAMEKEY_INVALID;
static NameKeyType sliderDrawDistanceID = NAMEKEY_INVALID;

static GameWindow *sliderMaxCameraHeight = nullptr;
static GameWindow *sliderMinCameraHeight = nullptr;
static GameWindow *sliderCameraPitch = nullptr;
static GameWindow *sliderScrollSpeed = nullptr;
static GameWindow *sliderDrawDistance = nullptr;
#if defined(SAGE_GENERALS_ONLINE)
// GeneralsX @feature BenderAI 11/07/2026 Expose the shared self-hosted
// GeneralsOnline endpoint through the same engine UI on every platform.
static NameKeyType textEntryOnlineServerID = NAMEKEY_INVALID;
static GameWindow *textEntryOnlineServer = nullptr;
static NameKeyType buttonTestOnlineServerID = NAMEKEY_INVALID;
#endif

static OptionPreferences *pref = nullptr;

// Slider ranges (matching .wnd definitions)
static const Int SLIDER_MAX_CAMERA_HEIGHT_MIN = 100;
static const Int SLIDER_MAX_CAMERA_HEIGHT_MAX = 1000;
static const Int SLIDER_MIN_CAMERA_HEIGHT_MIN = 50;
static const Int SLIDER_MIN_CAMERA_HEIGHT_MAX = 300;
static const Int SLIDER_CAMERA_PITCH_MIN = 20;
static const Int SLIDER_CAMERA_PITCH_MAX = 60;
static const Int SLIDER_SCROLL_SPEED_MIN = 1;
static const Int SLIDER_SCROLL_SPEED_MAX = 200;
static const Int SLIDER_DRAW_DISTANCE_MIN = 100;
static const Int SLIDER_DRAW_DISTANCE_MAX = 200;

//-------------------------------------------------------------------------------------------------
void ExtrasMenuInit(WindowLayout *layout, void *userData)
{
	sliderMaxCameraHeightID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderMaxCameraHeight");
	sliderMinCameraHeightID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderMinCameraHeight");
	sliderCameraPitchID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderCameraPitch");
	sliderScrollSpeedID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderScrollSpeed");
	sliderDrawDistanceID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:SliderDrawDistance");
#if defined(SAGE_GENERALS_ONLINE)
	textEntryOnlineServerID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:TextEntryOnlineServer");
	buttonTestOnlineServerID = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:ButtonTestOnlineServer");
#endif

	sliderMaxCameraHeight = TheWindowManager->winGetWindowFromId(nullptr, sliderMaxCameraHeightID);
	sliderMinCameraHeight = TheWindowManager->winGetWindowFromId(nullptr, sliderMinCameraHeightID);
	sliderCameraPitch = TheWindowManager->winGetWindowFromId(nullptr, sliderCameraPitchID);
	sliderScrollSpeed = TheWindowManager->winGetWindowFromId(nullptr, sliderScrollSpeedID);
	sliderDrawDistance = TheWindowManager->winGetWindowFromId(nullptr, sliderDrawDistanceID);
#if defined(SAGE_GENERALS_ONLINE)
	textEntryOnlineServer = TheWindowManager->winGetWindowFromId(nullptr, textEntryOnlineServerID);
#endif

	pref = NEW OptionPreferences;

	// Populate sliders from current settings
	if (sliderMaxCameraHeight) {
		Int val = (Int)pref->getMaxCameraHeight();
		GadgetSliderSetPosition(sliderMaxCameraHeight, val);
	}
	if (sliderMinCameraHeight) {
		Int val = (Int)pref->getMinCameraHeight();
		GadgetSliderSetPosition(sliderMinCameraHeight, val);
	}
	if (sliderCameraPitch) {
		Int val = (Int)pref->getCameraPitch();
		GadgetSliderSetPosition(sliderCameraPitch, val);
	}
	if (sliderScrollSpeed) {
		Int val = (Int)(pref->getScrollFactor() * 100.0f);
		GadgetSliderSetPosition(sliderScrollSpeed, val);
	}
	if (sliderDrawDistance) {
		Int val = (Int)(pref->getTerrainDrawDistanceScale() * 100.0f);
		GadgetSliderSetPosition(sliderDrawDistance, val);
	}
#if defined(SAGE_GENERALS_ONLINE)
	if (textEntryOnlineServer) {
		UnicodeString serverURL;
		serverURL.translate(NGMP_OnlineServicesManager::Settings.Network_GetServiceURL().c_str());
		GadgetTextEntrySetText(textEntryOnlineServer, serverURL);
	}
#else
	GameWindow *onlineServerLabel = TheWindowManager->winGetWindowFromId(
		nullptr, TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:LabelOnlineServer"));
	GameWindow *onlineServerEntry = TheWindowManager->winGetWindowFromId(
		nullptr, TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:TextEntryOnlineServer"));
	GameWindow *onlineServerTest = TheWindowManager->winGetWindowFromId(
		nullptr, TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:ButtonTestOnlineServer"));
	if (onlineServerLabel)
		onlineServerLabel->winHide(TRUE);
	if (onlineServerEntry)
		onlineServerEntry->winHide(TRUE);
	if (onlineServerTest)
		onlineServerTest->winHide(TRUE);
#endif
}

//-------------------------------------------------------------------------------------------------
void ExtrasMenuUpdate(WindowLayout *layout, void *userData)
{
}

//-------------------------------------------------------------------------------------------------
void ExtrasMenuShutdown(WindowLayout *layout, void *userData)
{
	sliderMaxCameraHeight = nullptr;
	sliderMinCameraHeight = nullptr;
	sliderCameraPitch = nullptr;
	sliderScrollSpeed = nullptr;
	sliderDrawDistance = nullptr;
#if defined(SAGE_GENERALS_ONLINE)
	textEntryOnlineServer = nullptr;
#endif

	if (pref) {
		delete pref;
		pref = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
static Bool saveExtras()
{
	if (!pref)
		return FALSE;

#if defined(SAGE_GENERALS_ONLINE)
	if (textEntryOnlineServer) {
		AsciiString serverURL;
		serverURL.translate(GadgetTextEntryGetText(textEntryOnlineServer));
		if (!NGMP_OnlineServicesManager::Settings.Network_SetServiceURL(serverURL.str())) {
			GSMessageBoxOk(UnicodeString(L"Invalid Online Server Address"),
				UnicodeString(L"Enter a complete http:// or https:// address, including the server name or IP and port."));
			return FALSE;
		}
	}
#endif

	Int val;

	val = GadgetSliderGetPosition(sliderMaxCameraHeight);
	if (val > 0) {
		TheWritableGlobalData->m_maxCameraHeight = (Real)val;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["MaxCameraHeight"] = prefString;
	}

	val = GadgetSliderGetPosition(sliderMinCameraHeight);
	if (val > 0) {
		TheWritableGlobalData->m_minCameraHeight = (Real)val;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["MinCameraHeight"] = prefString;
	}

	val = GadgetSliderGetPosition(sliderCameraPitch);
	if (val > 0) {
		TheWritableGlobalData->m_cameraPitch = (Real)val;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["CameraPitch"] = prefString;
	}

	val = GadgetSliderGetPosition(sliderScrollSpeed);
	if (val > 0) {
		TheWritableGlobalData->m_keyboardScrollFactor = val / 100.0f;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["ScrollFactor"] = prefString;
	}

	val = GadgetSliderGetPosition(sliderDrawDistance);
	if (val > 0) {
		TheWritableGlobalData->m_terrainDrawDistanceScale = val / 100.0f;
		AsciiString prefString;
		prefString.format("%d", val);
		(*pref)["TerrainDrawDistanceScale"] = prefString;
	}

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
static void setDefaults()
{
	if (sliderMaxCameraHeight)
		GadgetSliderSetPosition(sliderMaxCameraHeight, 500);
	if (sliderMinCameraHeight)
		GadgetSliderSetPosition(sliderMinCameraHeight, 80);
	if (sliderCameraPitch)
		GadgetSliderSetPosition(sliderCameraPitch, 37);
	if (sliderScrollSpeed)
		GadgetSliderSetPosition(sliderScrollSpeed, 100);
	if (sliderDrawDistance)
		GadgetSliderSetPosition(sliderDrawDistance, 105);
#if defined(SAGE_GENERALS_ONLINE)
	if (textEntryOnlineServer) {
		GadgetTextEntrySetText(textEntryOnlineServer,
			UnicodeString(L"https://localhost:9000/env/prod/contract/1"));
	}
#endif
}

#if defined(SAGE_GENERALS_ONLINE)
// GeneralsX @feature BenderAI 12/07/2026 Probe the entered self-hosted service
// before saving it, with platform-neutral HTTP and clear in-menu feedback.
static void testOnlineServer()
{
	if (!textEntryOnlineServer)
		return;

	AsciiString enteredURL;
	enteredURL.translate(GadgetTextEntryGetText(textEntryOnlineServer));
	std::string normalizedURL;
	if (!GenOnlineSettings::Network_NormalizeServiceURL(enteredURL.str(), &normalizedURL)) {
		GSMessageBoxOk(UnicodeString(L"Invalid Online Server Address"),
			UnicodeString(L"Enter a complete http:// or https:// address, including the server name or IP and port."));
		return;
	}

	NGMP_OnlineServicesManager *manager = NGMP_OnlineServicesManager::GetInstance();
	if (!manager || !manager->GetHTTPManager()) {
		GSMessageBoxOk(UnicodeString(L"Connection Test Unavailable"),
			UnicodeString(L"GeneralsOnline networking is not initialized."));
		return;
	}

	GameWindow *testButton = TheWindowManager->winGetWindowFromId(nullptr, buttonTestOnlineServerID);
	if (testButton)
		testButton->winEnable(FALSE);

	std::map<std::string, std::string> headers;
	const std::string testURL = normalizedURL + "/ServiceConfig";
	manager->GetHTTPManager()->SendGETRequest(testURL.c_str(), EIPProtocolVersion::DONT_CARE, headers,
		[](bool requestSucceeded, int statusCode, std::string, HTTPRequest *) {
			GameWindow *button = TheWindowManager->winGetWindowFromId(nullptr, buttonTestOnlineServerID);
			if (button)
				button->winEnable(TRUE);

			if (requestSucceeded && statusCode >= 200 && statusCode < 300) {
				GSMessageBoxOk(UnicodeString(L"Connection Successful"),
					UnicodeString(L"The GeneralsOnline server is reachable and returned a valid HTTP response."));
			} else {
				UnicodeString detail;
				if (statusCode > 0)
					detail.format(L"The server returned HTTP status %d. Check the address and server configuration.", statusCode);
				else
					detail = UnicodeString(L"The server could not be reached within five seconds. Check the address, firewall, TLS certificate, and local-network access.");
				GSMessageBoxOk(UnicodeString(L"Connection Failed"), detail);
			}
		}, nullptr, 5000);
}
#endif

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType ExtrasMenuSystem(GameWindow *window, UnsignedInt msg,
																				WindowMsgData mData1, WindowMsgData mData2)
{
	static NameKeyType buttonBack = NAMEKEY_INVALID;
	static NameKeyType buttonDefaults = NAMEKEY_INVALID;
	static NameKeyType buttonAccept = NAMEKEY_INVALID;

	switch (msg) {

		case GWM_CREATE:
		{
			buttonBack = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:ButtonBack");
			buttonDefaults = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:ButtonDefaults");
			buttonAccept = TheNameKeyGenerator->nameToKey("ExtrasMenu.wnd:ButtonAccept");
			break;
		}

		case GWM_DESTROY:
			break;

		case GWM_INPUT_FOCUS:
		{
			if (mData1 == TRUE)
				*(Bool *)mData2 = TRUE;
			return MSG_HANDLED;
		}

		case GBM_SELECTED:
		{
			GameWindow *control = (GameWindow *)mData1;
			Int controlID = control->winGetWindowId();

			if (controlID == buttonBack) {
				TheShell->pop();
			}
			else if (controlID == buttonAccept) {
				if (!saveExtras())
					break;
				if (pref) {
					pref->write();
				}
				TheShell->pop();
			}
			else if (controlID == buttonDefaults) {
				setDefaults();
			}
#if defined(SAGE_GENERALS_ONLINE)
			else if (controlID == buttonTestOnlineServerID) {
				testOnlineServer();
			}
#endif
			break;
		}

		default:
			break;
	}

	return MSG_IGNORED;
}

//-------------------------------------------------------------------------------------------------
WindowMsgHandledType ExtrasMenuInput(GameWindow *window, UnsignedInt msg,
																				WindowMsgData mData1, WindowMsgData mData2)
{
	switch (msg) {

		case GWM_CHAR:
		{
			UnsignedByte key = mData1;
			UnsignedByte state = mData2;

			switch (key) {

				case KEY_ESC:
				{
					if (BitIsSet(state, KEY_STATE_UP)) {
						TheShell->pop();
					}
					return MSG_HANDLED;
				}

			}
		}

	}

	return MSG_IGNORED;
}
