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

/*
** SDL3GameEngine.cpp
**
** Linux implementation of GameEngine using SDL3 for windowing/input.
**
** TheSuperHackers @feature CnC_Generals_Linux 07/02/2026
** Provides SDL3-based input and window management for Linux builds.
** Based on fighter19 reference implementation.
*/

#ifndef _WIN32

#include "SDL3GameEngine.h"
#include "OpenALAudioManager.h"
#include "SDL3Device/GameClient/SDL3Mouse.h"
#include "SDL3Device/GameClient/SDL3Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/Keyboard.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Gadget.h"
#include "W3DDevice/GameLogic/W3DGameLogic.h"
#include "W3DDevice/GameClient/W3DGameClient.h"
#include "W3DDevice/Common/W3DModuleFactory.h"
#include "W3DDevice/Common/W3DThingFactory.h"
#include "W3DDevice/Common/W3DFunctionLexicon.h"
#include "W3DDevice/Common/W3DRadar.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameClient/W3DWebBrowser.h"
#include "StdDevice/Common/StdLocalFileSystem.h"
#include "StdDevice/Common/StdBIGFileSystem.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/BuildAssistant.h"
#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

// Shared mobile input path: iOS and Android both drive the game through the
// same SDL3 touch->mouse gesture translator below. App-lifecycle handling stays
// platform-specific (iOS suspends the process and MoltenVK owns the drawable;
// Android also blocks its event pump on pause, while the shared gate below
// prevents a final engine update from submitting GPU work after suspension.
#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || defined(__ANDROID__)
#define GENERALSX_MOBILE 1
#else
#define GENERALSX_MOBILE 0
#endif

#if defined(SAGE_GENERALS_ONLINE) && GENERALSX_MOBILE
// Narrow bridge keeps the SDL platform layer independent of NGMP headers.
void GeneralsOnlineSuspendForAppLifecycle();
void GeneralsOnlineResumeFromAppLifecycle();
#endif

// Extern globals for input devices (set by GameClient)
extern Mouse *TheMouse;
extern Keyboard *TheKeyboard;
extern GameWindowManager *TheWindowManager;

#if GENERALSX_MOBILE
#include <atomic>
#include <deque>

// ---------------------------------------------------------------------------
// iOS app lifecycle
//
// iOS suspends the process when the app leaves the foreground. Any GPU work
// submitted around suspension stalls on drawable acquisition (MoltenVK waits
// out a timeout per present), which surfaces as multi-second input hangs right
// after resuming. SDL warns that lifecycle events can arrive outside the
// normal poll cycle, so they are captured in an event watcher that fires
// immediately on the delivering thread; the engine update loop checks the
// flag and skips simulation + rendering while backgrounded.
// ---------------------------------------------------------------------------
// Two independent reasons to halt the render/sim loop on iOS:
//  - BACKGROUNDED (home / switched away): the process is about to be suspended.
//  - INACTIVE (multitasking switcher open, Control Center, a notification
//    banner): iOS snapshots the window and owns the CAMetalLayer drawable during
//    this window — and crucially, opening the app switcher fires resign-active
//    WITHOUT a full background transition.
// Acquiring a Metal drawable during EITHER state fights iOS for the layer; across
// repeated suspend/switcher cycles MoltenVK is driven into an unrecoverable
// surface state and the app crashes (the reported "crashes after backgrounding /
// multitasking a few times"). Pause whenever either is set.
static std::atomic<bool> s_appBackgrounded{false};
static std::atomic<bool> s_appInactive{false};

// GeneralsX @bugfix Codex 09/10/2026 Gate Android GPU work as soon as SDL queues suspension.
static inline bool mobileShouldPauseRendering()
{
	// GeneralsX @bugfix Codex 09/10/2026 Android focus can leave before the
	// background event; stop issuing GPU work throughout that transition too.
	return s_appBackgrounded.load() || s_appInactive.load();
}

static bool SDLCALL mobileLifecycleWatcher(void *userdata, SDL_Event *event)
{
	switch (event->type) {
		case SDL_EVENT_WILL_ENTER_BACKGROUND:
		case SDL_EVENT_DID_ENTER_BACKGROUND:
			s_appBackgrounded.store(true);
			// GeneralsX @bugfix Codex 09/10/2026 SDL blocks its Android event pump
			// immediately after queuing this event; pause OpenAL while queuing it.
			if (auto *audio = dynamic_cast<OpenALAudioManager *>(TheAudio))
				audio->setAppSuspended(true);
			break;
		case SDL_EVENT_DID_ENTER_FOREGROUND:
			s_appBackgrounded.store(false);
			if (auto *audio = dynamic_cast<OpenALAudioManager *>(TheAudio))
				audio->setAppSuspended(false);
			break;
		// Resign/become active. On iOS, SDL maps applicationWillResignActive ->
		// window focus lost and applicationDidBecomeActive -> window focus gained.
		// Stay paused until fully active again (focus regained), which arrives
		// after DID_ENTER_FOREGROUND.
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			s_appInactive.store(true);
			break;
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			s_appInactive.store(false);
			break;
		default:
			break;
	}
	return true;
}
#endif // GENERALSX_MOBILE (app lifecycle)

#if GENERALSX_MOBILE
// ---------------------------------------------------------------------------
// Mobile touch -> mouse gesture translation (iOS and Android)
//
// SDL's automatic touch-mouse synthesis is disabled on mobile (SDL3Main.cpp
// sets SDL_HINT_TOUCH_MOUSE_EVENTS=0); every mouse event the game sees on a
// touch device is synthesized here, through the same SDL3Mouse::addSDLEvent
// path real mice use.
//
// Gestures (matching the game's stock control scheme, which is LMB-centric):
//   1 finger tap/drag     -> left button click / drag (select, command, drag-box)
//   1 finger long-press   -> right button click (deselect), if finger stays put
//   2 finger drag         -> right-button drag at the centroid (camera scroll)
//   2 finger pinch        -> mouse wheel (camera zoom)
//   3 finger tap          -> Escape (skip movies/cutscenes, close cancellable UI)
// ---------------------------------------------------------------------------
namespace {

struct TouchState {
	enum Phase {
		IDLE,        // no fingers tracked
		PENDING,     // finger1 down, gesture identity not yet known, nothing sent
		DRAGGING,    // finger1 drag in progress, LMB held
		LONGPRESSED, // long-press fired (RMB click sent), swallow until lift
		PAN,         // two-finger camera pan, RMB held
		ESCAPE_SENT  // three-finger chord fired Escape; swallow until all lift
	};

