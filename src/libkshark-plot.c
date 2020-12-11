/* SPDX-License-Identifier: LGPL-2.1 */

/*
 * Copyright (C) 2018 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
 */

/**
 *  @file    libkshark-plot.c
 *  @brief   Basic tools for OpenGL plotting.
 */

// C
#ifndef _GNU_SOURCE
/** Use GNU C Library. */
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

// KernelShark
#include "libkshark-plot.h"

/*
 * STB TrueType (single-file public domain library)
 * https://github.com/nothings/stb
 */
/** Generate implementation. */
#define STB_TRUETYPE_IMPLEMENTATION

/** Make the implementation private. */
#define STBTT_STATIC
#include "stb_truetype.h"

#ifdef GLUT_FOUND

#include <GL/freeglut.h>

/**
 * @brief Create an empty scene for drawing.
 *
 * @param width: Width of the screen window in pixels.
 * @param height: Height of the screen window in pixels.
 */
void ksplot_make_scene(int width, int height)
{
	/* Set Display mode. */
	glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

	/* Prevent the program from exiting when a window is closed. */
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,
		      GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	/* Set window size. */
	glutInitWindowSize(width, height);

	/* Set window position on screen. */
	glutInitWindowPosition(50, 50);

	/* Open the screen window. */
	glutCreateWindow("KernelShark Plot");

	ksplot_resize_opengl(width, height);
}

#endif // GLUT_FOUND

/**
 * @brief Initialize OpenGL.
 *
 * @param dpr: Device Pixel Ratio.
 */
void ksplot_init_opengl(int dpr)
{
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_COLOR_MATERIAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_POLYGON_SMOOTH);
	glLineWidth(1.5 * dpr);
	glPointSize(2.5 * dpr);
	glClearColor(1, 1, 1, 1); // White
}

/**
 * @brief To be called whenever the OpenGL window has been resized.
 *
 * @param width: Width of the screen window in pixels.
 * @param height: Height of the screen window in pixels.
 */
