/* imgtl_script.h
 * This is the highest level structure in imgtl.
 * It coordinates access to the deck, essentially exporting the API in <imgtl_deck.h>.
 *
 * Scripts generally are line-oriented text.
 * LF and CR both break lines, and any byte 0x20 or lower is space.
 * Hash begins a line comment.
 * Tokens are separated by whitespace; first token is the command.
 * A quoted string is also a discrete token, even with whitespace inside.
 *
 * COMMANDS
 *
 *   save_hint HINT
 *     imgtl_image_save_hint
 *     One of: "small" "medium" "fast"
 *
 *   font PATH SIZE
 *     imgtl_text_setup()
 *     Load a TTF font file.
 *     SIZE in the approximate height in pixels of a glyph with ascender and descender.
 *
 *   load PATH
 *     imgtl_deck_load()
 *     Load TSV spreadsheet where each row is a card and each column a field.
 *     Top row are field names.
 *     All global data is wiped.
 *
 *   background PATH
 *     imgtl_deck_load_background_image(), imgtl_deck_begin_rendering()
 *     Load PNG file for each card's background (front face).
 *     This must come before any rendering commands.
 *
 *   blank W H
 *     imgtl_deck_set_blank_background_image(),imgtl_deck_begin_rendering()
 *     Similar to 'background', but generate a blank image of the requested size.
 *
 *   defaultcolor COLOR
 *     imgtl_deck_default_color()
 *     Set default color for 'label' and 'text', if they are pulling the color from a field.
 *
 *   label FIELD X Y ALIGN COLOR
 *     imgtl_deck_add_label()
 *     Put text on each card, drawing content from the named field.
 *     Y is the vertical baseline of text.
 *     X is the left, middle, or right, depending on ALIGN.
 *     COLOR is hexadecimal "RRGGBB" or "RRGGBBAA", or one of a few common names ("red", "white", etc).
 *     COLOR may also be a field name, in which case its content contains the actual color.
 *
 *   labelup FIELD X Y ALIGN COLOR
 *   labeldown FIELD X Y ALIGN COLOR
 *     Draw text as with 'label', but rotated 90 degrees clockwise ("down") or counterclockwise ("up").
 *     (X,Y) is the starting point, and it moves vertically from there.
 *
 *   text FIELD X Y W H COLOR
 *     imgtl_deck_add_text()
 *     Add multiline text to each card, in the given box.
 *     A sensible margin is automatically applied.
 *     At this time, the box's bottom edge is not respected.
 *
 *   labelfmt STRING X Y COLOR [ALIGN [ROTATION]]
 *   textfmt STRING X Y W H COLOR [ROTATION]
 *     imgtl_deck_add_formatted_label()
 *     imgtl_deck_add_formatted_text()
 *     Same as (label,labelup,labeldown,text), but argument is plain text instead of a field name.
 *     Any curly braces in text, we replace with the content of the named field.
 *
 *   image PATH X Y W H [ROTATION]
 *     imgtl_deck_add_image()
 *     Load a PNG file and draw it on to every card.
 *     If PATH contains a field name in curly braces, the content of that field is substituted per card.
 *     Image is centered and clipped to provided boundary.
 *     Use (0,0) for boundary size to disable clipping.
 *     ROTATION is one of (0,90,180,270), 0 if unset.
 *
 *   rect X Y W H COLOR
 *     imgtl_deck_draw_rect()
 *     Draw a solid rectangle on every card.
 *
 *   line AX AY BX BY COLOR
 *     imgtl_deck_draw_line()
 *     Draw a single-pixel line on every card.
 *
 *   save [PATH]
 *     imgtl_deck_save_images()
 *     Save all cards as PNG files under the named directory.
 *
 */

#ifndef IMGTL_SCRIPT_H
#define IMGTL_SCRIPT_H

/* Read script from a text file and run it.
 */
int imgtl_script_execute_line(const char *src,int srcc,const char *refname,int lineno);
int imgtl_script_execute_text(const char *src,int srcc,const char *refname);
int imgtl_script_execute(const char *path);

#endif
