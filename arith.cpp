#include <iostream>
#include <stddef.h>
#include <string.h>

#include "arith.h"

//
// set_bit
//
void set_bit(uint8_t *buffer_bits, int position, bool state) {
  // set specific bit in byte buffer
  if (buffer_bits != NULL) {
    if (state)
      buffer_bits[position / 8] |= 1 << (7 - (position % 8));
    else
      buffer_bits[position / 8] &= ~(1 << (7 - (position % 8)));
  }
} /* set_bit() */
//
// get_bit
//
bool get_bit(uint8_t *buffer_bits, int position) {
  // get specific bit of byte buffer
  if (position < 0) {
    return 0;
  }
  if (buffer_bits[position / 8] & (1 << (7 - (position & 7))))
    return 1;
  else
    return 0;
} /* get_bit() */
//
// get_pixel_in
//
bool get_pixel_in(image_s *image_input, int x, int y) {
  // get pixel from byte buffer
  if ((x >= 0) && (x < image_input->width) && (y >= 0) &&
      (y < image_input->height)) {
    return get_bit(&image_input->current_line[1], x);
  }
  return false;
} /* get_pixel_in() */

//
// The key is formed with a 3 pixel group around the current pixel
// C = current pixel, K = key pixels (number = bit number)
//
// +---*+----+
// | K0 | K1 |
// +----+----+
// | K2 | C  |
// +----+----+
//
int get_key_from_pixel(image_s *image_input, int x, int y) {
  // this function reads the 0 6 7 pixels of the bin_image data
  uint8_t p0 = get_bit(image_input->previous_line + 1, x - 1);
  uint8_t p1 = get_bit(image_input->previous_line + 1, x);
  uint8_t p2 = get_bit(image_input->current_line + 1, x - 1);

  int kp = p0 | p1 << 1 | p2 << 2;

  return kp;
} /* get_key_from_pixel() */
//
// get_key_value
//
int get_key_value(image_s *image_input, int x, int y, uint8_t *key_color,
                  entropy_calc_s *entropy) {
  // reads the key len and key color of the array based on the key_position
  int key_position = get_key_from_pixel(image_input, x, y);

  *key_color = entropy->key_color_out[key_position];
  return entropy->key_len_out[key_position];
} /* get_key_value() */
//
// clean_last_bits
//
void clean_last_bits(encode_data_s *encode_data) {
  // for every packet bigger then 255 we erase the last bits counting them to
  // one main bit
  int cur_bits_pos = encode_data->out.len - 1;

  if (cur_bits_pos >= 0 && encode_data->out.buffer != NULL) {
    while (1) {
      if (!get_bit(encode_data->out.buffer, cur_bits_pos))
        break;
      set_bit(encode_data->out.buffer, cur_bits_pos, 0);
      cur_bits_pos--;
      if (cur_bits_pos < 0)
        return;
    }
    set_bit(encode_data->out.buffer, cur_bits_pos, 1);
  }
} /* clean_last_bits() */
//
// write_next_bit
//
// If the current state has (countdown_bits & 0x80), append 1, else, append 0
//
// Shift countdown_bits left, saturating at 0xff
// Shift cur_part_pixel_count left
//
void write_next_bit(encode_data_s *encode_data) {
  // write next bit in output buffer and the needed meta data
  encode_data->cur_part_pixel_count *= 2;
  uint32_t cur_bits_len = encode_data->out.len;

  if (cur_bits_len < encode_data->out.len_max) {
    set_bit(encode_data->out.buffer, cur_bits_len,
            encode_data->countdown_bits & 0x80);
    encode_data->out.len++;
  }
  encode_data->countdown_bits = (encode_data->countdown_bits * 2) & 0xff;
} /* write_next_bit() */
//
// handle_bit_decode
//
void handle_bit_decode(encode_data_s *encode_data, int key_value, int key_color,
                       int pixel_color) {
  // process the current pixel color and the given key data
  int cur_pixel_count = encode_data->cur_part_pixel_count;
  int trimmed_max_pixel = cur_pixel_count >> key_value;

  if (pixel_color == key_color) {
    encode_data->cur_part_pixel_count = trimmed_max_pixel;

    // noop cur_part_pixel_count, but keep writing bits
    for (int i = key_value; i > 0; i--) {
      write_next_bit(encode_data);
    }
  } else {
    // cur_part_pixel_count = cur_part_pixel_count - (cur_part_pixel_count >>
    // (key_value))
    encode_data->countdown_bits += trimmed_max_pixel;
    encode_data->cur_part_pixel_count = cur_pixel_count - trimmed_max_pixel;

    // trimmed_max_pixel = number of times we've called write_next_bit
    if (encode_data->countdown_bits &
        0x100) { // for every packet bigger then 255 we erase the last bits
                 // counting them to one main bit
      encode_data->countdown_bits = encode_data->countdown_bits & 0xff;
      clean_last_bits(encode_data);
    }

    if (!(encode_data->cur_part_pixel_count & 0x80)) {
      write_next_bit(encode_data);
    }
  }
} /* handle_bit_decode() */
//
// complete_last_bit_part
//
void complete_last_bit_part(encode_data_s *encode_data) {
  // when all bits where encoded we need to close the current part, its done
  // here
  for (int i = 0; i < 8; i++) {
    write_next_bit(encode_data);
  }
} /* complete_last_bit_part() */
//
// calculate_entropy
//
void calculate_entropy(uint8_t *pBitmap, image_s *bin_image_input,
                       _bmp_s *bmp_infos, entropy_calc_s *calc) {
  // clear line buffer beyond our image size so that we don't
  // have to check boundaries on every pixel
  memset(&bin_image_input->current_line[0], 0,
         sizeof(bin_image_input->current_line));

  // go over every pixel and save the average of the len between each key change
  for (int row_idx = 0; row_idx < bin_image_input->height; row_idx++) {
    // Current line becomes the previous
    memcpy(bin_image_input->previous_line, bin_image_input->current_line,
           sizeof(bin_image_input->current_line));
    // Get a new line of image
    uint8_t *start = NULL;
    if (bmp_infos->bTopDown) {
      start = &pBitmap[bmp_infos->offset + bmp_infos->stride * row_idx];
    } else {
      start = &pBitmap[bmp_infos->offset +
                       bmp_infos->stride * (bmp_infos->height - 1 - row_idx)];
    }
    memcpy(&bin_image_input->current_line[1], start, bmp_infos->stride);

    for (int col_idx = 0; col_idx < bin_image_input->width; col_idx++) {

      calc->cur_key_value =
          get_key_from_pixel(bin_image_input, col_idx, row_idx);
      calc->cur_key_color = get_pixel_in(bin_image_input, col_idx, row_idx);

      if ((calc->cur_key_value != calc->last_key_value) &&
          (calc->cur_key_color == calc->last_key_color)) {

        calc->key_average[calc->last_key_value] =
            (calc->key_average[calc->last_key_value] + calc->cur_key_counter) /
            2;

        calc->cur_key_counter = 1;
        calc->last_key_value = calc->cur_key_value;
      } else {
        calc->cur_key_counter++;
      }

      calc->last_key_color = calc->cur_key_color;
      // save how many pixels for each key are black or white
      if (calc->cur_key_color) {
        calc->color_1_count[calc->cur_key_value]++;
      } else {
        calc->color_0_count[calc->cur_key_value]++;
      }
    }
  }

  // calculate the color and len average to rounded numbers
  for (int i = 0; i < 8; i++) {
    calc->key_len_out[i] = (int)(calc->key_average[i] + 0.5f);
    if (calc->key_len_out[i] < 1)
      calc->key_len_out[i] = 1;
    else if (calc->key_len_out[i] > 7)
      calc->key_len_out[i] = 7;
    calc->key_color_out[i] = (calc->color_0_count[i] > calc->color_1_count[i]);
  }

  // format the key into a byte form for the output in the buffer later
  for (int i = 0; i < 4; i++) {
    calc->key[i] =
        (((calc->key_len_out[i * 2] << 1) | calc->key_color_out[i * 2]) << 4 |
         ((calc->key_len_out[(i * 2) + 1] << 1) |
          calc->key_color_out[(i * 2) + 1]));
  }
} /* calculate_entropy() */
//
// encode_raw_image
//
// Pass a NULL value for the output buffer ptr to
// know how big the compressed data will be.
//
// Input data can come from a file or bitmap in memory. if File is NULL
// then the data will be read from the memory pointer and assumed to be a
// top-down bitmap
//
// bmp_infos->width must be <= 80b (current_line is DWORD-aligned with a buffer
// to 84)
//
size_t encode_raw_image(uint8_t *pBitmap, _bmp_s *bmp_infos,
                        uint8_t *output_bit_buffer, size_t max_output_size) {

  encode_data_s encode_data;
  entropy_calc_s entropy = {0};
  image_s image = {bmp_infos->height, bmp_infos->width};

  uint8_t key_color;

  if (bmp_infos->height < 1 || bmp_infos->width < 1)
    return 0; // if we dont have any width or heigth there is nothing do encode

  encode_data.out.len = 0;
  encode_data.out.len_max = ((max_output_size - 8) * 8);
  encode_data.out.buffer = NULL;
  encode_data.cur_part_pixel_count = 0xff;
  encode_data.countdown_bits = 0;

  calculate_entropy(pBitmap, &image, bmp_infos, &entropy);

  if (output_bit_buffer != NULL) {
    output_bit_buffer[0] = 0x03; // entropy description
    output_bit_buffer[1] = 0x00;
    output_bit_buffer[2] = 0x06;
    output_bit_buffer[3] = 0x07;
    output_bit_buffer[4] = entropy.key[0]; // 4 bytes entropy keys and color
    output_bit_buffer[5] = entropy.key[1];
    output_bit_buffer[6] = entropy.key[2];
    output_bit_buffer[7] = entropy.key[3];

    encode_data.out.buffer = &output_bit_buffer[8];
  }

  key_color = 0;

  memset(&image.current_line[0], 0, sizeof(image.current_line));

  for (int row_idx = 0; row_idx < image.height; row_idx++) {
    // Current line becomes the previous
    memcpy(image.previous_line, image.current_line, sizeof(image.current_line));
    // Get a new line of image
    uint8_t *start = NULL;
    if (bmp_infos->bTopDown) {
      start = &pBitmap[bmp_infos->offset + bmp_infos->stride * row_idx];
    } else {
      start = &pBitmap[bmp_infos->offset +
                       bmp_infos->stride * (bmp_infos->height - 1 - row_idx)];
    }
    memcpy(&image.current_line[1], start, bmp_infos->stride);

    for (int col_idx = 0; col_idx < image.width; col_idx++) {

      uint8_t key_value =
          get_key_value(&image, col_idx, row_idx, &key_color, &entropy);

      bool pixel_color = get_pixel_in(&image, col_idx, row_idx);

      handle_bit_decode(&encode_data, key_value, key_color, pixel_color);
    }
  }
  complete_last_bit_part(&encode_data);

  return 8 /*8bytes for the entropy data*/ + (encode_data.out.len / 8) +
         ((encode_data.out.len % 8) ? 1 : 0) /*length rounded to next byte*/;
} /* encode_raw_image() */

