/* imgtl_png.h
 * PNG encoder and decoder.
 *
 * Decoder is mostly conformant; the big violation is interlacing -- we don't support it at all.
 * (Other violations are trivial, like we don't assert most chunk ordering rules).
 * We support any format whose total pixel size is a factor or multiple of 8, up to 64.
 * This includes a few combinations not enumerated by the spec, eg 16-bit RGBA.
 * We enforce a few sanity limits on image size, to prevent overflow.
 * These technically violate the standard, but you'll only see them in images of preposturous size.
 *
 * Encoder produces conformant files unless you specify an unusual pixel format, and the LOOSE_FORMAT flag.
 * Encoder does not validate your custom chunk IDs on the way out, so you can easily violate the standard that way.
 * We never produce interlaced files.
 *
 */

#ifndef IMGTL_PNG_H
#define IMGTL_PNG_H

struct imgtl_buffer;
struct imgtl_png; // See below.

/* Conveniences.
 *****************************************************************************/

/* Encode pixels to PNG without even needing a context object.
 * Rows must be padded to 1 byte.
 * (depth,colortype) are straight from the PNG standard.
 * (depth) is the size of one channel, in bits.
 * (colortype) is one of: 0=gray, 2=RGB, 3=index, 4=gray+alpha, 6=RGBA.
 */
int imgtl_png_encode_simple(struct imgtl_buffer *dst,const void *src,int w,int h,int depth,int colortype);
#define imgtl_png_encode_y8(dst,src,w,h) imgtl_png_encode_simple(dst,src,w,h,8,0)
#define imgtl_png_encode_rgb(dst,src,w,h) imgtl_png_encode_simple(dst,src,w,h,8,2)
#define imgtl_png_encode_rgba(dst,src,w,h) imgtl_png_encode_simple(dst,src,w,h,8,6)

/* Encode and decode, lowest level.
 *****************************************************************************/

/* Decoding must be done in a single pass (the whole file must be in memory).
 * You should not reuse the context, except maybe to re-encode the same image.
 */
int imgtl_png_decode(struct imgtl_png *png,const void *src,int srcc);

/* Serialized PNG is appended to (dst), should normally be empty when we begin.
 * Caller must set these fields before calling:
 *   pixels w h depth colortype
 * Optionally set plte, trns, and chunkv if you want them written.
 * Extra chunks go just before IDAT, in the order listed.
 * All other fields are initialized internally.
 */
int imgtl_png_encode(struct imgtl_buffer *dst,struct imgtl_png *png);

/* Full interface.
 *****************************************************************************/

// Flags for encode [e] and decode [d].
#define IMGTL_PNG_CHECK_CRC        0x00000001 // [d] Require valid CRCs for all chunks.
#define IMGTL_PNG_REQUIRE_IEND     0x00000002 // [d] Pedantically insist on proper termination.
#define IMGTL_PNG_VALIDATE_PLTE    0x00000004 // [d] Require PLTE for indexed images, and validate PLTE size for all formats.
#define IMGTL_PNG_CHECK_ANCILLARY  0x00000008 // [d] Check the ANCILLARY bit of unknown chunks, as mandated by standard.
#define IMGTL_PNG_KEEP_CHUNKS      0x00000010 // [d] Fill 'chunkv' with all unknown chunks (all but IHDR,IDAT,IEND,PLTE,tRNS).
#define IMGTL_PNG_LOOSE_FORMAT     0x00000020 // [e] Permit any pixel format we can decode, not just the PNG-legal ones.
#define IMGTL_PNG_UNCOMPRESSED     0x00000040 // [e] Ask zlib not to actually compress anything. Super fast, and super wasteful of disk.

// filter_strategy, for encoding.
// By default, you get MOST_ZEROES, which is a good choice.
// The first three require us to perform all five filters on every row, so there is a performance fee.
// For maximum performance and worst compression, use ALWAYS_NONE.
// ALWAYS_UP and ALWAYS_PAETH are pretty good compromises.
#define IMGTL_PNG_FILTER_MOST_ZEROES      0 // Recommended by spec. Use whichever filter produces the most zero bytes.
#define IMGTL_PNG_FILTER_LONGEST_RUN      1 // Use whichever produces the single longest run of any value.
#define IMGTL_PNG_FILTER_MOST_NEIGHBORS   2 // Use whichever produces the most neighbor pairs.
#define IMGTL_PNG_FILTER_ALWAYS_NONE      3 // Don't filter at all, just emit a zero byte before each row.
#define IMGTL_PNG_FILTER_ALWAYS_SUB       4 // Always use the SUB filter.
#define IMGTL_PNG_FILTER_ALWAYS_UP        5 // Always use the UP filter.
#define IMGTL_PNG_FILTER_ALWAYS_AVG       6 // Always use the AVG filter.
#define IMGTL_PNG_FILTER_ALWAYS_PAETH     7 // Always use the PAETH filter.

struct imgtl_png_chunk {
  char id[4];
  void *v;
  int c;
};

/* Encoder or decoder context.
 * Initialize a context to straight zeroes.
 * Clean up after any use of a context, even if the first call fails.
 * Generally, contexts are single-use.
 */
struct imgtl_png {

  void *pixels;
  int w,h;
  int depth,colortype;
  
  int stride;
  int pixelsize;
  int chanc;
  int xstride; // Bytes horizontally, minimum 1.

  void *plte;
  int pltec; // Bytes.
  void *trns;
  int trnsc; // Bytes.

  char *msg;
  int msgc;

  uint32_t flags;
  void *userdata;
  int (*decode_other)(struct imgtl_png *png,const char *chunkid,const void *src,int srcc);
  int filter_strategy;

  struct imgtl_png_chunk *chunkv;
  int chunkc,chunka;

  // Private, internal use only:
  void *z;
  uint8_t *rowbuf;
  int y;

};

void imgtl_png_cleanup(struct imgtl_png *png);
void imgtl_png_chunk_cleanup(struct imgtl_png_chunk *chunk);

int imgtl_png_set_msg(struct imgtl_png *png,const char *src,int srcc);
int imgtl_png_set_msgfv(struct imgtl_png *png,const char *fmt,va_list vargs);
int imgtl_png_set_msgf(struct imgtl_png *png,const char *fmt,...);

int imgtl_png_set_plte(struct imgtl_png *png,const void *src,int srcc);
int imgtl_png_set_trns(struct imgtl_png *png,const void *src,int srcc);
int imgtl_png_add_chunk(struct imgtl_png *png,const char *id,const void *src,int srcc);

// The spec has pretty strict requirements; we only insist that it be printable ASCII.
int imgtl_png_chunkid_valid(const char *src);

#endif