	Phase phase = IDLE;
	SDL_FingerID finger1 = 0;
	SDL_FingerID finger2 = 0;
	float downX = 0.0f, downY = 0.0f;   // finger1 down position (window points)
	float lastX = 0.0f, lastY = 0.0f;   // finger1 latest position
	float panX = 0.0f, panY = 0.0f;     // pan centroid
	float pinchDist = 0.0f;             // finger distance at last wheel step
	Uint64 downTicks = 0;
	float f1x = 0.0f, f1y = 0.0f, f2x = 0.0f, f2y = 0.0f; // normalized per finger
	int escapeFingersRemaining = 0;
};

TouchState s_touch;

const Uint64 LONG_PRESS_MS = 600;
const float PINCH_STEP_RATIO = 0.06f;  // 6% distance change per wheel tick
const float TAP_DEAD_ZONE_PX = 8.0f;   // jitter below this keeps a tap a tap

void sendSyntheticMouse(SDL3Mouse *mouse, SDL_Window *window, Uint32 type,
                        float x, float y, Uint8 button = 0, float wheelY = 0.0f)
{
	// The windowID must be valid: SDL3Mouse::scaleMouseCoordinates() looks the
	// window up by id to map window points into the game's internal resolution,
	// and silently skips scaling when the lookup fails.
	const SDL_WindowID windowID = SDL_GetWindowID(window);

	SDL_Event ev;
	SDL_zero(ev);
	ev.type = type;
	ev.common.timestamp = SDL_GetTicksNS();
	switch (type) {
		case SDL_EVENT_MOUSE_MOTION:
			ev.motion.windowID = windowID;
			ev.motion.x = x;
			ev.motion.y = y;
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			ev.button.windowID = windowID;
			ev.button.button = button;
			ev.button.down = (type == SDL_EVENT_MOUSE_BUTTON_DOWN);
			ev.button.clicks = 1;
			ev.button.x = x;
			ev.button.y = y;
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			ev.wheel.windowID = windowID;
			ev.wheel.x = 0.0f;
			ev.wheel.y = wheelY;
			ev.wheel.mouse_x = x;
			ev.wheel.mouse_y = y;
			break;
	}
	mouse->addSDLEvent(&ev);
}

// GeneralsX @bugfix Codex 09/10/2026 Deliver hover, press, and release in
// separate engine updates. SAGE publishes its mouse position before consuming
// this frame's motion events; an instantaneous touch click otherwise reaches
// hover-dependent controls before their mouse-enter message.
struct TouchClick {
	float x, y;
	Uint8 button;
	int stage;
};
std::deque<TouchClick> s_touchClicks;

void queueTouchClick(float x, float y, Uint8 button)
{
	if (s_touchClicks.size() < 16)
		s_touchClicks.push_back({x, y, button, 0});
}

void updateTouchClicks(SDL3Mouse *mouse, SDL_Window *window)
{
	if (s_touchClicks.empty())
		return;
	TouchClick &click = s_touchClicks.front();
	sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, click.x, click.y);
	if (click.stage == 1)
		sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_DOWN, click.x, click.y, click.button);
	else if (click.stage == 2)
		sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP, click.x, click.y, click.button);
	if (++click.stage == 3)
		s_touchClicks.pop_front();
}

void cancelTouchGestures(SDL3Mouse *mouse, SDL_Window *window)
{
	if (!s_touchClicks.empty() && s_touchClicks.front().stage == 2) {
		const TouchClick &click = s_touchClicks.front();
		sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP, click.x, click.y, click.button);
	}
	s_touchClicks.clear();
	if (s_touch.phase == TouchState::DRAGGING)
		sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP, s_touch.lastX, s_touch.lastY, SDL_BUTTON_LEFT);
	else if (s_touch.phase == TouchState::PAN)
		sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP, s_touch.panX, s_touch.panY, SDL_BUTTON_RIGHT);
	s_touch = TouchState{};
}

void sendSyntheticEscape(SDL_Window *window)
{
	SDL3Keyboard *keyboard = TheKeyboard ? dynamic_cast<SDL3Keyboard *>(TheKeyboard) : nullptr;
	if (!keyboard)
		return;

	for (int index = 0; index < 2; ++index) {
		const bool down = index == 0;
		const SDL_EventType type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
		SDL_Event ev;
		SDL_zero(ev);
		ev.type = type;
		ev.key.type = type;
		ev.key.timestamp = SDL_GetTicksNS();
		ev.key.windowID = SDL_GetWindowID(window);
		ev.key.scancode = SDL_SCANCODE_ESCAPE;
		ev.key.key = SDLK_ESCAPE;
		ev.key.down = down;
		ev.key.repeat = false;
		keyboard->addSDLEvent(&ev);
	}
}

void beginPan(SDL3Mouse *mouse, SDL_Window *window, int winW, int winH)
{
	s_touch.panX = (s_touch.f1x + s_touch.f2x) * 0.5f * (float)winW;
	s_touch.panY = (s_touch.f1y + s_touch.f2y) * 0.5f * (float)winH;
	const float dx = (s_touch.f1x - s_touch.f2x) * (float)winW;
	const float dy = (s_touch.f1y - s_touch.f2y) * (float)winH;
	s_touch.pinchDist = SDL_sqrtf(dx * dx + dy * dy);
	sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, s_touch.panX, s_touch.panY);
	sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_DOWN,
	                   s_touch.panX, s_touch.panY, SDL_BUTTON_RIGHT);
	s_touch.phase = TouchState::PAN;
}

