#pragma once

#include <stdint.h>

typedef struct _bmp_s_tag {
  int height;
  int width;
  int offset;
  int stride;
  int bTopDown;
} _bmp_s;

typedef struct image_s_tag {
  int height;
  int width;
  uint8_t current_line[84];
  uint8_t previous_line[84];
} image_s;

typedef struct output_data_s_tag {
  uint32_t len;
  uint32_t len_max;
  uint8_t *buffer;
} output_data_s;

typedef struct encode_data_s_tag {
  output_data_s out;
  uint32_t cur_part_pixel_count;
  uint32_t countdown_bits;
} encode_data_s;

typedef struct entropy_calc_s_tag {
  int cur_key_value;
  int last_key_value;
  int cur_key_counter;
  float key_average[8];
  int cur_key_color;
  int last_key_color;
  int key_color_out[8];
  int key_len_out[8];
  int color_1_count[8];
  int color_0_count[8];
  unsigned char key[4];
} entropy_calc_s;

void set_bit(uint8_t *buffer_bits, int position, bool state);
bool get_bit(uint8_t *buffer_bits, int position);
bool get_pixel_in(image_s *image_input, int x, int y);
int get_key_from_pixel(image_s *image_input, int x, int y);
int get_key_value(image_s *image_input, int x, int y, uint8_t *key_color,
                  entropy_calc_s *entropy);
void clean_last_bits(encode_data_s *encode_data);
void write_next_bit(encode_data_s *encode_data);
void handle_bit_decode(encode_data_s *encode_data, int key_value, int key_color,
                       int pixel_color);
void complete_last_bit_part(encode_data_s *encode_data);
void calculate_entropy(uint8_t *pBitmap, image_s *bin_image_input,
                       _bmp_s *bmp_infos, entropy_calc_s *calc);
size_t encode_raw_image(uint8_t *pBitmap, _bmp_s *bmp_infos,
                        uint8_t *output_bit_buffer, size_t max_output_size);

size_t fill_header(uint8_t *output_bit_buffer, size_t payload_size, int height,
                   int width, int compression_type, bool color, int header_size,
                   uint16_t checksum);
