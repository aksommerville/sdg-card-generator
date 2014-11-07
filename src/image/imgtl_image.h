/* imgtl_image.h
 *
 * General-purpose in-memory image.
 * Once created, and image can not be resized or reformatted.
 * They may or may not be mutable.
 * Images can point into a larger image.
 * All images are stored LRTB, no fancy tricks like separate planes or compression.
 * Pixels are 1..8 bytes. No sub-byte pixels.
 * Every legal pixel format is enumerated below.
 *
 * It's easy to image images that can't be described this way, eg:
 *  - Pixels smaller than one byte.
 *  - Color tables.
 *  - Storage other than LRTB.
 * If we have to deal with such a thing, convert it to a usable format first.
 *
 * There's a division of responsibility between 'image' and 'draw' which is not 100% clear.
 * For the most part, storage concerns are 'image', and rendering is 'draw', but there's overlap.
 */

#ifndef IMGTL_IMAGE_H
#define IMGTL_IMAGE_H

/* Image formats.
 *****************************************************************************/

// Format colorspace, ie which channels are present?
#define IMGTL_CS_UNSPEC  0
#define IMGTL_CS_RGBA    1
#define IMGTL_CS_RGB     2
#define IMGTL_CS_YA      3
#define IMGTL_CS_Y       4
#define IMGTL_CS_A       5

#define IMGTL_FMT_1          0 // 1-byte pixels, content undefined.
#define IMGTL_FMT_2          1 // 2 "
#define IMGTL_FMT_3          2 // 3 "
#define IMGTL_FMT_4          3 // 4 "
#define IMGTL_FMT_5          4 // 5 "
#define IMGTL_FMT_6          5 // 6 "
#define IMGTL_FMT_7          6 // 7 "
#define IMGTL_FMT_8          7 // 8 "
#define IMGTL_FMT_RGB        8 // 24-bit RGB.
#define IMGTL_FMT_RGBA       9 // 32-bit RGBA.
#define IMGTL_FMT_Y8        10 // 8-bit gray.
#define IMGTL_FMT_A8        11 // 8-bit alpha.

int imgtl_fmt_size(int fmt); // => pixel size in bytes (not bits!)
int imgtl_fmt_stride(int fmt,int w); // => minimum row size in bytes
int imgtl_fmt_colorspace(int fmt); // => see constants above

const char *imgtl_fmt_repr(int fmt);
int imgtl_fmt_eval(const char *src,int srcc);

typedef void (*imgtl_rgba_from_pixel_fn)(uint8_t *rgba,const uint8_t *pixel);
typedef void (*imgtl_pixel_from_rgba_fn)(uint8_t *pixel,const uint8_t *rgba);
imgtl_rgba_from_pixel_fn imgtl_fmt_get_reader(int fmt);
imgtl_pixel_from_rgba_fn imgtl_fmt_get_writer(int fmt);

// If nonzero, the function in question is unnecessary -- first four bytes of pixel can be read as RGBA.
int imgtl_pixel_reader_is_verbatim(imgtl_rgba_from_pixel_fn fn);
int imgtl_pixel_writer_is_verbatim(imgtl_pixel_from_rgba_fn fn);

int imgtl_raw_path_eval(int *w,int *h,int *fmt,const char *path,int pathc);

/* Image object.
 *
 * Everything in the image struct is read-only for your purposes.
 * We provide a setter for (ctab).
 * You can write to (*pixels) only if (write_pixels) is nonzero.
 *
 * We strictly prohibit retention loops.
 * The two object references in each image, ctab and keepalive, must not indirectly refer back to this image.
 * imgtl_image_set_ctab() asserts this rule.
 *****************************************************************************/

struct imgtl_image {

  int w,h; // Size in pixels.
  int stride; // Row size in bytes.
  int fmt; // See above.
  int pixelsize; // Derived from (fmt), size of one pixel in bytes.
  void *pixels; // Raw pixel data.
  
