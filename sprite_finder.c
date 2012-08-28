#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <GL/glut.h>

struct rgb_t
{
	int r;
	int g;
	int b;
};

struct graphics_t
{
	int width, height;
	int gl_line_bytes;
	int gl_texture_bytes;
	unsigned char *gl_texture;
	unsigned char *font;
	unsigned char *c64snapshot;
	struct rgb_t palette[16];
} graphics_global;

#define C64SNAPSHOT_SIZE 69948

static unsigned char *init_c64snapshot(const char *filename)
{
	FILE *fp;
	unsigned char *snap;
	
	fp = fopen(filename, "rb");
	if (fp == NULL)
	{
		return NULL;
	}
	
	snap = calloc(C64SNAPSHOT_SIZE, 1);
	if (snap)
	{
		if (-1 == fread(snap, 1, C64SNAPSHOT_SIZE, fp))
		{
			free(snap);
			return NULL;
		}
		fclose(fp);
	}

	return snap;
}

static unsigned char *init_font(const char *filename)
{
	FILE *fp;
	unsigned char *font;
	
	fp = fopen(filename, "rb");
	if (fp == NULL)
	{
		return NULL;
	}
	
	font = calloc(4096, 1);
	if (font)
	{
		if (4096 != fread(font, 1, 4096, fp))
		{
			free(font);
			return NULL;
		}
		fclose(fp);
	}

	return font;
}

static void init_palette(struct rgb_t *rgb)
{
	int i, color;
	int c64_palette[16] = {
		0x101010, 0xffffff, 0xe04040, 0x60ffff,
		0xe060e0, 0x40e040, 0x4040e0, 0xffff40,
		0xe0a040, 0x9c7448, 0xffa0a0, 0x545454,
		0x888888, 0xa0ffa0, 0xa0a0ff, 0xc0c0c0
	};

	for(i = 0; i < 16; ++i)
	{
		color = c64_palette[i];
		rgb[i].r = (color >> 16) & 0xff;
		rgb[i].g = (color >>  8) & 0xff;
		rgb[i].b = (color >>  0) & 0xff;		
	}
}

static void print_symbol(struct graphics_t *gc, int x0, int y0, int symbol, int scale, int bg, int fg)
{
	int row, column, x, y, xx, yy;
	unsigned char *font_ptr, pixels, pixel;
	int bg0, bg1, fg0, fg1;
	unsigned char bgr[2], bgg[2], bgb[2];
	unsigned char fgr[2], fgg[2], fgb[2];
	int toggler;
	int texture_index;
	unsigned char *tp, *pr, *pg, *pb;
		
	bg0 = bg & 0x0f; bg1 = (bg >> 4) & 0x0f;
	fg0 = fg & 0x0f; fg1 = (fg >> 4) & 0x0f;
	
	bgr[0] = gc->palette[bg0].r; bgr[1] = gc->palette[bg1].r;
	bgg[0] = gc->palette[bg0].g; bgg[1] = gc->palette[bg1].g;
	bgb[0] = gc->palette[bg0].b; bgb[1] = gc->palette[bg1].b;

	fgr[0] = gc->palette[fg0].r; fgr[1] = gc->palette[fg1].r;
	fgg[0] = gc->palette[fg0].g; fgg[1] = gc->palette[fg1].g;
	fgb[0] = gc->palette[fg0].b; fgb[1] = gc->palette[fg1].b;
	
	font_ptr = &gc->font[2048 + ((symbol & 0xff) << 3)];
	
	for(toggler = 0, row = 0; row < 8; ++row)
	{
		pixels = font_ptr[row];
		for(y = 0; y < scale; ++y, toggler ^= 1)
		{
			yy = row * scale + y + y0;
			
			if (yy >= 0 && yy < gc->height)
			{
				texture_index = yy * gc->gl_line_bytes;		
				pixel = pixels;
				for(column = 0; column < 8; ++column)
				{
					pr = (pixel & 0x80 ? fgr : bgr);
					pg = (pixel & 0x80 ? fgg : bgg);
					pb = (pixel & 0x80 ? fgb : bgb);
					tp = &gc->gl_texture[texture_index+(x0+column*scale)*4];

					for(x = 0; x < scale; ++x, toggler ^= 1)
					{
						xx = column * scale + x + x0;
						if ((xx >= 0) && (xx < gc->width))
						{
							tp[4*x+0] = pb[toggler];
							tp[4*x+1] = pg[toggler];
							tp[4*x+2] = pr[toggler];
							tp[4*x+3] = 255;
						}
					}			
					pixel = (pixel & 0x7f) << 1;
				}
			}
		}
	}
}