void handleTouchEvent(SDL3Mouse *mouse, SDL_Window *window, const SDL_Event &event)
{
	int winW = 0, winH = 0;
	SDL_GetWindowSize(window, &winW, &winH);
	const float px = event.tfinger.x * (float)winW;
	const float py = event.tfinger.y * (float)winH;

	switch (event.type) {
	case SDL_EVENT_FINGER_DOWN:
		if (s_touch.phase == TouchState::IDLE) {
			// Defer all BUTTON output: a finger landing could become a tap, a
			// drag-box, a long-press, or the first finger of a camera pan. A
			// premature LMB down+up is a real click to the game (e.g. it sets a
			// rally point when a production building is selected).
			s_touch.finger1 = event.tfinger.fingerID;
			s_touch.phase = TouchState::PENDING;
			s_touch.downX = s_touch.lastX = px;
			s_touch.downY = s_touch.lastY = py;
			s_touch.f1x = event.tfinger.x;
			s_touch.f1y = event.tfinger.y;
			s_touch.downTicks = SDL_GetTicks();
			// Move the cursor to the touch point NOW (motion clicks nothing, so the
			// deferred-tap protection is intact). This lets the GUI process hover
			// over the next frame(s) before the tap commits — hover-driven widgets
			// (e.g. the Generals Challenge general buttons, which are checkboxes
			// that ignore a click unless WIN_STATE_HILITED was set by a prior
			// mouse-enter) then accept the click. Real mice hover before clicking;
			// without this, a synthetic tap teleports + clicks in one instant and
			// the widget is never hilited, so only the default/first item responds.
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, px, py);
		}
		else if (s_touch.phase == TouchState::PENDING) {
			// Second finger before the first committed to anything: pure pan,
			// no left-click ever happened.
			s_touch.finger2 = event.tfinger.fingerID;
			s_touch.f2x = event.tfinger.x;
			s_touch.f2y = event.tfinger.y;
			beginPan(mouse, window, winW, winH);
		}
		else if (s_touch.phase == TouchState::DRAGGING) {
			// Second finger during a live drag: finish the drag-box, then pan.
			s_touch.finger2 = event.tfinger.fingerID;
			s_touch.f2x = event.tfinger.x;
			s_touch.f2y = event.tfinger.y;
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP,
			                   s_touch.lastX, s_touch.lastY, SDL_BUTTON_LEFT);
			beginPan(mouse, window, winW, winH);
		}
		else if (s_touch.phase == TouchState::PAN) {
			// GeneralsX @feature nathanbradshaw 13/07/2026 A third finger
			// cancels the two-finger pan and injects an Escape down/up pair.
			// The movie-abort loop polls this same SDL keyboard queue.
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP,
				s_touch.panX, s_touch.panY, SDL_BUTTON_RIGHT);
			sendSyntheticEscape(window);
			s_touch.escapeFingersRemaining = 3;
			s_touch.phase = TouchState::ESCAPE_SENT;
		}
		// LONGPRESSED / ESCAPE_SENT extra fingers: ignored
		break;

	case SDL_EVENT_FINGER_MOTION:
		if (event.tfinger.fingerID == s_touch.finger1) {
			s_touch.f1x = event.tfinger.x;
			s_touch.f1y = event.tfinger.y;
			s_touch.lastX = px;
			s_touch.lastY = py;
		} else if (s_touch.phase == TouchState::PAN && event.tfinger.fingerID == s_touch.finger2) {
			s_touch.f2x = event.tfinger.x;
			s_touch.f2y = event.tfinger.y;
		} else {
			break;
		}

		if (s_touch.phase == TouchState::PENDING && event.tfinger.fingerID == s_touch.finger1) {
			const float moved = SDL_fabsf(px - s_touch.downX) + SDL_fabsf(py - s_touch.downY);
			if (moved >= TAP_DEAD_ZONE_PX) {
				// Commit to a drag: anchor the LMB at the original touch point so
				// drag-boxes start where the finger first landed.
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, s_touch.downX, s_touch.downY);
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_DOWN,
				                   s_touch.downX, s_touch.downY, SDL_BUTTON_LEFT);
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, px, py);
				s_touch.phase = TouchState::DRAGGING;
			}
		}
		else if (s_touch.phase == TouchState::DRAGGING && event.tfinger.fingerID == s_touch.finger1) {
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, px, py);
		}
		else if (s_touch.phase == TouchState::PAN) {
			const float cx = (s_touch.f1x + s_touch.f2x) * 0.5f * (float)winW;
			const float cy = (s_touch.f1y + s_touch.f2y) * 0.5f * (float)winH;
			sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_MOTION, cx, cy);
			s_touch.panX = cx;
			s_touch.panY = cy;

			const float dx = (s_touch.f1x - s_touch.f2x) * (float)winW;
			const float dy = (s_touch.f1y - s_touch.f2y) * (float)winH;
			const float dist = SDL_sqrtf(dx * dx + dy * dy);
			if (s_touch.pinchDist > 1.0f) {
				const float ratio = dist / s_touch.pinchDist;
				if (ratio > 1.0f + PINCH_STEP_RATIO) {
					sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_WHEEL, cx, cy, 0, 1.0f);
					s_touch.pinchDist = dist;
				} else if (ratio < 1.0f - PINCH_STEP_RATIO) {
					sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_WHEEL, cx, cy, 0, -1.0f);
					s_touch.pinchDist = dist;
				}
			}
		}
		break;

	case SDL_EVENT_FINGER_UP:
	case SDL_EVENT_FINGER_CANCELED:
		if (s_touch.phase == TouchState::ESCAPE_SENT) {
			if (--s_touch.escapeFingersRemaining <= 0) {
				s_touch.escapeFingersRemaining = 0;
				s_touch.phase = TouchState::IDLE;
			}
			break;
		}
		if (event.tfinger.fingerID != s_touch.finger1 &&
		    !(s_touch.phase == TouchState::PAN && event.tfinger.fingerID == s_touch.finger2)) {
			break;
		}
		switch (s_touch.phase) {
			case TouchState::PENDING:
				// A CANCELED touch (incoming call, notification shade, palm
				// rejection) must not become a committed tap — that would be a
				// phantom select/command/rally-point click at the cancel point.
				if (event.type == SDL_EVENT_FINGER_CANCELED) {
					break;
				}
				// Clean tap: deliver the full click at the exact press position.
				queueTouchClick(s_touch.downX, s_touch.downY, SDL_BUTTON_LEFT);
				break;
			case TouchState::DRAGGING:
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP, px, py, SDL_BUTTON_LEFT);
				break;
			case TouchState::PAN:
				sendSyntheticMouse(mouse, window, SDL_EVENT_MOUSE_BUTTON_UP,
				                   s_touch.panX, s_touch.panY, SDL_BUTTON_RIGHT);
				break;
			default:
				break;
		}
		s_touch.phase = TouchState::IDLE;
		break;
	}
}

