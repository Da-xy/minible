/* 
 * This file is part of the Mooltipass Project (https://github.com/mooltipass).
 * Copyright (c) 2019 Stephan Mathieu
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
/*!  \file     sh1122.h
*    \brief    SH1122 OLED driver
*    Created:  30/12/2017
*    Author:   Mathieu Stephan
*/


#ifndef SH1122_H_
#define SH1122_H_

#include <asf.h>
#include "platform_defines.h"
#include "custom_bitstream.h"
#include "custom_fs.h"
#include "defines.h"


/* Command defines */
#define SH1122_CMD_SET_DISPLAY_OFF                      0xAE
#define SH1122_CMD_SET_ROW_ADDR                         0xB0
#define SH1122_CMD_SET_HIGH_COLUMN_ADDR                 0x10
#define SH1122_CMD_SET_LOW_COLUMN_ADDR                  0x00
#define SH1122_CMD_SET_CLOCK_DIVIDER                    0xD5
#define SS1122_CMD_SET_DISCHARGE_PRECHARGE_PERIOD       0xD9
#define SH1122_CMD_SET_DISPLAY_START_LINE               0x40
#define SH1122_CMD_SET_CONTRAST_CURRENT                 0x81
#define SH1122_CMD_SET_SEGMENT_REMAP                    0xA0
#define SH1122_CMD_SET_SCAN_DIRECTION                   0xC0
#define SH1122_CMD_SET_DISPLAY_OFF_ON                   0xA4
#define SH1122_CMD_SET_NORMAL_DISPLAY                   0xA6
#define SH1122_CMD_SET_REVERSE_DISPLAY                  0xA7
#define SH1122_CMD_SET_MULTIPLEX_RATIO                  0xA8
#define SH1122_CMD_SET_DCDC_SETTING                     0xAD
#define SH1122_CMD_SET_DISPLAY_OFFSET                   0xD3
#define SH1122_CMD_SET_VCOM_DESELECT_LEVEL              0xDB
#define SH1122_CMD_SET_VSEGM_LEVEL                      0xDC
#define SH1122_CMD_SET_DISCHARGE_VSL_LEVEL              0x30
#define SH1122_CMD_SET_DISPLAY_ON                       0xAF

/* Screen defines */
#define SH1122_OLED_Shift           0x1C
#define SH1122_OLED_Max_Column      0x7F     // 256/2-1
#define SH1122_OLED_Max_Row         0x3F     // 64-1
#define SH1122_OLED_Brightness      0x05     // Up to 0x0F
#define SH1122_OLED_Contrast        0xFF     // Up to 0xFF
#define SH1122_OLED_WIDTH           256
#define SH1122_OLED_HEIGHT          64
#define SH1122_OLED_BPP             4

/* Transition defines */
#define SH1122_TRANSITION_PIXEL     0x03

/* Enums */
typedef enum {OLED_TRANS_NONE, OLED_LEFT_RIGHT_TRANS, OLED_RIGHT_LEFT_TRANS, OLED_TOP_BOT_TRANS, OLED_BOT_TOP_TRANS, OLED_IN_OUT_TRANS, OLED_OUT_IN_TRANS} oled_transition_te;
typedef enum {OLED_SCROLL_NONE = 0, OLED_SCROLL_UP = 1, OLED_SCROLL_DOWN = 2, OLED_SCROLL_FLIP = 3} oled_scroll_te;
typedef enum {OLED_ALIGN_LEFT = 0, OLED_ALIGN_RIGHT = 1, OLED_ALIGN_CENTER = 2} oled_align_te;

/* Structs */
// pixel buffer to allow merging of adjacent image data.
// To conserve memory, only one GDDRAM word is kept per display line.
// The library assumes the display will be filled left to right, and
// hence it only needs to merge the rightmost data with the next write
// on that line.
// Note that the buffer is twice the OLED height, supporting
// the second hidden screen buffer.
/* Solution compiled with -Wpacked */
typedef struct
{
    int16_t xaddr;
    uint8_t pixels;
} gddram_px_t;

