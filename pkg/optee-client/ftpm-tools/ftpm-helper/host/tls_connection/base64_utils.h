#ifndef __BASE64_UTILS_H__
#define __BASE64_UTILS_H__

#include <stdint.h>
#include <stdlib.h>

char *base64_encode(const uint8_t *data, size_t input_length, size_t *output_length);

uint8_t *base64_decode(const char *data, size_t input_length, size_t *output_length);

#endif
