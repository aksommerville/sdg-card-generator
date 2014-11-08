/* imgtl_draw.h
 * High-level rendering to image objects.
 */

#ifndef IMGTL_DRAW_H
#define IMGTL_DRAW_H

struct imgtl_image;

#define IMGTL_XFORM_NONE         0
#define IMGTL_XFORM_CLOCK        1
#define IMGTL_XFORM_180          2
#define IMGTL_XFORM_COUNTER      3
#define IMGTL_XFORM_FLOP         4
#define IMGTL_XFORM_FLOPCLOCK    5
#define IMGTL_XFORM_FLIP         6
#define IMGTL_XFORM_FLOPCOUNTER  7

// Solid rectangle. Alpha is not used, it is transferred like a color.
int imgtl_draw_rect(struct imgtl_image *image,int x,int y,int w,int h,uint32_t rgba);

// Single-pixel line. Alpha is not used, it is transferred like a color.
int imgtl_draw_line(struct imgtl_image *image,int ax,int ay,int bx,int by,uint32_t rgba);

/* Blit from one image to another.
 * Source alpha is used if present.
 */
int imgtl_draw_image(struct imgtl_image *dst,int dstx,int dsty,const struct imgtl_image *src,int srcx,int srcy,int w,int h);

/* Produce a new image by performing the given transform on the entire source image.
 */
struct imgtl_image *imgtl_xform(struct imgtl_image *src,int xform);

/* Copy alpha image with single color onto an image, eg for text.
 */
int imgtl_draw_a8(struct imgtl_image *image,int x,int y,const void *alpha,int srcstride,int w,int h,uint32_t rgba);

/* Text uses some globals to talk to FreeType.
 * They'll initialize implicitly, but you still should clean up manually.
 */
int imgtl_text_init();
void imgtl_text_quit();

/* Replace font.
 * (path) should refer to a TTF file.
 * (size) is a hint for the glyphs' height in pixels.
 */
int imgtl_text_setup(const char *path,int size);

/* Draw text with the current font into this image.
 * (x) is the left, middle, or right edge, depnding on (align).
 * (y) is the baseline, typically just below the bottom of most glyphs.
 * Text is UTF-8.
 */
int imgtl_draw_text(struct imgtl_image *image,int x,int y,int align,const char *src,int srcc,uint32_t rgba);

/* Return a WEAK reference to the glyph for this codepoint, and some metrics for it.
 *  dx,dy: Top left corner of glyph output, relative to baseline.
 *  advance: Horizontal movement along baseline after rendering.
 * Glyphs are cached internally, so after getting it the first time, it's cheap to get again.
 */
struct imgtl_image *imgtl_text_get_glyph(int *dx,int *dy,int *advance,int ch);

/* Transform the glyph image and also transform its metrics.
 * Returns a STRONG reference, caller must free it.
 * Transformed glyphs are not cached anywhere.
 */
struct imgtl_image *imgtl_text_get_transformed_glyph(int *dx,int *dy,int *advancex,int *advancey,int ch,int xform);

/* Measure a row of text with the current font.
 * Returns total height, ie (ascent+descent).
 * There are no line breaks or anything; it's always one row.
 */
int imgtl_measure_text(int *w,int *ascent,int *descent,const char *src,int srcc);

/* Render text into the given box.
 * Lines broken automatically will expand to fill the availble width.
 * Leading space is stripped from every line. (TODO Will we ever want indentation?)
 */
int imgtl_draw_multiline_text(struct imgtl_image *image,int x,int y,int w,int h,const char *src,int srcc,uint32_t rgba);

int imgtl_draw_rotated_text(struct imgtl_image *image,int x,int y,int align,const char *src,int srcc,uint32_t rgba,int rotation);

#endif