static int ascii_2_cbm(unsigned char ascii)
{
	if (ascii == 0x40)
	{
		return 0;
	}

	if (ascii >= 0x40 && ascii <= 0x5f)
	{
		return ascii;
	}

	if (ascii >= ' ' && ascii <= 0x3f)
	{
		return ascii;
	}

	if (ascii >= 0x60 && ascii <= 127)
	{
		return ascii - 0x60;
	}
	
	/* symbols above 128 are mapped to [0-127] so we can represent all non reversed chars
	 */
	
	return ascii & 0x7f;
}

static void print_string(struct graphics_t *gc, const unsigned char *str,
													int x0, int y0, int scale, int spacing,
													int reverse, int bg, int fg)
{
	int i, x, y;

	x = x0; y = y0;
	
	for(i = 0; str[i]; ++i)
	{
		print_symbol(gc, x, y, ascii_2_cbm(str[i]) + (reverse ? 128 : 0), scale, bg, fg);
		x += scale * (8 + spacing);
	}
}

static void print_string_line_col(struct graphics_t *gc, const unsigned char *str,
													int line, int col, int scale, int spacing,
													int reverse, int bg, int fg)
{
	print_string(gc, str, scale * (8 + spacing) * col, scale * (8 + spacing) * line,
								scale, spacing, reverse, bg, fg);
}


static void put_sprite(struct graphics_t *gc, unsigned char *sdata,
											int x0, int y0, int scale, int bg, int fg)
{
	int row, column, x, y, xx, yy;
	int pixels, pixel;
	unsigned char bgr, bgg, bgb;
	unsigned char fgr, fgg, fgb;
	int texture_index;
	unsigned char *tp, pr, pg, pb;
			
	bgr = gc->palette[bg].r; bgg = gc->palette[bg].g; bgb = gc->palette[bg].b;
	fgr = gc->palette[fg].r; fgg = gc->palette[fg].g; fgb = gc->palette[fg].b;
	
	for(row = 0; row < 21; ++row)
	{
		pixels = (sdata[3*row+0] << 16) | (sdata[3*row+1] << 8) | (sdata[3*row+2]);
		for(y = 0; y < scale; ++y)
		{
			yy = row * scale + y + y0;
			
			if (yy >= 0 && yy < gc->height)
			{
				texture_index = yy * gc->gl_line_bytes;		
				pixel = pixels;
				for(column = 0; column < 24; ++column)
				{
					switch(pixel & 0x800000)
					{
						case 0x000000:
							pr = bgr; pg = bgg; pb = bgb;
							break;
						default:
							pr = fgr; pg = fgg; pb = fgb;
							break;
					}

					tp = &gc->gl_texture[texture_index+(x0+column*scale)*4];

					for(x = 0; x < scale; ++x)
					{
						xx = column * scale + x + x0;
						if ((xx >= 0) && (xx < gc->width))
						{
							tp[4*x+0] = pb;
							tp[4*x+1] = pg;
							tp[4*x+2] = pr;
							tp[4*x+3] = 255;
						}
					}			
					pixel = (pixel & 0x7fffff) << 1;
				}
			}
		}
	}
}

