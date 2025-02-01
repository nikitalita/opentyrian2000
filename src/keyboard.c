/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) 2007-2009  The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "keyboard.h"

#include "config.h"
#include "joystick.h"
#include "mouse.h"
#include "network.h"
#include "opentyr.h"
#include "video.h"
#include "video_scale.h"

#ifdef WITH_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include <stdio.h>

JE_boolean ESCPressed;

JE_boolean newkey, newmouse, keydown, mousedown;
SDL_Scancode lastkey_scan;
SDL_Keymod lastkey_mod;
Uint8 lastmouse_but;
Sint32 lastmouse_x, lastmouse_y;
JE_boolean mouse_pressed[4] = {false, false, false, false};
Sint32 mouse_x, mouse_y;
bool windowHasFocus = false;

#ifdef WITH_SDL3
Uint8 keysactive[SDL_SCANCODE_COUNT] = { 0 };
#else
Uint8 keysactive[SDL_NUM_SCANCODES] = { 0 };
#endif

bool new_text;
char last_text[SDL_TEXTINPUTEVENT_TEXT_SIZE];

static bool mouseRelativeEnabled;

// Relative mouse position in window coordinates.
static Sint32 mouseWindowXRelative;
static Sint32 mouseWindowYRelative;

void flush_events_buffer(void)
{
	SDL_Event ev;

	while (SDL_PollEvent(&ev));
}

void wait_input(JE_boolean keyboard, JE_boolean mouse, JE_boolean joystick)
{
	service_SDL_events(false);
	while (!((keyboard && keydown) || (mouse && mousedown) || (joystick && joydown)))
	{
		SDL_Delay(SDL_POLL_INTERVAL);
		push_joysticks_as_keyboard();
		service_SDL_events(false);

#ifdef WITH_NETWORK
		if (isNetworkGame)
			network_check();
#endif
	}
}

void wait_noinput(JE_boolean keyboard, JE_boolean mouse, JE_boolean joystick)
{
	service_SDL_events(false);
	while ((keyboard && keydown) || (mouse && mousedown) || (joystick && joydown))
	{
		SDL_Delay(SDL_POLL_INTERVAL);
		poll_joysticks();
		service_SDL_events(false);

#ifdef WITH_NETWORK
		if (isNetworkGame)
			network_check();
#endif
	}
}

void init_keyboard(void)
{
	//SDL_EnableKeyRepeat(500, 60); TODO Find if SDL2 has an equivalent.

	newkey = newmouse = false;
	keydown = mousedown = false;

#ifdef WITH_SDL3
    SDL_HideCursor();
#else
	SDL_ShowCursor(SDL_FALSE);
#endif

#if SDL_VERSION_ATLEAST(2, 26, 0)
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "1");
#endif
}

void mouseSetRelative(bool enable)
{
#ifdef WITH_SDL3
    SDL_SetWindowRelativeMouseMode(main_window, enable && windowHasFocus);
#else
	SDL_SetRelativeMouseMode(enable && windowHasFocus);
#endif

	mouseRelativeEnabled = enable;

	mouseWindowXRelative = 0;
	mouseWindowYRelative = 0;
}

JE_word JE_mousePosition(JE_word *mouseX, JE_word *mouseY)
{
	service_SDL_events(false);
	*mouseX = mouse_x;
	*mouseY = mouse_y;
	return mousedown ? lastmouse_but : 0;
}

void mouseGetRelativePosition(Sint32 *const out_x, Sint32 *const out_y)
{
	service_SDL_events(false);

	scaleWindowDistanceToScreen(&mouseWindowXRelative, &mouseWindowYRelative);
	*out_x = mouseWindowXRelative;
	*out_y = mouseWindowYRelative;

	mouseWindowXRelative = 0;
	mouseWindowYRelative = 0;
}

void service_SDL_events(JE_boolean clear_new)
{
	SDL_Event ev;
#ifdef WITH_SDL3
    Sint32 bx = 0;
    Sint32 by = 0;
    Sint32 mx = 0;
    Sint32 my = 0;
    Sint32 mxrel = 0;
    Sint32 myrel = 0;
#endif

	if (clear_new)
	{
		newkey = false;
		newmouse = false;
		new_text = false;
	}

	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
#ifndef WITH_SDL3
			case SDL_WINDOWEVENT:
				switch (ev.window.event)
                {
#endif
#ifdef WITH_SDL3
                case SDL_EVENT_WINDOW_FOCUS_LOST:
#else
				case SDL_WINDOWEVENT_FOCUS_LOST:
#endif
					windowHasFocus = false;

					mouseSetRelative(mouseRelativeEnabled);
					break;

#ifdef WITH_SDL3
                case SDL_EVENT_WINDOW_FOCUS_GAINED:
#else
				case SDL_WINDOWEVENT_FOCUS_GAINED:
#endif
					windowHasFocus = true;

					mouseSetRelative(mouseRelativeEnabled);
					break;

#ifdef WITH_SDL3
                case SDL_EVENT_WINDOW_RESIZED:
#else
				case SDL_WINDOWEVENT_RESIZED:
#endif
					video_on_win_resize();
					break;
#ifndef WITH_SDL3
				}
				break;
#endif

#ifdef WITH_SDL3
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.mod & SDL_KMOD_ALT && ev.key.scancode == SDL_SCANCODE_RETURN)
#else
			case SDL_KEYDOWN:
                if (ev.key.keysym.mod & KMOD_ALT && ev.key.keysym.scancode == SDL_SCANCODE_RETURN)
#endif
				/* <alt><enter> toggle fullscreen */
				{
					toggle_fullscreen();
					break;
				}

#ifdef WITH_SDL3
                keysactive[ev.key.scancode] = 1;