// Called once per engine frame (not just per touch event): a perfectly
// stationary finger produces no SDL events, so the long-press timer must be
// polled from the frame loop or it would never fire.
void updateTouchLongPress(SDL3Mouse *mouse, SDL_Window *window)
{
	if (s_touch.phase == TouchState::PENDING &&
	    (SDL_GetTicks() - s_touch.downTicks) >= LONG_PRESS_MS) {
		// No LMB was sent yet (deferred), so this is a pure right-click.
		queueTouchClick(s_touch.downX, s_touch.downY, SDL_BUTTON_RIGHT);
		s_touch.phase = TouchState::LONGPRESSED;
	}
}

} // anonymous namespace
#endif // GENERALSX_MOBILE (touch translation)

namespace {

Bool DecodeNextUtf8Codepoint(const char* text, size_t length, size_t& offset, UnsignedInt& outCodepoint)
{
	outCodepoint = 0;
	if (!text || offset >= length) {
		return false;
	}

	const unsigned char first = static_cast<unsigned char>(text[offset]);
	if (first == 0) {
		return false;
	}

	if (first < 0x80) {
		outCodepoint = first;
		offset += 1;
		return true;
	}

	if ((first & 0xE0) == 0xC0 && offset + 1 < length) {
		const unsigned char second = static_cast<unsigned char>(text[offset + 1]);
		if ((second & 0xC0) == 0x80) {
			outCodepoint = ((first & 0x1F) << 6) | (second & 0x3F);
			offset += 2;
			return true;
		}
	}

	if ((first & 0xF0) == 0xE0 && offset + 2 < length) {
		const unsigned char second = static_cast<unsigned char>(text[offset + 1]);
		const unsigned char third = static_cast<unsigned char>(text[offset + 2]);
		if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80) {
			outCodepoint = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
			offset += 3;
			return true;
		}
	}

	if ((first & 0xF8) == 0xF0 && offset + 3 < length) {
		const unsigned char second = static_cast<unsigned char>(text[offset + 1]);
		const unsigned char third = static_cast<unsigned char>(text[offset + 2]);
		const unsigned char fourth = static_cast<unsigned char>(text[offset + 3]);
		if ((second & 0xC0) == 0x80 && (third & 0xC0) == 0x80 && (fourth & 0xC0) == 0x80) {
			outCodepoint = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6) | (fourth & 0x3F);
			offset += 4;
			return true;
		}
	}

	// Invalid UTF-8 sequence: skip one byte and keep processing.
	offset += 1;
	return false;
}

}

/**
 * Constructor: Initialize SDL3 game engine state
 */
SDL3GameEngine::SDL3GameEngine()
	: GameEngine(),
	  m_SDLWindow(nullptr),
	  m_IsInitialized(false),
	  m_IsActive(false),
	  m_IsTextInputActive(false),
	  m_TextInputFocusWindow(nullptr)
{
	fprintf(stderr, "DEBUG: SDL3GameEngine::SDL3GameEngine() created\n");
}

/**
 * Destructor: Cleanup SDL3 resources
 */
SDL3GameEngine::~SDL3GameEngine()
{
#if GENERALSX_MOBILE
	SDL_RemoveEventWatch(mobileLifecycleWatcher, nullptr);
#endif
	if (m_SDLWindow && m_IsTextInputActive) {
		SDL_StopTextInput(m_SDLWindow);
		m_IsTextInputActive = false;
		m_TextInputFocusWindow = nullptr;
	}

	if (m_IsInitialized) {
		// Window cleanup is done in reset/shutdown
	}
	fprintf(stderr, "DEBUG: SDL3GameEngine::~SDL3GameEngine() destroyed\n");
}

/**
 * From GameEngine: init() - initialize subsystems
 * 
 * GeneralsX @bugfix felipebraz 16/02/2026
 * Simplified to follow fighter19 pattern - SDL3/Vulkan initialized in SDL3Main.cpp
 * before GameEngine is created. This init() only delegates to parent GameEngine::init().
 * ApplicationHWnd and TheSDL3Window are already set by main() before this is called.
 */