typedef struct
{
    Sercom* sercom_pt;
    uint16_t dma_trigger_id;
    pin_group_te sh1122_cs_pin_group;
    PIN_MASK_T sh1122_cs_pin_mask;
    pin_group_te sh1122_cd_pin_group;
    PIN_MASK_T sh1122_cd_pin_mask;
    gddram_px_t gddram_pixel[SH1122_OLED_HEIGHT];       // Buffer to merge adjascent pixels
    custom_fs_address_t currentFontAddress;             // Current font address
    font_header_t current_font_header;                  // Current font header
    unicode_interval_desc_t current_unicode_inters[15]; // Current unicode interval descriptors
    BOOL question_mark_support_described;               // If this font describes '?' support
    BOOL screen_wrapping_allowed;                       // If we are allowing screen wrapping
    BOOL carriage_return_allowed;                       // If we are allowing \r
    BOOL line_feed_allowed;                             // If we are allowing \n
    BOOL allow_text_partial_y_draw;                     // Allow drawing of text if they go over max Y
    BOOL allow_text_partial_x_draw;                     // Allow drawing of text if they go over max X
    BOOL screen_inverted;                               // If the screen is inverted
    uint16_t min_disp_y;                                // Min display Y
    uint16_t max_disp_y;                                // Max display Y
    uint16_t max_disp_x;                                // Max display X
    int16_t min_text_x;                                 // Min text X
    int16_t max_text_x;                                 // Max text X
    oled_align_te new_line_justify;                     // Justification for new lines
    int16_t new_line_x;                                 // X position when return occurs
    int16_t cur_text_x;                                 // Current x for writing text
    int16_t cur_text_y;                                 // Current y for writing text
    BOOL oled_on;                                       // Know if oled is on
    oled_transition_te loaded_transition;               // Loaded transition for full frame switch
    #ifdef OLED_INTERNAL_FRAME_BUFFER
    uint8_t frame_buffer[SH1122_OLED_HEIGHT][SH1122_OLED_WIDTH/(8/SH1122_OLED_BPP)];
    BOOL frame_buffer_flush_in_progress;
    #endif
} oled_descriptor_t;

