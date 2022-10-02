#pragma once

#include <stdbool.h>

#ifdef __APPLE__
#   include <OpenGL/gl3.h>
#   include <SDL2/SDL.h>
#elif _WIN32
#   define HAVE_M_PI
#   define SDL_MAIN_HANDLED
#   include <GL/glew.h>
#   include <SDL.h>
#else
#   include <GL/glew.h>
#   include <SDL2/SDL.h>
#endif

extern void context_init( const char *title, int width, int height );
extern bool context_flip_frame_poll_events( void );
extern SDL_GameController *context_get_controller( void );
extern void context_terminate( void );