static void put_mc_sprite(struct graphics_t *gc, unsigned char *sdata,
													int x0, int y0, int scale, int bg, int fg0, int fg1, int fg2)
{
	int row, column, x, y, xx, yy;
	int pixels, pixel;
	unsigned char bgr, bgg, bgb;
	unsigned char fg0r, fg0g, fg0b;
	unsigned char fg1r, fg1g, fg1b;
	unsigned char fg2r, fg2g, fg2b;
	int texture_index;
	unsigned char *tp, pr, pg, pb;
			
	bgr  = gc->palette[bg].r;  bgg  = gc->palette[bg].g;  bgb  = gc->palette[bg].b;
	fg0r = gc->palette[fg0].r; fg0g = gc->palette[fg0].g; fg0b = gc->palette[fg0].b;
	fg1r = gc->palette[fg1].r; fg1g = gc->palette[fg1].g; fg1b = gc->palette[fg1].b;
	fg2r = gc->palette[fg2].r; fg2g = gc->palette[fg2].g; fg2b = gc->palette[fg2].b;
	
	for(row = 0; row < 21; ++row)
	{
		pixels = (sdata[3*row+0] << 16) | (sdata[3*row+1] << 8) | (sdata[3*row+2]);
		for(y = 0; y < scale; ++y)
		{
			yy = row * scale + y + y0;
			
			if (yy >= 0 && yy < gc->height)
			{
				texture_index = yy * gc->gl_line_bytes;		
				pixel = pixels;
				for(column = 0; column < 12; ++column)
				{
					switch(pixel & 0xc00000)
					{
						case 0x000000:
							pr = bgr; pg = bgg; pb = bgb;
							break;
						case 0x400000:
							pr = fg0r; pg = fg0g; pb = fg0b;
							break;
						case 0x800000:
							pr = fg1r; pg = fg1g; pb = fg1b;
							break;
						default:
							pr = fg2r; pg = fg2g; pb = fg2b;
							break;
					}

					tp = &gc->gl_texture[texture_index+(x0+column*2*scale)*4];

					for(x = 0; x < 2*scale; ++x)
					{
						xx = column * scale + x + x0;
						if ((xx >= 0) && (xx < gc->width))
						{
							tp[4*x+0] = pb;
							tp[4*x+1] = pg;
							tp[4*x+2] = pr;
							tp[4*x+3] = 255;
						}
					}			
					pixel = (pixel & 0x3fffff) << 2;
				}
			}
		}
	}
}

int multi_color = 1;
int sprite_number = 0;
int snapshot_offset = 132;
int _bg = 0x00;
int _fg0 = 0x0f;
int _fg1 = 0x0c;
int _fg2 = 0x0b;


static void xyzzy(struct graphics_t *gc)
{
	char buf[32];
	int x, y;
	int bg, fg0, fg1, fg2;
	int addr;
	int sprite;
	
	bg = _bg;
	fg0 = _fg0;
	fg1 = _fg1;
	fg2 = _fg2;

	sprite_number &= 0x3ff;
	sprite = sprite_number;

	sprintf(buf, "# %03d", sprite_number);
	print_string_line_col(gc, (unsigned char *)buf, 0, 0, 4, 0, 0, 0x00, 0x65);
	sprintf(buf, "@ $%04x", sprite_number * 64);
	print_string_line_col(gc, (unsigned char *)buf, 0, 13, 4, 0, 0, 0x00, 0x27);

	sprintf(buf, "(o: %03d)", snapshot_offset);
	print_string_line_col(gc, (unsigned char *)buf, 1, 0, 4, 0, 0, 0x42, 0x7f);

	for(y = 0; y < 8; ++y)
	{
		sprintf(buf, "#%03d", sprite);
		print_string_line_col(gc, (unsigned char *)buf, 10+y*6, 0, 1, 0, 0, 0x00, 0xa8);
		for(x = 0; x < 8; ++x)
		{
			addr = snapshot_offset + 64 * (sprite++ & 0x3ff);
			if (addr < 0)
			{
				addr = 0;
			}
			if (addr > (C64SNAPSHOT_SIZE - 64))
			{
				addr = C64SNAPSHOT_SIZE - 64;
			}
			if (multi_color)
			{
				put_mc_sprite(gc, &gc->c64snapshot[addr],
				32 + 52 * x, 80 + y * 48, 2, bg, fg0, fg1, fg2);
			}
			else
			{
				put_sprite(gc, &gc->c64snapshot[addr],
				32 + 52 * x, 80 + y * 48, 2, bg, fg0);
			}
		}
	}
}

