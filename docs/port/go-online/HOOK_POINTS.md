# NGMP hook points outside the subtree (T0.3)

Source: their fork @ b7cfeaf0. Tags: **[MVP]** custom-match path,
**[non-MVP]** quickmatch/social/stats — leave unwired, **[verify]** unclear, classify during T4.1.

| File | Tag | Guess |
|---|---|---|
| Include/Common/GameEngine.h + Source/Common/GameEngine.cpp | MVP | engine init/shutdown/update pump for OnlineServices |
| Main/WinMain.cpp, Win32Device/.../Win32GameEngine.cpp | MVP | process startup init (port to our SDL3 engine entry) |
| GameClient/GUI/.../MainMenu.cpp | MVP | online entry button |
| Menus/WOLLoginMenu.cpp, WOLWelcomeMenu.cpp | MVP | login + online landing |
| Menus/WOLLobbyMenu.cpp, WOLGameSetupMenu.cpp, WOLMapSelectMenu.cpp | MVP | custom-match lobby/staging/map |
| Menus/PopupHostGame.cpp, PopupJoinGame.cpp | MVP | host/join dialogs |
| GameClient/GameWindowManagerScript.cpp, GameNetwork/GUIUtil.cpp | MVP | window/script wiring for above menus |
| GameLogic/System/GameLogic.cpp, GameClient/GameClient.cpp | MVP | frame pump / transport handoff |
| Include/GameNetwork/UDPTransport.h | MVP | transport abstraction their NextGenTransport plugs into |
| Common/version.cpp | MVP | version gate for match compatibility |
| Menus/WOLQuickMatchMenu.cpp | non-MVP | quickmatch |
| Menus/WOLBuddyOverlay.cpp, PopupPlayerInfo.cpp | non-MVP | social/buddies |
| Menus/ScoreScreen.cpp, Common/StatsExporter.cpp | non-MVP | stats upload |
| Common/Recorder.cpp | non-MVP | replay upload (verify: may also touch match flow) |
| GUICallbacks/InGameChat.cpp | verify | in-game chat — MVP if it rides the match transport |
| Menus/OptionsMenu.cpp | verify | online settings UI |
| Common/System/registry.cpp | verify | settings storage |
| MessageStream/CommandXlat.cpp, SelectionXlat.cpp | verify | possibly 60Hz input changes |
| GameClient/InGameUI.cpp, W3DDevice/.../W3DInGameUI.cpp | verify | possibly connection-status UI |
| GameLogic: EMPUpdate.cpp, Weapon.cpp, Scripts.cpp | verify | may be 60Hz/logic-rate refs — MUST understand before skipping (sim-affecting) |
| WWVegas/WW3D2/ww3d.cpp/.h | verify | render-loop pacing? |

## Raw grep hits (first 3 per file)