size_t fill_header(uint8_t *buffer_out, size_t compression_size, int height,
                   int width, int compression_type, int color, int header_size,
                   uint16_t checksum) {
  memset(buffer_out, 0, header_size);
  // fill the output data so it can directly be send to the display
  buffer_out[0] = 0x83;
  buffer_out[1] = 0x19;
  buffer_out[2] = 0;
  buffer_out[3] = (uint8_t)width;
  buffer_out[4] = (uint8_t)(width >> 8);
  buffer_out[5] = (uint8_t)height;
  buffer_out[6] = (uint8_t)(height >> 8);
  buffer_out[7] = (uint8_t)(compression_size >> 0);
  buffer_out[8] = (uint8_t)(compression_size >> 8);

  buffer_out[22] = (uint8_t)(0x100 - checksum);
  buffer_out[23] = compression_type;

  buffer_out[26] = color ? 0x21 : 0x00; // 0 for one color 21 for two color

  buffer_out[27] = 0x84;

  if (header_size == 32) {
    buffer_out[28] = 0x80;
    buffer_out[29] = 0x00;
    buffer_out[30] = (uint8_t)(compression_size >> 8);
    buffer_out[31] = (uint8_t)(compression_size >> 0);
  } else {
    buffer_out[28] = (uint8_t)(compression_size >> 8) | 0x80;
    buffer_out[29] = (uint8_t)(compression_size >> 0);
  }
  buffer_out[compression_size + header_size + 0] = 0x85;
  buffer_out[compression_size + header_size + 1] = 0x05;
  buffer_out[compression_size + header_size + 2] = 0x08;
  buffer_out[compression_size + header_size + 3] = 0x00;
  buffer_out[compression_size + header_size + 4] = 0x00;
  buffer_out[compression_size + header_size + 5] = 0x01;
  buffer_out[compression_size + header_size + 6] = 0x01;

  return header_size + compression_size + 7;
}
