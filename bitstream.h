#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stddef.h>

typedef struct BitStream BitStream;

BitStream *BitStreamOpenFD(int fd, int mode);
void BitStreamClose(BitStream *bs);

void WriteBitSequence(BitStream *bs, const uint8_t *data, size_t bit_len);
void ReadBitSequence(BitStream *bs, uint8_t *out, size_t bit_len);


void WriteUInt64(BitStream *bs, uint64_t x, size_t bit_len);
uint64_t ReadUInt64(BitStream *bs, size_t bit_len);

void BitStreamFlush(BitStream *bs);

#endif 