#else
				keysactive[ev.key.keysym.scancode] = 1;
#endif

				newkey = true;
#ifdef WITH_SDL3
                lastkey_scan = ev.key.scancode;
                lastkey_mod = ev.key.mod;
#else
				lastkey_scan = ev.key.keysym.scancode;
				lastkey_mod = ev.key.keysym.mod;
#endif
				keydown = true;

				mouseInactive = true;
				return;

#ifdef WITH_SDL3
            case SDL_EVENT_KEY_UP:
                keysactive[ev.key.scancode] = 0;
#else
			case SDL_KEYUP:
                keysactive[ev.key.keysym.scancode] = 0;
#endif
				keydown = false;
				return;

#ifdef WITH_SDL3
            case SDL_EVENT_MOUSE_MOTION:
                mx = (Sint32)ev.motion.x;
                my = (Sint32)ev.motion.y;
                mouse_x = mx;
                mouse_y = my;
#else
			case SDL_MOUSEMOTION:
                mouse_x = ev.motion.x;
                mouse_y = ev.motion.y;
                mxrel = 0;
                myrel = 0;
#endif

				mapWindowPointToScreen(&mouse_x, &mouse_y);

				if (mouseRelativeEnabled && windowHasFocus)
				{
#ifdef WITH_SDL3
                    if ((ev.motion.xrel > 0) && (ev.motion.xrel < 1))
                    {
                        mxrel = 1;
                    } else if ((ev.motion.xrel < 0) && (ev.motion.xrel > 1)) {
                        mxrel = -1;
                    } else {
                        mxrel = (Sint32)ev.motion.xrel;
                    }

                    if ((ev.motion.yrel > 0) && (ev.motion.yrel < 1))
                    {
                        myrel = 1;
                    } else if ((ev.motion.yrel < 0) && (ev.motion.yrel > -1)) {
                        myrel = -1;
                    } else {
                        myrel = (Sint32)ev.motion.yrel;
                    }

					mouseWindowXRelative += mxrel;
					mouseWindowYRelative += myrel;
#else
                    mouseWindowXRelative += ev.motion.xrel;
                    mouseWindowYRelative += ev.motion.yrel;
#endif
				}

				// Show system mouse pointer if outside screen.
#ifdef WITH_SDL3
                if (mouse_x < 0 || mouse_x >= vga_width ||
                    mouse_y < 0 || mouse_y >= vga_height ? true : false)
                {
                    SDL_ShowCursor();
                } else {
                    SDL_HideCursor();
                }
#else
				SDL_ShowCursor(mouse_x < 0 || mouse_x >= vga_width ||
				               mouse_y < 0 || mouse_y >= vga_height ? SDL_TRUE : SDL_FALSE);
#endif

#ifdef WITH_SDL3
                if (mxrel != 0 || myrel != 0)
#else
				if (ev.motion.xrel != 0 || ev.motion.yrel != 0)
#endif
					mouseInactive = false;
				break;

#ifdef WITH_SDL3
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
#else
			case SDL_MOUSEBUTTONDOWN:
#endif
				mouseInactive = false;

				// fall through
#ifdef WITH_SDL3
            case SDL_EVENT_MOUSE_BUTTON_UP:
                bx = (Sint32)ev.button.x;
                by = (Sint32)ev.button.y;

                mapWindowPointToScreen((Sint32 *)&bx, (Sint32 *)&by);

#else
			case SDL_MOUSEBUTTONUP:
                bx = (Sint32)ev.button.x;
                by = (Sint32)ev.button.y;

                mapWindowPointToScreen((Sint32 *)&bx, (Sint32 *)&by);
#endif

#ifdef WITH_SDL3
                if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
#else
				if (ev.type == SDL_MOUSEBUTTONDOWN)
#endif
				{
					newmouse = true;
					lastmouse_but = ev.button.button;
#ifdef WITH_SDL3
                    lastmouse_x = bx;
                    lastmouse_y = by;
#else
					lastmouse_x = ev.button.x;
					lastmouse_y = ev.button.y;
#endif
					mousedown = true;
				}
				else
				{
					mousedown = false;
				}

				int whichMB = -1;
				switch (ev.button.button)
				{
					case SDL_BUTTON_LEFT:   whichMB = 0; break;
					case SDL_BUTTON_RIGHT:  whichMB = 1; break;
					case SDL_BUTTON_MIDDLE: whichMB = 2; break;
				}

				if (whichMB < 0)
					break;

				switch (mouseSettings[whichMB])
				{
					case 1: // Fire Main Weapons
						mouse_pressed[0] = mousedown;
						break;
					case 2: // Fire Left Sidekick
						mouse_pressed[1] = mousedown;
						break;
					case 3: // Fire Right Sidekick
						mouse_pressed[2] = mousedown;
						break;
					case 4: // Fire BOTH Sidekicks
						mouse_pressed[1] = mousedown;
						mouse_pressed[2] = mousedown;
						break;
					case 5: // Change Rear Mode
						mouse_pressed[3] = mousedown;
						break;
				}
				break;

#ifdef WITH_SDL3
            case SDL_EVENT_TEXT_INPUT:
#else
			case SDL_TEXTINPUT:
#endif
				SDL_strlcpy(last_text, ev.text.text, COUNTOF(last_text));
				new_text = true;
				break;

#ifdef WITH_SDL3
            case SDL_EVENT_TEXT_EDITING:
#else
			case SDL_TEXTEDITING:
#endif
				break;

#ifdef WITH_SDL3
            case SDL_EVENT_QUIT:
#else
			case SDL_QUIT:
#endif
				/* TODO: Call the cleanup code here. */
				exit(0);
				break;
		}
	}
}

void JE_clearKeyboard(void)
{
	// /!\ Doesn't seems important. I think. D:
}
