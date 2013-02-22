#ifndef __CGA_H
#define __CGA_H

#include <stdint.h>
#include <SDL/SDL.h>

#define CGA_MODE_TEXT_40x25		0
#define CGA_MODE_TEXT_80x25		1
#define CGA_MODE_GRAPHICS_320x200	2
#define CGA_MODE_GRAPHICS_640x200	3

#define CGA_PALETTE_0			0
#define CGA_PALETTE_1			1
#define CGA_INTENSITY_NORMAL		0
#define CGA_INTENSITY_HIGH		8	/* magic value, don't change */

struct cga_context
{
	uint16_t	palette:4;		/* palette number */
	uint16_t	intensity:4;		/* palette intensity */
	SDL_Color	background_color;	/* palette color of background */
	SDL_Surface	*surface;		/* surface for this CGA display */
	int		mode;			/* CGA display mode */

	/* font related data */
	SDL_Surface	*fonts;
	int		font_type;		/* thick or thin */
	int		font_color;		/* foreground color */

	/* cursor position; full SDL_Rect struct for speed. */
	SDL_Rect	cursor_pos;
};

/* A sprite structure.
 *
 * It differs from struct cga_image as it has on-screen coordinates, and it
 * is a collection of image frames, which can be blitted by index.
 *
 * Note that all image frames should have the same dimensions, but frames
 * can be stored in 'surface' on multiple rows.
 */
struct cga_sprite
{
	SDL_Surface	*surface;

	int		x;
	int		y;
	int		width;
	int		height;
	int		frames;		/* number of frames in this sprite */
};

struct cga_image
{
	SDL_Surface	*surface;
};

/* This structure is used to store flat CGA images in a format suitable for
 * directly blitting to CGA hardware video memory.
 */
struct cga_image_raw
{
        uint8_t         *data;
        size_t          size;		/* data in use in bytes */
	size_t		allocated;	/* data allocated in bytes */

        /* dimensions of the raw image in pixels */
        uint16_t        width;
        uint16_t        height;
};

int cga_set_surface(struct cga_context *cga, SDL_Surface *surface);
void cga_floodfill(struct cga_context *cga, int color);

/* low-level CGA video memory access routines */
void cga_poke_byte(struct cga_context *cga, int loc, uint8_t byte);
void cga_poke_bytes(struct cga_context *cga, int loc, uint8_t byte, size_t count);
void cga_poke_word(struct cga_context *cga, int loc, uint16_t word);
void cga_poke_words(struct cga_context *cga, int loc, uint16_t word, size_t count);
void cga_poke_image(struct cga_context *cga, struct cga_image_raw *image, int loc);
void cga_poke_overlay(struct cga_context *cga, struct cga_sprite *sprite, int loc);

uint8_t cga_peek_byte(struct cga_context *cga, int loc);
void cga_peek_bytes(struct cga_context *cga, int loc, uint8_t *bytes, size_t count);
void cga_peek_image(struct cga_context *cga, struct cga_sprite *sprite, int loc);

/* high-level CGA drawing routines */
void cga_draw_image(struct cga_context *cga, struct cga_sprite *sprite, int x, int y);
void cga_draw_overlay(struct cga_context *cga, struct cga_sprite *sprite, int x, int y);

void cga_read_sprite(struct cga_context *cga, struct cga_sprite *sprite, int x, int y);


/* CGA sprite and image handling */
int cga_image_load(struct cga_image *image, struct cga_image_raw *raw);
int cga_image_blit(struct cga_context *cga, struct cga_image *image, int x, int y);

int cga_sprite_load(struct cga_sprite *sprite, ...);
int cga_sprite_set_colorkey(struct cga_sprite *sprite, int color);
int cga_print_image(struct cga_image *image);

#endif