void SDL3GameEngine::init(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::init() starting\n");

	if (TheGlobalData && TheGlobalData->m_headless) {
		// GeneralsX @bugfix Copilot 17/05/2026 Allow headless replay path to initialize engine subsystems without an SDL window.
		fprintf(stderr, "INFO: SDL3GameEngine::init() headless mode - skipping SDL window binding\n");
		m_SDLWindow = nullptr;
		m_IsInitialized = true;
		m_IsActive = true;
		GameEngine::init();
		return;
	}

	// Verify window was created by SDL3Main.cpp
	extern SDL_Window* TheSDL3Window;
	extern HWND ApplicationHWnd;
	
	if (!TheSDL3Window || !ApplicationHWnd) {
		fprintf(stderr, "FATAL: SDL3 window not initialized before GameEngine::init()\n");
		fprintf(stderr, "FATAL: TheSDL3Window=%p, ApplicationHWnd=%p\n", TheSDL3Window, ApplicationHWnd);
		return;
	}

	// Store window reference locally
	m_SDLWindow = TheSDL3Window;
	m_IsInitialized = true;
	m_IsActive = true;

#if GENERALSX_MOBILE
	// Lifecycle events can fire outside the poll cycle on iOS; catch them
	// immediately so rendering halts before the process is suspended.
	SDL_AddEventWatch(mobileLifecycleWatcher, nullptr);
#endif

	fprintf(stderr, "INFO: SDL3GameEngine using pre-initialized window\n");

	// Call parent init to initialize game subsystems
	GameEngine::init();
}

/**
 * From GameEngine: reset() - reset system to starting state
 */
void SDL3GameEngine::reset(void)
{
	fprintf(stderr, "DEBUG: SDL3GameEngine::reset()\n");
	if (m_SDLWindow && m_IsTextInputActive) {
		SDL_StopTextInput(m_SDLWindow);
		m_IsTextInputActive = false;
		m_TextInputFocusWindow = nullptr;
	}
	GameEngine::reset();
}

// GeneralsX @build Codex 08/10/2026 Opt-in, one-match command smoke test.
// Submit ordinary network messages rather than modifying simulation objects.
// "surrender" ends the match; any other nonempty value only produces/moves.
static void updateOnlineCommandSmokeTest()
{
	const char *mode = getenv("GENERALSX_SMOKE_COMMANDS");
	if (!mode || !*mode || !TheGameLogic || !TheMessageStream || !ThePlayerList ||
		!TheGameLogic->isInMultiplayerGame()) return;
	static Int stage = 0;
	if (stage == 4) return;
	static ObjectID movedID = INVALID_ID;
	static Coord3D moveStart;
	const UnsignedInt frame = TheGameLogic->getFrame();
	Player *local = ThePlayerList->getLocalPlayer();
	Object *worker = nullptr;
	Object *factory = nullptr;
	Int workers = 0;
	for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject()) {
		if (obj->getControllingPlayer() != local) continue;
		if (obj->isKindOf(KINDOF_DOZER)) {
			++workers;
			if (!worker || obj->getID() > worker->getID()) worker = obj;
		}
		if (obj->isKindOf(KINDOF_COMMANDCENTER)) factory = obj;
	}
	if (stage == 0 && frame >= 300) {
		stage = 1;
		ProductionUpdateInterface *production = factory ? factory->getProductionUpdateInterface() : nullptr;
		if (!worker || !production || production->canQueueCreateUnit(worker->getTemplate()) != CANMAKE_OK) {
			fprintf(stderr, "[GeneralsX] COMMAND_SMOKE production unavailable frame=%u workers=%d\n", frame, workers);
			return;
		}
		GameMessage *select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND);
		select->appendBooleanArgument(TRUE);
		select->appendObjectIDArgument(factory->getID());
		GameMessage *build = TheMessageStream->appendMessage(GameMessage::MSG_QUEUE_UNIT_CREATE);
		build->appendIntegerArgument(worker->getTemplate()->getTemplateID());
		build->appendIntegerArgument(production->requestUniqueUnitID());
		fprintf(stderr, "[GeneralsX] COMMAND_SMOKE queued frame=%u factory=%u unit=%s workers=%d\n",
			frame, factory->getID(), worker->getTemplate()->getName().str(), workers);
	} else if (stage == 1 && frame >= 900) {
		stage = 2;
		if (!worker || workers < 2) {
			fprintf(stderr, "[GeneralsX] COMMAND_SMOKE production failed frame=%u workers=%d\n", frame, workers);
			return;
		}
		movedID = worker->getID();
		moveStart = *worker->getPosition();
		Coord3D destination = moveStart;
		destination.x += 160.0f;
		destination.y += 80.0f;
		GameMessage *select = TheMessageStream->appendMessage(GameMessage::MSG_CREATE_SELECTED_GROUP_NO_SOUND);
		select->appendBooleanArgument(TRUE);
		select->appendObjectIDArgument(movedID);
		GameMessage *move = TheMessageStream->appendMessage(GameMessage::MSG_DO_MOVETO);
		move->appendLocationArgument(destination);
		fprintf(stderr, "[GeneralsX] COMMAND_SMOKE move frame=%u object=%u workers=%d from=(%.3f,%.3f) to=(%.3f,%.3f)\n",
			frame, movedID, workers, moveStart.x, moveStart.y, destination.x, destination.y);
	} else if (stage == 2 && frame >= 1500) {
		stage = 3;
		Object *moved = TheGameLogic->findObjectByID(movedID);
		Real distanceSquared = 0.0f;
		if (moved) {
			Real dx = moved->getPosition()->x - moveStart.x;
			Real dy = moved->getPosition()->y - moveStart.y;
			distanceSquared = dx*dx + dy*dy;
		}
		fprintf(stderr, "[GeneralsX] COMMAND_SMOKE movement %s frame=%u object=%u distanceSquared=%.3f\n",
			distanceSquared > 400.0f ? "passed" : "failed", frame, movedID, distanceSquared);
	} else if (stage == 3 && frame >= 2100) {
		stage = 4;
		if (strcmp(mode, "surrender") == 0) {
			GameMessage *quit = TheMessageStream->appendMessage(GameMessage::MSG_SELF_DESTRUCT);
			quit->appendBooleanArgument(TRUE);
			fprintf(stderr, "[GeneralsX] COMMAND_SMOKE surrender frame=%u\n", frame);
		} else {
			fprintf(stderr, "[GeneralsX] COMMAND_SMOKE complete frame=%u\n", frame);
		}
	}
}

/**
 * From GameEngine: update() - per-frame update
 */
