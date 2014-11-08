/* imgtl_deck.h
 */

#ifndef IMGTL_DECK_H
#define IMGTL_DECK_H

void imgtl_deck_quit();
void imgtl_deck_unload();

/* Replace the current deck with a new spreadsheet.
 * (path) refers to a TSV file: Columns separated by HT, then rows separated by LF.
 * The first row are captions and any empty row is ignored. All other rows are one card each.
 */
int imgtl_deck_load(const char *path);

/* Open the PNG file for each card's background.
 * This does not replace the individual cards' images.
 */
int imgtl_deck_load_background_image(const char *path);

/* The field count here is the count of titled fields, which may include gaps between.
 */
int imgtl_deck_count_cards();
int imgtl_deck_count_fields();

/* Return key and value for an enumerated field.
 * Either or both may be empty.
 * Cards do not necessarily all have the same field count.
 * But the same field index in different cards is always the same field.
 * (From the spreadsheet, cardid is row and fieldid is column).
 * We guarantee that every card will report at least one field, to ease iteration.
 * If the value is a positive integer, we return it. Otherwise 0==success or -1==failure.
 */
int imgtl_deck_get_indexed_field(void *kpp,int *kc,void *vpp,int *vc,int cardid,int fieldid);

/* Return ID of field with the given name.
 * Empty names never match.
 */
int imgtl_deck_lookup_field(const char *name,int namec);

/* Copy the background image into all cards.
 * If you're going to run out of memory, this is probably where it will happen.
 * The image previously loaded by imgtl_deck_load_background_image() is committed at this time.
 * Any other rendering is overwritten.
 * If no background image is loaded, this unloads all card images.
 */
int imgtl_deck_begin_rendering();

// Set the fallback color for calls that try to pull it from card fields, when they fail.
int imgtl_deck_default_color(uint32_t rgba);

/* Render field content as text on each card.
 * Output is centered at (x,y), of color (rgba).
 * If ((rgba&0x00ffffff)==0x00fe0100), its high byte is the field ID containing actual color for each card.
 */
int imgtl_deck_add_label(int fieldid,int x,int y,int align,uint32_t rgba);
int imgtl_deck_add_rlabel(int fieldid,int x,int y,int align,int rotation,uint32_t rgba);

/* Break lines and everything.
 * If ((rgba&0x00ffffff)==0x00fe0100), its high byte is the field ID containing actual color for each card.
 */
int imgtl_deck_add_text(int fieldid,int x,int y,int w,int h,uint32_t rgba);

int imgtl_deck_add_formatted_label(const char *src,int srcc,int x,int y,uint32_t rgba,int align,int rotation);
int imgtl_deck_add_formatted_text(const char *src,int srcc,int x,int y,int w,int h,uint32_t rgba,int rotation);

/* (path) may contain a field name enclosed in curly braces.
 * If so, there can only be one field insertion, and images are considered optional.
 * If the image exists, it is drawn centered and clipped in the stated rectangle.
 */
int imgtl_deck_add_image(const char *path,int pathc,int x,int y,int w,int h,int rotation);

/* Create the directory named by (path), and into it, save an image file for each card.
 */
int imgtl_deck_save_images(const char *path);

#endif