  int refc; // Reference count.
  struct imgtl_image *keepalive; // Source image.
  int free_pixels; // Nonzero if (pixels) must be freed at cleanup.
  int write_pixels; // Nonzero if (pixels) may be written to.
  
};

void imgtl_image_del(struct imgtl_image *image);
int imgtl_image_ref(struct imgtl_image *image);

/* Basic constructors.
 *   alloc: Mutable image, initially zeroed.
 *   handoff: Mutable image from the pixels you provide. We will free pixels eventually.
 *   borrow: Use provided pixels but do not free them. Mutability optional.
 */
struct imgtl_image *imgtl_image_new_alloc(int w,int h,int fmt);
struct imgtl_image *imgtl_image_new_handoff(void *pixels,int w,int h,int fmt,int stride);
struct imgtl_image *imgtl_image_new_borrow(const void *pixels,int w,int h,int fmt,int stride,int writeable);

// Nonzero if (src) refers to (query), even indirectly. TRUE if they are the same object.
int imgtl_image_refers(const struct imgtl_image *src,const struct imgtl_image *query);

/* Produce a new image pointing into (src) at the given rectangle.
 * Changes to one are immediately visible in the other.
 * Boundaries must be nonzero and within (src), otherwise we refuse.
 * Child inherits parent's mutability. You may safely set the new (write_pixels) to zero if you want.
 */
struct imgtl_image *imgtl_image_sub(struct imgtl_image *src,int x,int y,int w,int h);

/* Copy pixels from one image to another.
 * We quietly clamp to both images' boundaries.
 * If the two have different formats, we convert.
 * Source alpha is treated like a regular color, this is not a 'blit'.
 */
int imgtl_copy(struct imgtl_image *dst,int dstx,int dsty,const struct imgtl_image *src,int srcx,int srcy,int w,int h);

/* Copy a portion of any image into a new image.
 * Caller must eventually delete the returned image.
 * Any of (dstfmt,w,h) may be <0 to fill in automatically.
 * Boundaries may exceed source, in which case the excess is present and zeroed.
 * But the boundary must intersect source for at least one pixel.
 * This is the basic way to resize or reformat an image.
 */
struct imgtl_image *imgtl_image_copy(int dstfmt,const struct imgtl_image *src,int x,int y,int w,int h);

/* Rewrite an entire image with source pixels smaller than one byte.
 * Pixel values are read from (src), optionaly normalized to 0..255, then written to the first byte of each pixel in (image).
 * If (image) pixels are larger than one byte, the extra bytes are not touched.
 * Caller must ensure that these images are exactly the same size!
 * Bits are packed big-endianly, as in PNG. (0x80 is first, 0x01 is last).
 */
int imgtl_image_read_1bit(struct imgtl_image *image,const void *src,int normalize);
int imgtl_image_read_2bit(struct imgtl_image *image,const void *src,int normalize);
int imgtl_image_read_4bit(struct imgtl_image *image,const void *src,int normalize);

/* Pixels are 1..8 bytes.
 * RGBA is 0xff000000==R, etc.
 * If you allocate 8 bytes for a native pixel, it's always safe.
 * Drawing ops take these pixels instead of colors.
 */
int imgtl_pixel_from_rgba(uint8_t *pixel,const struct imgtl_image *image,uint32_t rgba);
int imgtl_rgba_from_pixel(uint32_t *rgba,const struct imgtl_image *image,const uint8_t *pixel);

/* Encode and decode.
 */

#define IMGTL_IMAGE_SAVE_HINT_DEFAULT       0 // Tend to favor small files over fast processing.
#define IMGTL_IMAGE_SAVE_HINT_FASTER        1 // Cut corners for speed that might make files larger. (eg no PNG filter optimization)
#define IMGTL_IMAGE_SAVE_HINT_MUCH_FASTER   2 // Take extraordinary steps to improve speed, like skipping compression entirely.
extern int imgtl_image_save_hint;

struct imgtl_image *imgtl_image_load_file(const char *path);
int imgtl_image_save_file(const char *path,const struct imgtl_image *image);

#endif