void SDL3GameEngine::update(void)
{
	pollSDL3Events();
#if GENERALSX_MOBILE
	// Pause sim + render while backgrounded OR inactive (see mobileLifecycleWatcher).
	// Acquiring a Metal drawable in these windows fights iOS for the layer and,
	// across repeated suspend/switcher cycles, crashes MoltenVK. Keep polling so
	// we still catch the resume events; just don't touch the GPU.
	if (mobileShouldPauseRendering()) {
		SDL_Delay(50);
		return;
	}
#endif
#if GENERALSX_MOBILE
	if (TheMouse && m_SDLWindow) {
		SDL3Mouse *mouse = dynamic_cast<SDL3Mouse *>(TheMouse);
		if (mouse)
			updateTouchClicks(mouse, m_SDLWindow);
	}
#endif
	GameEngine::update();
	updateOnlineCommandSmokeTest();
}

/**
 * From GameEngine: execute() - main game loop
 */
void SDL3GameEngine::execute(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::execute() - entering main loop\n");
	GameEngine::execute();
	fprintf(stderr, "INFO: SDL3GameEngine::execute() - exited main loop\n");
}

/**
 * From GameEngine: serviceWindowsOS() - native OS service
 * On Linux, process SDL3 events
 */
void SDL3GameEngine::serviceWindowsOS(void)
{
	pollSDL3Events();
}

/**
 * Check if game has OS focus
 */
Bool SDL3GameEngine::isActive(void)
{
	return m_IsActive;
}

/**
 * Set OS focus status
 */
void SDL3GameEngine::setIsActive(Bool isActive)
{
	m_IsActive = isActive;
}

/**
 * Poll and process SDL3 events
 * Handles keyboard, mouse, window, and quit events
 */
void SDL3GameEngine::pollSDL3Events(void)
{
	if (!m_SDLWindow) {
		return;
	}

	updateTextInputState();

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_EVENT_QUIT:
				m_quitting = true;
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				m_quitting = true;
				break;

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				m_IsActive = true;
				if (TheMouse) {
					TheMouse->regainFocus();
					TheMouse->refreshCursorCapture();
				}
				break;

			case SDL_EVENT_WINDOW_FOCUS_LOST:
				m_IsActive = false;
#if GENERALSX_MOBILE
				if (SDL3Mouse *mouse = dynamic_cast<SDL3Mouse *>(TheMouse))
					cancelTouchGestures(mouse, m_SDLWindow);
#endif
				if (m_IsTextInputActive) {
					SDL_StopTextInput(m_SDLWindow);
					m_IsTextInputActive = false;
					m_TextInputFocusWindow = nullptr;
				}
				if (TheMouse) {
					TheMouse->loseFocus();
				}
				break;

#if GENERALSX_MOBILE
			// GeneralsX @bugfix Codex 09/10/2026 Reuse authenticated reconnect on Android.
			// App suspension/resume: mirror the desktop focus handling so audio
			// and mouse state pause cleanly (the render gate lives in update()).
			case SDL_EVENT_WILL_ENTER_BACKGROUND:
			case SDL_EVENT_DID_ENTER_BACKGROUND:
				m_IsActive = false;
				if (auto *audio = dynamic_cast<OpenALAudioManager *>(TheAudio))
					audio->setAppSuspended(true);
			#if defined(SAGE_GENERALS_ONLINE)
				GeneralsOnlineSuspendForAppLifecycle();
			#endif
				if (TheMouse) {
					TheMouse->loseFocus();
				}
				break;

			case SDL_EVENT_DID_ENTER_FOREGROUND:
				m_IsActive = true;
				if (auto *audio = dynamic_cast<OpenALAudioManager *>(TheAudio))
					audio->setAppSuspended(false);
			#if defined(SAGE_GENERALS_ONLINE)
				GeneralsOnlineResumeFromAppLifecycle();
			#endif
				if (TheMouse) {
					TheMouse->regainFocus();
					TheMouse->refreshCursorCapture();
				}
				break;
#endif

			case SDL_EVENT_WINDOW_MOUSE_ENTER:
				if (TheMouse) {
					TheMouse->onCursorMovedInside();
				}
				break;

			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				if (TheMouse) {
					TheMouse->onCursorMovedOutside();
				}
				break;

			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
#if GENERALSX_MOBILE
				// The Android Back action (trapped as AC_BACK, see SDL3Main.cpp)
				// maps to Escape: skip movies/cutscenes, close cancellable menus.
				// The game keyboard has no binding for AC_BACK, so translate and
				// swallow it rather than forwarding the raw key.
				if (event.key.key == SDLK_AC_BACK) {
					if (event.type == SDL_EVENT_KEY_DOWN) {
						sendSyntheticEscape(m_SDLWindow);
					}
					break;
				}
#endif
				// Fighter19 pattern: direct addSDLEvent() call
				// GeneralsX @refactor felipebraz 16/02/2026 Simplified event routing
				if (TheKeyboard) {
					SDL3Keyboard* keyboard = dynamic_cast<SDL3Keyboard*>(TheKeyboard);
					if (keyboard) {
						keyboard->addSDLEvent(&event);
					}
				}
				break;

			case SDL_EVENT_TEXT_INPUT:
				forwardTextInputEvent(event.text.text);
				break;

			case SDL_EVENT_MOUSE_MOTION:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_MOUSE_WHEEL:
#if GENERALSX_MOBILE
				// Belt-and-braces: drop SDL's own touch-synthesized mouse events.
				// The gesture translator owns all touch->mouse conversion; double
				// delivery would produce phantom second clicks.
				if (event.motion.which == SDL_TOUCH_MOUSEID) {
					break;
				}
#endif
				// Fighter19 pattern: direct addSDLEvent() call with raw SDL_Event
				// GeneralsX @refactor felipebraz 16/02/2026 Simplified event routing
				if (TheMouse) {
					SDL3Mouse* mouse = dynamic_cast<SDL3Mouse*>(TheMouse);
					if (mouse) {
						mouse->addSDLEvent(&event);
					}
				}
				break;

#if GENERALSX_MOBILE
			case SDL_EVENT_FINGER_DOWN:
			case SDL_EVENT_FINGER_MOTION:
			case SDL_EVENT_FINGER_UP:
			case SDL_EVENT_FINGER_CANCELED:
				if (TheMouse && m_SDLWindow) {
					SDL3Mouse* mouse = dynamic_cast<SDL3Mouse*>(TheMouse);
					if (mouse) {
						handleTouchEvent(mouse, m_SDLWindow, event);
					}
				}
				break;
#endif

			case SDL_EVENT_WINDOW_RESIZED:
				handleWindowEvent(event.window);
				break;

			default:
				// Ignore other events for now
				break;
		}

		updateTextInputState();
	}