```
== GeneralsMD/Code/GameEngine/Include/Common/GameEngine.h
53:void TearDownGeneralsOnline();
== GeneralsMD/Code/GameEngine/Include/GameNetwork/UDPTransport.h
31:// NGMP NOTE: We have multiple transports now, so UDPTransport is what Transport was. It's the legacy, direct connection transport the original game used. Transport is now a base class.
== GeneralsMD/Code/GameEngine/Source/Common/GameEngine.cpp
114:#include "../OnlineServices_Init.h"
119:static bool g_bTearDownGeneralsOnlineRequested = false;
120:void TearDownGeneralsOnline()
== GeneralsMD/Code/GameEngine/Source/Common/Recorder.cpp
50:#include "../NGMPGame.h"
51:#include "../OnlineServices_Init.h"
53:extern NGMPGame* TheNGMPGame;
== GeneralsMD/Code/GameEngine/Source/Common/StatsExporter.cpp
38:#include "GameNetwork/GeneralsOnline/json.hpp"
== GeneralsMD/Code/GameEngine/Source/Common/System/registry.cpp
33:#include "../NGMP_include.h"
== GeneralsMD/Code/GameEngine/Source/Common/version.cpp
325:	return L"GeneralsOnline";
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/InGameChat.cpp
55:#include "../NGMPGame.h"
56:extern NGMPGame* TheNGMPGame;
206:	if (TheNGMPGame == nullptr)
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp
77:#include "../OnlineServices_Init.h"
219:	NGMP_OnlineServicesManager::GetInstance()->CancelUpdate();
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp
80:#include "../OnlineServices_Init.h"
1414:	if( (TheGameLogic->isInGame() && TheGameLogic->getGameMode() != GAME_SHELL) || NGMP_OnlineServicesManager::GetInstance() != nullptr)
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/PopupHostGame.cpp
77:#include "../OnlineServices_Init.h"
78:#include "../OnlineServices_LobbyInterface.h"
79:#include "../OnlineServices_Auth.h"
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/PopupJoinGame.cpp
104:	NGMP_OnlineServices_LobbyInterface* pLobbyInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_LobbyInterface>();
107:		DEBUG_LOG(("NGMP_OnlineServices_LobbyInterface is not initialized!"));
255:	NGMP_OnlineServices_LobbyInterface* pLobbyInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_LobbyInterface>();
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/PopupPlayerInfo.cpp
65:#include "../OnlineServices_Init.h"
151:	userPrefFilename.format("GeneralsOnline\\MiscPref%d.ini", playerID);
186:	// TODO_NGMP_STATS:
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/ScoreScreen.cpp
104:#include "../NGMP_interfaces.h"
105:#include "../OnlineServices_Init.h"
106:#include "../OnlineServices_StatsInterface.h"
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/WOLBuddyOverlay.cpp
57:#include "../OnlineServices_SocialInterface.h"
58:#include "../OnlineServices_Init.h"
59:#include "../OnlineServices_LobbyInterface.h"
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/WOLGameSetupMenu.cpp
69:#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
72:#include "../OnlineServices_Init.h"
74:NGMPGame* TheNGMPGame = NULL;
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/WOLLobbyMenu.cpp
72:#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
198:		// TODO_NGMP
209:		NGMP_OnlineServices_RoomsInterface* pRoomsInterface = NGMP_OnlineServicesManager::GetInterface<NGMP_OnlineServices_RoomsInterface>();
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/WOLLoginMenu.cpp
35:#include "../NGMP_types.h"
37:void NGMP_WOLLoginMenu_LoginCallback(ELoginResult loginResult);
74:#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/WOLMapSelectMenu.cpp
71:#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
72:#include "GameNetwork/GeneralsOnline/NGMPGame.h"
73:extern NGMPGame* TheNGMPGame;
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/WOLQuickMatchMenu.cpp
83:#include "../OnlineServices_Init.h"
84:#include "../OnlineServices_Auth.h"
85:#include "../NGMPGame.h"
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GUICallbacks/Menus/WOLWelcomeMenu.cpp
70:#include "GameNetwork/GeneralsOnline/NGMP_interfaces.h"
235:	// TODO_NGMP
244:		AsciiString aMotd = NGMP_OnlineServicesManager::GetInstance() == nullptr ? AsciiString() : AsciiString(NGMP_OnlineServicesManager::GetInstance()->GetMOTD().c_str());
== GeneralsMD/Code/GameEngine/Source/GameClient/GUI/GameWindowManagerScript.cpp
2731:	char gofilepath[_MAX_PATH] = "GeneralsOnlineGameData\\";
2733:		sprintf(gofilepath, "GeneralsOnlineGameData\\%s", filename);
== GeneralsMD/Code/GameEngine/Source/GameClient/GameClient.cpp
803:	// TODO_NGMP: This should really use partial frame intervals instead of a fixed 60hz update
== GeneralsMD/Code/GameEngine/Source/GameClient/InGameUI.cpp
97:#include "../NGMP_interfaces.h"
98:#include "../OnlineServices_Init.h"
1920:	// GeneralsOnline NOTE: Increasing this, it's short + we increased framerate which is tied into the calc elsewhere
== GeneralsMD/Code/GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp
94:#include "../OnlineServices_Init.h"
278:	// TODO_NGMP: Remove this, SH saves it now
279:    NGMP_OnlineServicesManager::Settings.Graphics_SetFPS(maxRenderFps, TheWritableGlobalData->m_useFpsLimit);
== GeneralsMD/Code/GameEngine/Source/GameClient/MessageStream/SelectionXlat.cpp
1200:					// NGMP_CHANGE: Check every object in the group we are merging with if we are NOT a structure, and the target group includes a structure, do not merge. (solves SCUD bug)
== GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Update/EMPUpdate.cpp
139:	// TODO_NGMP: We should actually use a frame time delta here, not assume we're hitting 60
== GeneralsMD/Code/GameEngine/Source/GameLogic/Object/Weapon.cpp
504:// TODO_NGMP: Better solution, less hackyness
512:	// TODO_NGMP: Better solution for this, seems like an ini data bug
== GeneralsMD/Code/GameEngine/Source/GameLogic/ScriptEngine/Scripts.cpp
105:	"ShellGeneralsOnlineLogin", //SHELL_SCRIPT_HOOK_GENERALS_ONLINE_LOGIN,
106:	"ShellGeneralsOnlineLogout", //SHELL_SCRIPT_HOOK_GENERALS_ONLINE_LOGOUT,
107:	"ShellGeneralsOnlineEnteredFromGame", //SHELL_SCRIPT_HOOK_GENERALS_ONLINE_ENTERED_FROM_GAME,
== GeneralsMD/Code/GameEngine/Source/GameLogic/System/GameLogic.cpp
1358:			TheGameInfo = game = TheNGMPGame;	/// @todo: MDC add back in after demo
2883:			// TODO_NGMP: Handle missing CRCs, although that doesnt seem common
2888:			if (TheNGMPGame != nullptr)
== GeneralsMD/Code/GameEngine/Source/GameNetwork/GUIUtil.cpp
378:  // NGMP: safety
379:  // TODO_NGMP: Why can we get in here with no data during lobby creation? async?
493:				// NGMP: Support host migration, names can change for non-human occupied slots during migration
== GeneralsMD/Code/GameEngineDevice/Source/W3DDevice/GameClient/W3DInGameUI.cpp
53:#include "../OnlineServices_Init.h"
== GeneralsMD/Code/GameEngineDevice/Source/Win32Device/Common/Win32GameEngine.cpp
38:#include "../OnlineServices_Init.h"
109:			if (NGMP_OnlineServicesManager::GetInstance() != nullptr)
111:				NGMP_OnlineServicesManager::GetInstance()->Tick();
== GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/ww3d.cpp
120:#include "../../../GameEngine/Include/GameNetwork/GeneralsOnline/NextGenMP_defines.h"
== GeneralsMD/Code/Libraries/Source/WWVegas/WW3D2/ww3d.h
44:#include "../../../GameEngine/Include/GameNetwork/GeneralsOnline/NextGenMP_defines.h"
== GeneralsMD/Code/Main/WinMain.cpp
73:#include "../OnlineServices_Init.h"
893:		NGMP_OnlineServicesManager::AttemptLoadSteam();
912:        // TODO_NGMP: Better solution
```
