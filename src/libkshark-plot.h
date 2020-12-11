/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2018 VMware Inc, Yordan Karadzhov <y.karadz@gmail.com>
 */

 /**
  *  @file    libkshark-plot.h
  *  @brief   Basic tools for OpenGL plotting.
  */

#ifndef _LIB_KSHARK_PLOT_H
#define _LIB_KSHARK_PLOT_H

// C
#include <stdbool.h>

// OpenGL
#include <GL/gl.h>
#include <GL/glu.h>

/*
 * STB TrueType (single-file public domain library)
 * https://github.com/nothings/stb
 */
#include "stb_truetype.h"

#ifdef __cplusplus
extern "C" {
#endif

// KernelShark
#include "KsCmakeDef.hpp"

/** Structure defining a RGB color. */
struct ksplot_color {
	/** The Red component of the color. */
	uint8_t red;

	/** The Green component of the color. */
	uint8_t green;

	/** The Blue component of the color. */
	uint8_t blue;
};

/** Structure defining a 2D point. */
struct ksplot_point {
	/** The horizontal coordinate of the point in pixels. */
	int x;

	/** The vertical coordinate of the pointin in pixels. */
	int y;
};

#ifdef GLUT_FOUND

void ksplot_make_scene(int width, int height);

#endif // GLUT_FOUND

void ksplot_init_opengl(int dpr);

void ksplot_resize_opengl(int width, int height);

void ksplot_draw_point(const struct ksplot_point *p,
		       const struct ksplot_color *col,
		       float size);

void ksplot_draw_line(const struct ksplot_point *a,
		      const struct ksplot_point *b,
		      const struct ksplot_color *col,
		      float size);

void ksplot_draw_polyline(const struct ksplot_point *points,
			  size_t n_points,
			  const struct ksplot_color *col,
			  float size);

void ksplot_draw_polygon(const struct ksplot_point *points,
			 size_t n_points,
			 const struct ksplot_color *col,
			 float size);

void ksplot_draw_polygon_contour(const struct ksplot_point *points,
				 size_t n_points,
				 const struct ksplot_color *col,
				 float size);

/** The index of the "Space" character. */
#define KS_SPACE_CHAR 32

/** The index of the "Tilda" character. */
#define KS_TILDA_CHAR 126

/** Total number of characters supported for drawing. */
#define KS_N_CHAR (KS_TILDA_CHAR - KS_SPACE_CHAR + 1)

/** Structure defining a font. */
struct ksplot_font {
	/** Identifier of the font's texture. */
	GLuint texture_id;

	/** Font's texture baking data. */
	stbtt_bakedchar cdata[KS_N_CHAR];

	/** The height of a text line. */
	int height;

	/** The vertical position of the font's baseline. */
	int base;

	/** The size of the font. */
	int size;

	/**
	 * The width of the 'z' character. To be use as an average character
	 * width.
	 */
	int char_width;
};

/** Check if the texture of the font is loaded. */
static inline bool ksplot_font_is_loaded(struct ksplot_font *f)
{
	return f->texture_id > 1;
}

char *ksplot_find_font_file(const char *font_family, const char *font_name);

bool ksplot_init_font(struct ksplot_font *font, float size, const char *file);

void ksplot_print_text(const struct ksplot_font *font,
		       const struct ksplot_color *col,
		       float x, float y,
		       const char *text);

#ifdef __cplusplus
}
#endif

#endif