#if GENERALSX_MOBILE
	// Poll the long-press timer every frame; a stationary finger emits no events.
	if (m_IsActive && TheMouse && m_SDLWindow) {
		SDL3Mouse* touchMouse = dynamic_cast<SDL3Mouse*>(TheMouse);
		if (touchMouse) {
			updateTouchLongPress(touchMouse, m_SDLWindow);
		}
	}
#endif
}

// GeneralsX @bugfix felipebraz 01/04/2026 Enable SDL text input only while an entry gadget owns focus.
void SDL3GameEngine::updateTextInputState(void)
{
	if (!m_SDLWindow || !TheWindowManager) {
		return;
	}

	GameWindow* focusedWindow = TheWindowManager->winGetFocus();
	const Bool wantsTextInput =
		focusedWindow != nullptr && BitIsSet(focusedWindow->winGetStyle(), GWS_ENTRY_FIELD);

	if (wantsTextInput) {
		// GeneralsX @bugfix nathanbradshaw 12/07/2026 Give mobile IMEs the focused
		// SAGE edit rectangle so SDL can keep it visible above the software keyboard.
		ICoord2D inputOrigin;
		ICoord2D inputSize;
		focusedWindow->winGetScreenPosition(&inputOrigin.x, &inputOrigin.y);
		focusedWindow->winGetSize(&inputSize.x, &inputSize.y);
		Int windowWidth = 0;
		Int windowHeight = 0;
		SDL_GetWindowSize(m_SDLWindow, &windowWidth, &windowHeight);
		const Real inputScaleX = TheDisplay->getWidth() > 0
			? INT_TO_REAL(windowWidth) / INT_TO_REAL(TheDisplay->getWidth()) : 1.0f;
		const Real inputScaleY = TheDisplay->getHeight() > 0
			? INT_TO_REAL(windowHeight) / INT_TO_REAL(TheDisplay->getHeight()) : 1.0f;
		SDL_Rect inputArea = {
			REAL_TO_INT(inputOrigin.x * inputScaleX),
			REAL_TO_INT(inputOrigin.y * inputScaleY),
			REAL_TO_INT(inputSize.x * inputScaleX),
			REAL_TO_INT(inputSize.y * inputScaleY)
		};
		SDL_SetTextInputArea(m_SDLWindow, &inputArea, 0);

		if (!m_IsTextInputActive) {
			if (SDL_StartTextInput(m_SDLWindow)) {
				m_IsTextInputActive = true;
			}
		}
		m_TextInputFocusWindow = focusedWindow;
	} else {
		if (m_IsTextInputActive) {
			SDL_StopTextInput(m_SDLWindow);
			m_IsTextInputActive = false;
		}
		SDL_SetTextInputArea(m_SDLWindow, nullptr, 0);
		m_TextInputFocusWindow = nullptr;
	}
}

// GeneralsX @bugfix felipebraz 01/04/2026 Forward SDL UTF-8 text input through existing GWM_IME_CHAR path.
void SDL3GameEngine::forwardTextInputEvent(const char* utf8Text)
{
	if (!utf8Text || !TheWindowManager) {
		return;
	}

	// GeneralsX @bugfix felipebraz 01/04/2026 Use tracked text-input focus window to keep SDL text delivery stable.
	GameWindow* targetWindow = m_TextInputFocusWindow;
	if (!targetWindow || !BitIsSet(targetWindow->winGetStyle(), GWS_ENTRY_FIELD)) {
		return;
	}

	const size_t textLength = strlen(utf8Text);
	size_t offset = 0;
	while (offset < textLength) {
		UnsignedInt codepoint = 0;
		if (!DecodeNextUtf8Codepoint(utf8Text, textLength, offset, codepoint)) {
			continue;
		}

		// GeneralsX @bugfix felipebraz 01/04/2026 Clamp IME char forwarding to BMP and reject UTF-16 surrogate range.
		if (codepoint == 0 || codepoint > 0x10FFFFU) {
			continue;
		}

		if (codepoint >= 0xD800U && codepoint <= 0xDFFFU) {
			continue;
		}

		if (codepoint > 0xFFFFU) {
			continue;
		}

		const WideChar wideCharacter = static_cast<WideChar>(codepoint);
		TheWindowManager->winSendInputMsg(targetWindow, GWM_IME_CHAR, static_cast<WindowMsgData>(wideCharacter), 0);
	}
}

/**
 * Handle keyboard event -dispatch to Keyboard manager
 * TheSuperHackers @build 10/02/2026 BenderAI - Phase 1.5 event wiring
 */
void SDL3GameEngine::handleKeyboardEvent(const SDL_KeyboardEvent& event)
{
	// Dispatch to SDL3Keyboard if available
	if (TheKeyboard) {
		SDL3Keyboard* sdlKeyboard = dynamic_cast<SDL3Keyboard*>(TheKeyboard);
		if (sdlKeyboard) {
			sdlKeyboard->addSDL3KeyEvent(event);
		}
	}
}

/**
 * Handle mouse motion event - dispatch to Mouse manager
 * TheSuperHackers @build 10/02/2026 BenderAI - Phase 1.5 event wiring
 */
