#include <fstream>
#include <iostream>
#include <vector>

#include "arith.h"

#define HEADER_SIZE 32

size_t compress_bmp(std::string &path, _bmp_s *bmp_info, uint8_t *out,
                    size_t out_max_size) {
  std::ifstream inFile(path, std::ios_base::binary);

  if (!inFile.is_open()) {
    std::cerr << "Failed to open " << path << std::endl;
    return -1;
  }

  inFile.seekg(0, std::ios_base::end);
  size_t length = inFile.tellg();
  std::cout << "BMP file has length " << length << std::endl;
  inFile.seekg(0, std::ios_base::beg);

  std::vector<char> buffer;
  buffer.reserve(length);
  std::copy(std::istreambuf_iterator<char>(inFile),
            std::istreambuf_iterator<char>(), std::back_inserter(buffer));

  if (buffer[0] != 'B' && buffer[1] != 'M') {
    std::cerr << "Not a Windows BMP file" << std::endl;
  }

  // First 14 bytes are the bitmap file header; the last int is the offset to
  // the pixel data.
  int32_t offset = *reinterpret_cast<int32_t *>(&buffer[10]);

  // There are 7 different versions of the DIB header. We support only
  // `BITMAPINFOHEADER`. We can distinguish the header format by size.
  if (buffer[14] != 40 || buffer[15] != 0 || buffer[16] != 0 ||
      buffer[17] != 0) {
    std::cerr << "BMP DIB format is not BITMAPINFOHEADER" << std::endl;
    return -1;
  }

  int32_t width = *reinterpret_cast<int32_t *>(&buffer[18]);
  int32_t height = *reinterpret_cast<int32_t *>(&buffer[22]);
  int16_t bpp = *reinterpret_cast<int16_t *>(&buffer[28]);

  std::cout << "dimensions: " << width << "x" << height << " @ " << bpp
            << " bpp" << std::endl;

  if (bpp != 1) {
    std::cerr << "This program supports only 1bpp (binary) bitmaps"
              << std::endl;
  }

  if (buffer[30] != 0) {
    std::cerr << "This program supports only uncompressed (BI_RGB) bitmaps"
              << std::endl;
  }

  // Print the color table - we only support 1bpp
  // Color table starts after BMP header followed by DIB header.
  size_t color_table_start = 14 + 40;
  int32_t *colors = reinterpret_cast<int32_t *>(&buffer[color_table_start]);
  if (*colors != 0) {
    std::cout << "First color is not 0x0000000!" << std::endl;
    return -1;
  }

  // Row size in bytes (rounded up)
  int32_t bsize = (width + 7) / 8;
  // The pixel array is always DWORD aligned.
  int32_t stride = (bsize + 3) & 0xfffc;
  std::cout << "pixel data starts at offset " << offset << " with stride of "
            << stride << " (bsize=" << bsize << ")" << std::endl;

  bmp_info->height = (height > 0) ? height : -height;
  bmp_info->width = width;
  bmp_info->offset = offset;
  bmp_info->stride = stride;
  bmp_info->bTopDown = height < 0;

  size_t s = encode_raw_image(reinterpret_cast<uint8_t *>(buffer.data()),
                              bmp_info, &out[32], out_max_size - 32 - 7);
  return s;
}

int main(int argc, char **argv) {
  std::cout << "Arithmetic coding compression program" << std::endl;

  if (argc != 3 && argc != 4) {
    std::cerr << "Usage:" << std::endl;
    std::cerr << "\t" << argv[0] << " <filename>.bmp <output>.bin" << std::endl;
    std::cerr << "\t" << argv[0]
              << " <filename>.bmp <filename2>.bmp <output>.bin" << std::endl;
    return 1;
  }

  uint8_t out[65536];

  _bmp_s bmp_info;

  std::string p = argv[1];
  size_t s = compress_bmp(p, &bmp_info, out, sizeof(out) - HEADER_SIZE - 7);
  if (s == -1) {
      return 1;
  }
  if (argc == 4) {
    p = argv[2];
    s += compress_bmp(p, &bmp_info, out, sizeof(out) - HEADER_SIZE - 7 - s);
  }
  // header size is 32 for chroma74, 30 for chroma29
  // checksum can always be zero.
  size_t total_size = fill_header(out, s, bmp_info.height, bmp_info.width, 2,
                                  argc == 4, HEADER_SIZE, 0);

  std::cout << "Compressed size: " << s << std::endl;
  std::cout << "Total size: " << total_size << std::endl;

  uint32_t x = 0;
  for (uint32_t i = 0; i < total_size; ++i) {
    x = (x + x ^ out[i]);
  }
  std::cout << "XOR hash: " << x << std::endl;

  std::ofstream fout;
  fout.open(argv[argc - 1], std::ios::binary | std::ios::out);
  fout.write(reinterpret_cast<char *>(out), total_size);
  fout.close();

  return 0;
}
