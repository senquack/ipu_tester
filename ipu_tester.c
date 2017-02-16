/***************************************************************************
 *  Copyright (c) 2017 Daniel Silsby (senquack)                            *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include <SDL_ttf.h>

const int SCREEN_SDL_FLAGS_BASE = SDL_HWSURFACE;
const int SCREEN_W_MAX = 640;
const int SCREEN_W_MIN = 1;
const int SCREEN_H_MAX = 480;
const int SCREEN_H_MIN = 1;
#ifdef SDL_TRIPLEBUF
	const int SCREEN_BUFFERS_MAX = 3;
#else
	const int SCREEN_BUFFERS_MAX = 2;
#endif
const int SCREEN_BUFFERS_MIN = 1;
const int screen_adjust_step_MAX = 64;
const int AUTO_RANGE_TEST_INTERVAL_MAX = 1000;
const int AUTO_RANGE_TEST_INTERVAL_MIN = 1;
static int auto_range_test_interval = 90; // 90ms seems to work well as default
static int screen_bpp = 16;
static int screen_w = 320;
static int screen_h = 240;
static int screen_buffers = 2;
static int screen_adjust_step = 16;
static SDL_Surface *screen;

static TTF_Font *font;
const char *font_file = "font/synchronizer_nbp.ttf";
const int font_size = 16;
const SDL_Color font_color = { 255, 255, 255 };

void video_setmode(int w, int h)
{
	unsigned sdl_flags = SCREEN_SDL_FLAGS_BASE;
	const char *buffering = "single";
	if (screen_buffers == 2) {
		sdl_flags |= SDL_DOUBLEBUF;
		buffering = "double";
	} else if (screen_buffers == 3) {
#ifdef SDL_TRIPLEBUF
		sdl_flags |= SDL_TRIPLEBUF;
		buffering = "triple";
#endif
	}

	printf("Setting vid mode %dx%dx%d  (%s buffering)\n", w, h, screen_bpp, buffering);
	screen = SDL_SetVideoMode(w, h, screen_bpp, sdl_flags);

	if (!screen) {
		printf("ERROR SDL_SetVideoMode: %s\n", SDL_GetError());
		exit(1);
	}

	if (screen->w != w ||
	    screen->h != h ||
	    screen->format->BitsPerPixel != screen_bpp)
	{
		printf("WARNING: unexpected SDL screen width/height/depth\n"
			   " Requested %dx%dx%d, got %dx%d%d\n",
			   w, h, screen_bpp, screen->w, screen->h, screen->format->BitsPerPixel);
	}
}

void load_font(const char *fname, int size)
{
	if (font)
		return;
	char *p;
	font = TTF_OpenFont(fname, size);
	if (!font) {
		printf("Error loading font file %s.\n", fname);
		printf("ERROR TTF_OpenFont: %s\n", TTF_GetError());
		exit(1);
	}
}

void free_font()
{
	if (font)
		TTF_CloseFont(font);
	font = NULL;
}

void screen_update()
{
	// Fill screen with black, and draw a 2-pixel-width white border
	boxRGBA(screen, 0, 0, screen->w-1, screen->h-1, 0, 0, 0, 255);
	rectangleRGBA(screen, 0, 0, screen->w-1, screen->h-1, 255, 255, 255, 255);
	rectangleRGBA(screen, 1, 1, screen->w-2, screen->h-2, 255, 255, 255, 255);

	// Draw current screen resolution and adjustment increment
	SDL_Surface *text_surf;
	SDL_Surface *text_surf2;
	char msg[80];
	sprintf(msg, "Res: %dx%d", screen_w, screen_h);
	text_surf = TTF_RenderText_Solid(font, msg, font_color);
	sprintf(msg, "Incr: %d", screen_adjust_step);
	text_surf2 = TTF_RenderText_Solid(font, msg, font_color);

	if (!text_surf || !text_surf2) {
		printf("ERROR TTF_RenderTextSolid: %s\n", TTF_GetError());
		exit(1);
	}

	SDL_Rect dst_rect;
	dst_rect.x = screen_w/2 - text_surf->w/2;
	dst_rect.y = screen_h/2 - text_surf->h - 2;
	dst_rect.w = text_surf->w;
	dst_rect.h = text_surf->h;
	SDL_BlitSurface(text_surf, NULL, screen, &dst_rect);
	SDL_FreeSurface(text_surf);

	dst_rect.x = screen_w/2 - text_surf2->w/2;
	dst_rect.y = screen_h/2 + 2;
	dst_rect.w = text_surf2->w;
	dst_rect.h = text_surf2->h;
	SDL_BlitSurface(text_surf2, NULL, screen, &dst_rect);
	SDL_FreeSurface(text_surf2);

	SDL_Flip(screen);
}

int main(int argc, char **argv)
{
	printf("IPU screen-scaling tester (C) 2017 Daniel Silsby (senquack)\n");

	// command line options
	int auto_range_test_w = 0;
	int auto_range_test_h = 0;
	int screen_no_draw = 0;
	int param_parse_error = 0;
	int param_range_error = 0;
	int param_range_min = -1;
	int param_range_max = -1;
	int i;
	for (i = 1; i < argc; i++)
	{
		if (0 == strcmp(argv[i], "-h") ||
		    0 == strcmp(argv[i], "-?"))
		{
			printf("Optional params:                                           \n"
			       " -W x   Set start screen width,   1..%d (default: %d)      \n"
			       " -H x   Set start screen height,  1..%d (default: %d)      \n"
			       " -D x   Set screen depth,         16,32  (default: %d)     \n"
				   " -B x   Set screen buffers,       1..3   (default: 2)      \n"
				   "         (2 is double buffering, 3 is triple buffering)    \n"
				   " -I x   Set adjustment step,      1..%d  (default: %d)     \n"
				   " -AW    Automatically test entire horizontal range,        \n"
				   "         starting from maximum supported width             \n"
				   "         (use -I param to specify increment amount)        \n"
				   " -AH    Automatically test entire vertical range,          \n"
				   "         starting from maximum supported height            \n"
				   "         (use -I param to specify increment amount)        \n"
				   " -AD x  Set interval in ms between automatic test steps,   \n"
				   "         %d..%d (default: %d)                              \n"
				   " -ND    Draw nothing to screen, to prove that any issues   \n"
				   "         encountered could not possibly be related         \n",
				   SCREEN_W_MAX, screen_w,
				   SCREEN_H_MAX, screen_h,
				   screen_bpp,
				   screen_adjust_step_MAX, screen_adjust_step,
			       AUTO_RANGE_TEST_INTERVAL_MIN, AUTO_RANGE_TEST_INTERVAL_MAX, auto_range_test_interval
				  );
			exit(0);
		}
		else if (0 == strcmp(argv[i], "-W"))
		{
			if (i+1 >= argc) {
				param_parse_error = 1;
				break;
			}

			++i;
			param_range_min = SCREEN_W_MIN;
			param_range_max = SCREEN_W_MAX;
			int tmp = atoi(argv[i]);
			if (tmp < param_range_min || tmp > param_range_max) {
				param_range_error = 1;
				break;
			}

			screen_w = tmp;
		}
		else if (0 == strcmp(argv[i], "-H"))
		{
			if (i+1 >= argc) {
				param_parse_error = 1;
				break;
			}

			++i;
			param_range_min = SCREEN_H_MIN;
			param_range_max = SCREEN_H_MAX;
			int tmp = atoi(argv[i]);
			if (tmp < param_range_min || tmp > param_range_max) {
				param_range_error = 1;
				break;
			}

			screen_h = tmp;
		}
		else if (0 == strcmp(argv[i], "-B"))
		{
			if (i+1 >= argc) {
				param_parse_error = 1;
				break;
			}

			++i;
			param_range_min = SCREEN_BUFFERS_MIN;
			param_range_max = SCREEN_BUFFERS_MAX;
			int tmp = atoi(argv[i]);
			if (tmp < param_range_min || tmp > param_range_max) {
				param_range_error = 1;
				break;
			}

			screen_buffers = tmp;
		}
		else if (0 == strcmp(argv[i], "-D"))
		{
			if (i+1 >= argc) {
				param_parse_error = 1;
				break;
			}

			++i;
			int tmp = atoi(argv[i]);
			if (tmp != 16 && tmp != 32) {
				printf("Error: only screen depths 16,32 supported\n");
				exit(1);
			}

			screen_bpp = tmp;
		}
		else if (0 == strcmp(argv[i], "-I"))
		{
			if (i+1 >= argc) {
				param_parse_error = 1;
				break;
			}

			++i;
			param_range_min = 1;
			param_range_max = screen_adjust_step_MAX;
			int tmp = atoi(argv[i]);
			if (tmp < param_range_min || tmp > param_range_max) {
				param_range_error = 1;
				break;
			}

			screen_adjust_step = tmp;
		}
		else if (0 == strcmp(argv[i], "-AW"))
		{
			auto_range_test_w = 1;
		}
		else if (0 == strcmp(argv[i], "-AH"))
		{
			auto_range_test_h = 1;
		}
		else if (0 == strcmp(argv[i], "-AD"))
		{
			if (i+1 >= argc) {
				param_parse_error = 1;
				break;
			}

			++i;
			param_range_min = AUTO_RANGE_TEST_INTERVAL_MIN;
			param_range_max = AUTO_RANGE_TEST_INTERVAL_MAX;
			int tmp = atoi(argv[i]);
			if (tmp < param_range_min || tmp > param_range_max) {
				param_range_error = 1;
				break;
			}

			auto_range_test_interval = tmp;
		}
		else if (0 == strcmp(argv[i], "-ND"))
		{
			screen_no_draw = 1;
		}
		else
		{
			printf("Error: unrecognized parameter '%s'\n", argv[i]);
			exit(1);
		}
	}

	if (param_parse_error) {
		printf("Error: missing argument for option '%s'\n", argv[i]);
		exit(1);
	}

	if (param_range_error) {
		printf("Error: argument for option '%s' out of range %d..%d\n",
		       argv[i-1], param_range_min, param_range_max);
		exit(1);
	}

   printf("Controls:\n"
          " DPAD up:      Increase screen height\n"
          " DPAD down:    Decrease screen height\n"
          " DPAD right:   Increase screen width\n"
          " DPAD left:    Decrease screen width\n"
          " Button A:     Increase adjustment step\n"
          " Button B:     Decrease adjustment step\n"
          " Start/Select: Exit\n\n");

	if (TTF_Init() == -1) {
		printf("ERROR TTF_Init: %s\n", TTF_GetError());
		exit(1);
	}

	load_font(font_file, font_size);

	printf("Initializing SDL.\n");
	if (SDL_Init(SDL_INIT_VIDEO) == -1) {
		printf("ERROR SDL_Init: %s\n", SDL_GetError());
		exit(1);
	}

	atexit(SDL_Quit); /* remember to quit SDL */
	atexit(TTF_Quit); /* remember to quit SDL_ttf */
	atexit(free_font);
	SDL_ShowCursor(SDL_DISABLE);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	
	if (auto_range_test_w) {
		screen_w = SCREEN_W_MAX;
	}
	if (auto_range_test_h) {
		screen_h = SCREEN_H_MAX;
	}

	video_setmode(screen_w, screen_h);
	if (!screen_no_draw)
		screen_update();

	unsigned last_auto_range_test = SDL_GetTicks();
	int quit = 0;
	do
	{
		if (auto_range_test_w || auto_range_test_h)
		{
			unsigned ticks_now = SDL_GetTicks();
			if ((ticks_now - last_auto_range_test) >= auto_range_test_interval)
			{
				int new_mode = 0;
				last_auto_range_test = ticks_now;

				if (auto_range_test_w) {
					if ((screen_w - screen_adjust_step) >= SCREEN_W_MIN) {
						screen_w -= screen_adjust_step;
						new_mode = 1;
					}
				}

				if (auto_range_test_h) {
					if ((screen_h - screen_adjust_step) >= SCREEN_H_MIN) {
						screen_h -= screen_adjust_step;
						new_mode = 1;
					}
				}
				
				if (new_mode)
					video_setmode(screen_w, screen_h);
			}
		}

		if (!screen_no_draw)
			screen_update();

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				quit = 1;
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
					case SDLK_ESCAPE:
					case SDLK_RETURN:
						quit = 1;
						break;

					case SDLK_UP:
						if ((screen_h + screen_adjust_step) <= SCREEN_H_MAX) {
							screen_h += screen_adjust_step;
							video_setmode(screen_w, screen_h);
						}
						break;

					case SDLK_DOWN:
						if ((screen_h - screen_adjust_step) >= SCREEN_H_MIN) {
							screen_h -= screen_adjust_step;
							video_setmode(screen_w, screen_h);
						}
						break;

					case SDLK_RIGHT:
						if ((screen_w + screen_adjust_step) <= SCREEN_W_MAX) {
							screen_w += screen_adjust_step;
							video_setmode(screen_w, screen_h);
						}
						break;

					case SDLK_LEFT:
						if ((screen_w - screen_adjust_step) >= SCREEN_W_MIN) {
							screen_w -= screen_adjust_step;
							video_setmode(screen_w, screen_h);
						}
						break;

					case SDLK_LCTRL:  // Button 'A'
						screen_adjust_step++;
						break;

					case SDLK_LALT:   // Button 'B'
						screen_adjust_step--;
						if (screen_adjust_step <= 0)
							screen_adjust_step = 1;
						break;

					default:
						break;
				}
				break;

			default: break;
			}
		}
	} while (!quit);

	printf("Exiting normally.\n");

	exit(0);	
}