void SDL3GameEngine::handleMouseMotionEvent(const SDL_MouseMotionEvent& event)
{
	// Dispatch to SDL3Mouse if available
	if (TheMouse) {
		SDL3Mouse* sdlMouse = dynamic_cast<SDL3Mouse*>(TheMouse);
		if (sdlMouse) {
			sdlMouse->addSDL3MouseMotionEvent(event);
		}
	}
}

/**
 * Handle mouse button event - dispatch to Mouse manager
 * TheSuperHackers @build 10/02/2026 BenderAI - Phase 1.5 event wiring
 */
void SDL3GameEngine::handleMouseButtonEvent(const SDL_MouseButtonEvent& event)
{
	// Dispatch to SDL3Mouse if available
	if (TheMouse) {
		SDL3Mouse* sdlMouse = dynamic_cast<SDL3Mouse*>(TheMouse);
		if (sdlMouse) {
			sdlMouse->addSDL3MouseButtonEvent(event);
		}
	}
}

/**
 * Handle mouse wheel event - dispatch to Mouse manager
 * TheSuperHackers @build 10/02/2026 BenderAI - Phase 1.5 event wiring
 */
void SDL3GameEngine::handleMouseWheelEvent(const SDL_MouseWheelEvent& event)
{
	// Dispatch to SDL3Mouse if available
	if (TheMouse) {
		SDL3Mouse* sdlMouse = dynamic_cast<SDL3Mouse*>(TheMouse);
		if (sdlMouse) {
			sdlMouse->addSDL3MouseWheelEvent(event);
		}
	}
}

/**
 * Handle window event (resize, etc.)
 */
void SDL3GameEngine::handleWindowEvent(const SDL_WindowEvent& event)
{
	// TODO: Phase 2 - Handle window resize, notify graphics subsystem
	// fprintf(stderr, "DEBUG: Window event (type=%d)\n", event.type);
}

/**
 * Factory Methods for GameEngine subsystems
 * TheSuperHackers @build felipebraz 13/02/2026
 * Implementations in .cpp to provide complete type definitions and avoid circular includes
 */

LocalFileSystem *SDL3GameEngine::createLocalFileSystem(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createLocalFileSystem() -> StdLocalFileSystem\n");
	return NEW StdLocalFileSystem;
}

ArchiveFileSystem *SDL3GameEngine::createArchiveFileSystem(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createArchiveFileSystem() -> StdBIGFileSystem\n");
	return NEW StdBIGFileSystem;
}

GameLogic *SDL3GameEngine::createGameLogic(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createGameLogic() -> W3DGameLogic\n");
	return NEW W3DGameLogic;
}

GameClient *SDL3GameEngine::createGameClient(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createGameClient() -> W3DGameClient\n");
	return NEW W3DGameClient;
}

ModuleFactory *SDL3GameEngine::createModuleFactory(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createModuleFactory() -> W3DModuleFactory\n");
	return NEW W3DModuleFactory;
}

ThingFactory *SDL3GameEngine::createThingFactory(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createThingFactory() -> W3DThingFactory\n");
	return NEW W3DThingFactory;
}

FunctionLexicon *SDL3GameEngine::createFunctionLexicon(void)
{
	fprintf(stderr, "INFO: SDL3GameEngine::createFunctionLexicon() -> W3DFunctionLexicon\n");
	return NEW W3DFunctionLexicon;
}

// GeneralsX @bugfix Copilot 15/04/2026 Match upstream GameEngine pure-virtual signature after sync.
Radar *SDL3GameEngine::createRadar(Bool dummy)
{
	// GeneralsX @bugfix fbraz 04/05/2026 Respect headless mode and create dummy radar.
	// Upstream reference: Win32GameEngine headless factory behavior, TheSuperHackers/GeneralsGameCode
	// https://github.com/TheSuperHackers/GeneralsGameCode
	if (dummy) {
		fprintf(stderr, "INFO: SDL3GameEngine::createRadar() -> RadarDummy (headless)\n");
		return NEW RadarDummy;
	}
	fprintf(stderr, "INFO: SDL3GameEngine::createRadar() -> W3DRadar\n");
	return NEW W3DRadar;
}

// GeneralsX @bugfix Copilot 24/03/2026 Match upstream GameEngine pure-virtual signature after sync.
ParticleSystemManager* SDL3GameEngine::createParticleSystemManager(Bool dummy)
{
	// GeneralsX @bugfix fbraz 04/05/2026 Respect headless mode and create dummy particle manager.
	if (dummy) {
		fprintf(stderr, "INFO: SDL3GameEngine::createParticleSystemManager() -> ParticleSystemManagerDummy (headless)\n");
		return NEW ParticleSystemManagerDummy;
	}
	fprintf(stderr, "INFO: SDL3GameEngine::createParticleSystemManager() -> W3DParticleSystemManager\n");
	return NEW W3DParticleSystemManager;
}

WebBrowser *SDL3GameEngine::createWebBrowser(void)
{
	// WebBrowser uses Windows COM (CComObject<W3DWebBrowser>)
	// Not available on Linux - return nullptr
	fprintf(stderr, "WARNING: WebBrowser not available on Linux platform\n");
	return nullptr;
}

/**
 * Factory method: AudioManager
 * Select audio backend based on compile flags
 * GeneralsX @bugfix Copilot 15/04/2026 Match upstream GameEngine pure-virtual signature after sync.
 */
AudioManager *SDL3GameEngine::createAudioManager(Bool dummy)
{
	(void)dummy;
	fprintf(stderr, "INFO: SDL3GameEngine::createAudioManager()\n");

#ifdef SAGE_USE_OPENAL
	fprintf(stderr, "INFO: Creating OpenAL audio backend\n");
	return new OpenALAudioManager();
#else
	fprintf(stderr, "INFO: Audio backend not available (SAGE_USE_OPENAL not defined)\n");
	fprintf(stderr, "WARNING: Falls back to parent implementation or silent mode\n");
	return GameEngine::createAudioManager();  // Call parent (may return stub)
#endif
}

#endif // !_WIN32