void ksplot_resize_opengl(int width, int height)
{
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	/*
	 * Set the origin of the coordinate system to be the top left corner.
	 * The "Y" coordinate is inverted.
	 */
	gluOrtho2D(0, width, height, 0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

/**
 * @brief Draw a point.
 *
 * @param p: Input location for the point object.
 * @param col: The color of the point.
 * @param size: The size of the point.
 */
void ksplot_draw_point(const struct ksplot_point *p,
		       const struct ksplot_color *col,
		       float size)
{
	if (!p || !col || size < .5f)
		return;

	glPointSize(size);
	glBegin(GL_POINTS);
	glColor3ub(col->red, col->green, col->blue);
	glVertex2i(p->x, p->y);
	glEnd();
}

/**
 * @brief Draw a line.
 *
 * @param a: Input location for the first finishing point of the line.
 * @param b: Input location for the second finishing point of the line.
 * @param col: The color of the line.
 * @param size: The size of the line.
 */
void ksplot_draw_line(const struct ksplot_point *a,
		      const struct ksplot_point *b,
		      const struct ksplot_color *col,
		      float size)
{
	if (!a || !b || !col || size < .5f)
		return;

	glLineWidth(size);
	glBegin(GL_LINES);
	glColor3ub(col->red, col->green, col->blue);
	glVertex2i(a->x, a->y);
	glVertex2i(b->x, b->y);
	glEnd();
}

/**
 * @brief Draw the a polyline.
 *
 * @param points: Input location for the array of points defining the polyline.
 * @param n_points: The size of the array of points.
 * @param col: The color of the polyline.
 * @param size: The size of the polyline.
 */
void ksplot_draw_polyline(const struct ksplot_point *points,
			  size_t n_points,
			  const struct ksplot_color *col,
			  float size)
{
	if (!points || !n_points || !col || size < .5f)
		return;

	/* Loop over the points of the polygon and draw connecting lines. */
	for(size_t i = 1; i < n_points; ++i)
		ksplot_draw_line(&points[i - 1],
				 &points[i],
				 col,
				 size);
}

/**
 * @brief Draw a polygon.
 *
 * @param points: Input location for the array of points defining the polygon.
 * @param n_points: The size of the array of points.
 * @param col: The color of the polygon.
 * @param size: The size of the polygon.
 */
void ksplot_draw_polygon(const struct ksplot_point *points,
			 size_t n_points,
			 const struct ksplot_color *col,
			 float size)
{
	if (!points || !n_points || !col || size < .5f)
		return;

	if (n_points == 1) {
		ksplot_draw_point(points, col, size);
		return;
	}

	if (n_points == 2) {
		ksplot_draw_line(points, points + 1, col, size);
		return;
	}

	/* Draw a Triangle Fan. */
	glBegin(GL_TRIANGLE_FAN);
	glColor3ub(col->red, col->green, col->blue);
	for (size_t i = 0; i < n_points; ++i)
		glVertex2i(points[i].x, points[i].y);

	glVertex2i(points[0].x, points[0].y);
	glEnd();
}

/**
 * @brief Draw the contour of a polygon.
 *
 * @param points: Input location for the array of points defining the polygon.
 * @param n_points: The size of the array of points.
 * @param col: The color of the polygon.
 * @param size: The size of the polygon.
 */
void ksplot_draw_polygon_contour(const struct ksplot_point *points,
				 size_t n_points,
				 const struct ksplot_color *col,
				 float size)
{
	if (!points || !n_points || !col || size < .5f)
		return;

	/* Loop over the points of the polygon and draw a polyline. */
	ksplot_draw_polyline(points, n_points, col, size);

	/* Close the contour. */
	ksplot_draw_line(&points[0],
			 &points[n_points - 1],
			 col,
			 size);
}

/**
 * @brief Find a TrueType font file.
 *
 * @param font_family: The family name of the font.
 * @param font_name: The name of the font file without the extention.
 *
 * @returns A string containing the absolute path to the TrueType font file
 *	    on success, or NULL on failure. The user is responsible for freeing
 *	    the string.
 */
char *ksplot_find_font_file(const char *font_family, const char *font_name)
{
	char buffer[1024], *end;
	char *cmd = NULL;
	int ret;
	FILE *f;

	/*
	 * This is sorta a hack.
	 * FIXME: Do this a bit more properly.
	 */
	ret = asprintf(&cmd, "fc-list \'%s\' |grep %s.ttf", font_family,
							    font_name);
	if (ret <= 0)
		goto fail;

	f = popen(cmd, "r");
	free(cmd);
	if (f == NULL)
		goto fail;

	end = fgets(buffer, sizeof(buffer), f);
	pclose(f);
	if (!end)
		goto fail;

	end = strchr(buffer, ':');
	if (!end)
		goto fail;

	return strndup(buffer, end - buffer);

 fail:
	fprintf(stderr, "Failed to find font file.\n" );
	return NULL;
}

/** The size of the bitmap matrix used to load the font. */
#define KS_FONT_BITMAP_SIZE 1024

/**
 * @brief Initialize a font.
 *
 * @param font: Output location for the font descriptor.
 * @param size: The size of the font.
 * @param file: Input location for the truetype font file.
 */
bool ksplot_init_font(struct ksplot_font *font, float size, const char *file)
{
	unsigned char bitmap[KS_FONT_BITMAP_SIZE * KS_FONT_BITMAP_SIZE];
	int ascent, descent, line_gap, lsb;
	ssize_t buff_size, ret;
	unsigned char *buffer;
	stbtt_fontinfo info;
	FILE *font_file;
	struct stat st;
	float scale;

	ret = stat(file, &st);
	if (ret < 0) {
		fprintf(stderr, "Font file %s not found.\n", file);
		return NULL;
	}

	font_file = fopen(file, "rb");
	if (!font_file) {
		fprintf(stderr, "Failed to open font file!\n");
		return false;
	}

	/* Get the size of the file. */
	buff_size = st.st_size;

	buffer = malloc(buff_size);
	if (!buffer) {
		fprintf(stderr, "Failed to allocat memory for font!\n");
		goto close_file;
	}

	ret = fread(buffer, buff_size, 1, font_file);
	fclose(font_file);

	if (ret == 0) {
		fprintf(stderr, "Failed to read from font file!\n");
		goto free_buffer;
	}

	if (!stbtt_InitFont(&info, buffer, 0)) {
		fprintf(stderr, "Failed to init font!\n");
		goto free_buffer;
	}

	/* Get the font's metrics. */
	scale = stbtt_ScaleForMappingEmToPixels(&info, size);
	stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
	if (line_gap == 0)
		line_gap = - descent;
	/*
	 * Calculate the dimensions of the font. Note that the descent has
	 * a negative value.
	 */
	font->height = (- descent + ascent + line_gap) * scale;
	font->base = (- descent + line_gap / 2) * scale;
	font->size = size;

	/*
	 * The width of the 'z' character will be considered as an average
	 * character width.
	 */
	stbtt_GetCodepointHMetrics(&info, 'z', &font->char_width, &lsb);
	font->char_width *= scale;

	ret = stbtt_BakeFontBitmap(buffer,
				   0,
				   size,
				   bitmap,
				   KS_FONT_BITMAP_SIZE,
				   KS_FONT_BITMAP_SIZE,
				   KS_SPACE_CHAR,
				   KS_N_CHAR,
				   font->cdata);

	if (ret <= 0) {
		fprintf(stderr, "Failed to bake font bitmap!\n");
		goto free_buffer;
	}

	free(buffer);

	glGenTextures(1, &font->texture_id);
	glBindTexture(GL_TEXTURE_2D, font->texture_id);

	glTexImage2D(GL_TEXTURE_2D,
		     0,
		     GL_ALPHA,
		     KS_FONT_BITMAP_SIZE,
		     KS_FONT_BITMAP_SIZE,
		     0,
		     GL_ALPHA,
		     GL_UNSIGNED_BYTE,
		     bitmap);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return true;

 close_file:
	fclose(font_file);

 free_buffer:
	free(buffer);
	return false;
}

/**
 * @brief Print(draw) a text.
 *
 * @param font: Intput location for the font descriptor.
 * @param col: The color of the polygon.
 * @param x: Horizontal coordinate of the beginning of the text.
 * @param y: Verticalal coordinate of the beginning of the text.
 * @param text: The text to be drawn.
 */
void ksplot_print_text(const struct ksplot_font *font,
		       const struct ksplot_color *col,
		       float x, float y,
		       const char *text)
{
	glEnable(GL_TEXTURE_2D);

	/* Set the color of the text. */
	if (col)
		glColor3ub(col->red, col->green, col->blue);
	else
		glColor3ub(0, 0, 0); // Black

	glBindTexture(GL_TEXTURE_2D, font->texture_id);
	glBegin(GL_QUADS);
	for (; *text; ++text) {
		if (*text < KS_SPACE_CHAR && *text > KS_TILDA_CHAR)
			continue;

		stbtt_aligned_quad quad;

		/* "x" is incremented here to a new position. */
		stbtt_GetBakedQuad(font->cdata,
				   KS_FONT_BITMAP_SIZE,
				   KS_FONT_BITMAP_SIZE,
				   *text - KS_SPACE_CHAR,
				   &x, &y,
				   &quad,
				   1);

		glTexCoord2f(quad.s0, quad.t1);
		glVertex2f(quad.x0, quad.y1);

		glTexCoord2f(quad.s1, quad.t1);
		glVertex2f(quad.x1, quad.y1);

		glTexCoord2f(quad.s1, quad.t0);
		glVertex2f(quad.x1, quad.y0);

		glTexCoord2f(quad.s0, quad.t0);
		glVertex2f(quad.x0, quad.y0);
	}

	glEnd();
	glDisable(GL_TEXTURE_2D);
}