static int init_graphics(struct graphics_t *gc, const char *filename, int width, int height)
{
	gc->gl_texture = NULL;

	gc->width = width;
	gc->height = height;
	
	gc->gl_line_bytes = 4 * gc->width;
	gc->gl_texture_bytes = gc->gl_line_bytes * gc->height;

	gc->c64snapshot = init_c64snapshot(filename);
	if (!gc->c64snapshot)
	{
		fprintf(stderr, "snap-kaka\n");
		exit(-1);
	}
	
	gc->font = init_font("/usr/local/lib/vice/C64/chargen");
	if (!gc->font)
	{
		fprintf(stderr, "font-kaka\n");
		exit(-1);
	}


	init_palette(&gc->palette[0]);

	gc->gl_texture = calloc(gc->gl_texture_bytes, 1);
	
	return 0;
}

static void update_world(int ignored);

static void schedule_update()
{
	glutTimerFunc(100, update_world, 0);
}

static void update_world(int ignored)
{
	struct graphics_t *gc;

	gc = &graphics_global;

	memset(&gc->gl_texture[0], 0, gc->gl_texture_bytes);
	xyzzy(gc);

	schedule_update();
	glutPostRedisplay();
}

static void glut_keyboard(unsigned char key, int x, int y)
{
	switch(key)
	{
		case '1':
			_bg = (_bg + 1) & 0xf;
			break;
		case '!':
			_bg = (_bg - 1) & 0xf;
			break;
		case '2':
			_fg0 = (_fg0 + 1) & 0xf;
			break;
		case '"':
			_fg0 = (_fg0 - 1) & 0xf;
			break;
		case '3':
			_fg1 = (_fg1 + 1) & 0xf;
			break;
		case '#':
			_fg1 = (_fg1 - 1) & 0xf;
			break;
		case '4':
			_fg2 = (_fg2 + 1) & 0xf;
			break;
		case 164:
			_fg2 = (_fg2 - 1) & 0xf;
			break;

		case 'm':
			multi_color = 1 ^ multi_color;
			break;

		case ' ':
			sprite_number += 1;
			break;
	
		case 'w':
			sprite_number -= 32;
			break;
		case 's':
			sprite_number += 32;
			break;
		case 'a':
			snapshot_offset -= 1;
			break;
		case 'd':
			snapshot_offset += 1;
			break;
		case 13:
			sprite_number = (sprite_number + 0x040) & 0x3c0;	
			break;
		case 27:
			exit(0);
	}
}

static void glut_display() {    
	struct graphics_t *gc;
	gc = &graphics_global;

	glEnable (GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D (
		  GL_TEXTURE_2D,
		  0,
		  GL_RGBA8,
		  gc->width,
		  gc->height,
		  0,
		  GL_BGRA,
		  GL_UNSIGNED_BYTE,
		  &gc->gl_texture[0]
	);

	glBegin(GL_QUADS);
		  glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0,  1.0);
		  glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0, -1.0);
		  glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0, -1.0);
		  glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0,  1.0);
	glEnd();

	glFlush();
	glutSwapBuffers();
}

int main(int argc, char **argv)
{
	const char *filename = "snap";
	
	if (argc > 1)
		filename = argv[1];

	init_graphics(&graphics_global, filename, 640, 512);

  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

  glutInitWindowPosition(100, 100);
  glutInitWindowSize(640, 512);
  glutCreateWindow("XYZZY");


  glMatrixMode(GL_PROJECTION);


  glLoadIdentity();

  glOrtho(-1, 1, -1, 1, -1, 1);
    
  glMatrixMode(GL_MODELVIEW);

  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
	glutKeyboardFunc(glut_keyboard);
  glutDisplayFunc(glut_display);
	schedule_update();

  glutMainLoop();

  return 0;
}

