#ifndef __SDL_COMMON_H
#define __SDL_COMMON_H

#include <stdint.h>
#include <SDL/SDL.h>

int SDL_SetPixel(SDL_Surface *screen, int x, int y, uint32_t pixel);

#endif
