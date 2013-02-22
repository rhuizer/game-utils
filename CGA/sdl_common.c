#include <stdint.h>
#include <SDL/SDL.h>

union __ptr {
	void		*ptr;
	uint8_t		*ptr8;
	uint16_t	*ptr16;
	uint32_t	*ptr32;
};

/* Slow, albeit portable. */
int SDL_SetPixel(SDL_Surface *screen, int x, int y, uint32_t colour)
{
	union __ptr ptr;
	register int index = x + screen->pitch * y;

	ptr.ptr = screen->pixels;
	switch(screen->format->BitsPerPixel) {
	case 8:
		ptr.ptr8[index] = (uint8_t)colour;
		break;
	case 16:
		ptr.ptr16[index] = (uint16_t)colour;
		break;
	case 24:
		index *= 3;
		ptr.ptr8[index] = (uint8_t)colour;
		ptr.ptr8[index + 1] = (uint8_t)(colour >> 8);
		ptr.ptr8[index + 2] = (uint8_t)(colour >> 16);
		break;
	case 32:
		ptr.ptr32[index] = (uint32_t)colour;
		break;
	default:
		return -1;
	}

	return 0;
}