/* Prototypes */
int16_t sh1122_put_string_xy(oled_descriptor_t* oled_descriptor, int16_t x, int16_t y, oled_align_te justify, const cust_char_t* string, BOOL write_to_buffer);
void sh1122_display_horizontal_pixel_line(oled_descriptor_t* oled_descriptor, int16_t x, uint16_t y, uint16_t width, uint8_t* pixels, BOOL write_to_buffer);
void sh1122_draw_rectangle(oled_descriptor_t* oled_descriptor, int16_t x, int16_t y, uint16_t width, uint16_t height, uint16_t color, BOOL write_to_buffer);
int16_t sh1122_get_start_x_for_string_based_on_alignment(oled_descriptor_t* oled_descriptor, int16_t x, oled_align_te justify, const cust_char_t* string);
void sh1122_draw_image_from_bitstream(oled_descriptor_t* oled_descriptor, int16_t x, int16_t y, bitstream_bitmap_t* bitstream, BOOL write_to_buffer);
void sh1122_draw_vertical_line(oled_descriptor_t* oled_descriptor, int16_t x, int16_t ystart, int16_t yend, uint8_t color, BOOL write_to_buffer);
RET_TYPE sh1122_display_bitmap_from_flash_at_recommended_position(oled_descriptor_t* oled_descriptor, uint32_t file_id, BOOL write_to_buffer);
RET_TYPE sh1122_display_bitmap_from_flash(oled_descriptor_t* oled_descriptor, int16_t x, int16_t y, uint32_t file_id, BOOL write_to_buffer);
void sh1122_init_display(oled_descriptor_t* oled_descriptor, BOOL leave_internal_logic_and_reflush_frame_buffer, uint8_t master_current);
uint16_t sh1122_get_number_of_printable_characters_for_string(oled_descriptor_t* oled_descriptor, int16_t x, const cust_char_t* string);
uint16_t sh1122_put_centered_string(oled_descriptor_t* oled_descriptor, uint8_t y, const cust_char_t* string, BOOL write_to_buffer);
void sh1122_put_centered_char(oled_descriptor_t* oled_descriptor, int16_t x, uint16_t y, cust_char_t c, BOOL write_to_buffer);
uint16_t sh1122_glyph_draw(oled_descriptor_t* oled_descriptor, int16_t x, int16_t y, cust_char_t ch, BOOL write_to_buffer);
void sh1122_erase_screen_and_put_top_left_emergency_string(oled_descriptor_t* oled_descriptor, const cust_char_t* string);
void sh1122_draw_full_screen_image_from_bitstream(oled_descriptor_t* oled_descriptor, bitstream_bitmap_t* bitstream);
int16_t sh1122_put_string(oled_descriptor_t* oled_descriptor, const cust_char_t* str, BOOL write_to_buffer);
uint16_t sh1122_get_glyph_width(oled_descriptor_t* oled_descriptor, cust_char_t ch, uint16_t* glyph_height);
void sh1122_fade_into_darkness(oled_descriptor_t* oled_descriptor, oled_transition_te transition);
int16_t sh1122_put_char(oled_descriptor_t* oled_descriptor, cust_char_t ch, BOOL write_to_buffer);
uint16_t sh1122_put_error_string(oled_descriptor_t* oled_descriptor, const cust_char_t* string);
void sh1122_set_contrast_current(oled_descriptor_t* oled_descriptor, uint8_t contrast_current);
void sh1122_load_transition(oled_descriptor_t* oled_descriptor, oled_transition_te transition);
void sh1122_set_discharge_charge_periods(oled_descriptor_t* oled_descriptor, uint8_t periods);
uint16_t sh1122_get_string_width(oled_descriptor_t* oled_descriptor, const cust_char_t* str);
void sh1122_set_discharge_vsl_level(oled_descriptor_t* oled_descriptor, uint8_t vsl_level);
void sh1122_set_screen_invert(oled_descriptor_t* oled_descriptor, BOOL screen_inverted);
void sh1122_move_display_start_line(oled_descriptor_t* oled_descriptor, int16_t offset);
RET_TYPE sh1122_refresh_used_font(oled_descriptor_t* oled_descriptor, uint16_t font_id);
void sh1122_set_colors_invert(oled_descriptor_t* oled_descriptor, BOOL colors_inverted);
void sh1122_add_emergency_dot_to_current_position(oled_descriptor_t* oled_descriptor);
void sh1122_write_single_command(oled_descriptor_t* oled_descriptor, uint8_t reg);
void sh1122_set_column_address(oled_descriptor_t* oled_descriptor, uint8_t start);
void sh1122_write_single_word(oled_descriptor_t* oled_descriptor, uint16_t data);
void sh1122_write_single_data(oled_descriptor_t* oled_descriptor, uint8_t data);
void sh1122_set_row_address(oled_descriptor_t* oled_descriptor, uint8_t start);
void sh1122_set_vsegm_level(oled_descriptor_t* oled_descriptor, uint8_t vsegm);
void sh1122_set_vcomh_level(oled_descriptor_t* oled_descriptor, uint8_t vcomh);
void sh1122_set_max_display_y(oled_descriptor_t* oled_descriptor, uint16_t y);
void sh1122_set_min_display_y(oled_descriptor_t* oled_descriptor, uint16_t y);
void sh1122_set_xy(oled_descriptor_t* oled_descriptor, int16_t x, int16_t y);
void sh1122_fill_screen(oled_descriptor_t* oled_descriptor, uint16_t color);
void sh1122_set_max_text_x(oled_descriptor_t* oled_descriptor, int16_t x);
void sh1122_set_min_text_x(oled_descriptor_t* oled_descriptor, int16_t x);
void sh1122_prevent_partial_text_y_draw(oled_descriptor_t* oled_descriptor);
void sh1122_prevent_partial_text_x_draw(oled_descriptor_t* oled_descriptor);
uint8_t sh1122_get_current_font_height(oled_descriptor_t* oled_descriptor);
void sh1122_allow_partial_text_y_draw(oled_descriptor_t* oled_descriptor);
void sh1122_allow_partial_text_x_draw(oled_descriptor_t* oled_descriptor);
void sh1122_clear_current_screen(oled_descriptor_t* oled_descriptor);
void sh1122_reset_lim_display_y(oled_descriptor_t* oled_descriptor);
void sh1122_set_emergency_font(oled_descriptor_t* oled_descriptor);
void sh1122_start_data_sending(oled_descriptor_t* oled_descriptor);
BOOL sh1122_is_screen_inverted(oled_descriptor_t* oled_descriptor);
void sh1122_prevent_line_feed(oled_descriptor_t* oled_descriptor);
void sh1122_stop_data_sending(oled_descriptor_t* oled_descriptor);
void sh1122_reset_max_text_x(oled_descriptor_t* oled_descriptor);
void sh1122_reset_min_text_x(oled_descriptor_t* oled_descriptor);
void sh1122_allow_line_feed(oled_descriptor_t* oled_descriptor);
BOOL sh1122_is_oled_on(oled_descriptor_t* oled_descriptor);
void sh1122_oled_off(oled_descriptor_t* oled_descriptor);
void sh1122_oled_on(oled_descriptor_t* oled_descriptor);

/* Depending on enabled features */
#ifdef OLED_INTERNAL_FRAME_BUFFER
void sh1122_flush_frame_buffer_window(oled_descriptor_t* oled_descriptor, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void sh1122_flush_frame_buffer_y_window(oled_descriptor_t* oled_descriptor, uint16_t ystart, uint16_t yend);
void sh1122_clear_y_frame_buffer(oled_descriptor_t* oled_descriptor, uint16_t ystart, uint16_t yend);
void sh1122_check_for_flush_and_terminate(oled_descriptor_t* oled_descriptor);
void sh1122_flush_frame_buffer(oled_descriptor_t* oled_descriptor);
void sh1122_clear_frame_buffer(oled_descriptor_t* oled_descriptor);
#endif

/* ifdef prototypes */
#ifdef OLED_PRINTF_ENABLED
    uint16_t sh1122_printf_xy(oled_descriptor_t* oled_descriptor, int16_t x, uint8_t y, oled_align_te justify, BOOL write_to_buffer, const char *fmt, ...);
#endif


#endif /* SH1122_H_ */
