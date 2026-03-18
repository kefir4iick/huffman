#include "bitstream.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

struct BitStream {
    int fd;
    uint8_t buffer;   
    int bit_pos;      
    int mode;         
};



BitStream *BitStreamOpenFD(int fd, int mode)
{
    BitStream *bs = malloc(sizeof(BitStream));
    if (!bs)
        return NULL;

    bs->fd = fd;
    bs->buffer = 0;

    bs->bit_pos = 0;

    bs->mode = mode;

    return bs;
}

void BitStreamClose(BitStream *bs) {
    if (!bs)
        return;

    if (bs->mode == 1 && bs->bit_pos != 0) {
        bs->buffer <<= (8 - bs->bit_pos);
        write(bs->fd, &bs->buffer, 1);
    }

    free(bs);
}


static void WriteBit(BitStream *bs, int bit) {
    bs->buffer = (bs->buffer << 1) | (bit & 1);
    bs->bit_pos++;

    if (bs->bit_pos == 8) {
        write(bs->fd, &bs->buffer, 1);
        bs->buffer = 0;
        bs->bit_pos = 0;
    }
}

static int ReadBit(BitStream *bs, int *bit)
{
    if (bs->bit_pos == 0) {
        ssize_t r = read(bs->fd, &bs->buffer, 1);
        if (r != 1)
            return 0;
        bs->bit_pos = 8;
    }

    *bit = (bs->buffer >> (bs->bit_pos - 1)) & 1;
    bs->bit_pos--;

    return 1;
}


void WriteBitSequence(BitStream *bs, const uint8_t *data, size_t bit_len)
{
    for (size_t i = 0; i < bit_len; i++) {
        size_t byte = i / 8;
        int bit = (data[byte] >> (7 - (i % 8))) & 1;
        WriteBit(bs, bit);
    }
}

void ReadBitSequence(BitStream *bs, uint8_t *out, size_t bit_len)
{
    size_t bytes = (bit_len + 7) / 8;
    for (size_t i = 0; i < bytes; i++)
        out[i] = 0;

    for (size_t i = 0; i < bit_len; i++) {
        int bit;
        if (!ReadBit(bs, &bit))
            break;

        size_t byte = i / 8;
        out[byte] |= bit << (7 - (i % 8));
    }
}



void WriteUInt64(BitStream *bs, uint64_t x, size_t bit_len) {
    for (size_t i = bit_len; i-- > 0; ) {
        int bit = (x >> i) & 1;
        WriteBit(bs, bit);
    }
}

uint64_t ReadUInt64(BitStream *bs, size_t bit_len) {
    uint64_t x = 0;
    for (size_t i = 0; i < bit_len; i++) {
        int bit;
        if (!ReadBit(bs, &bit)) {
            fprintf(stderr, "corrupted archive\n");
            exit(1);
        }
        x = (x << 1) | bit;
    }
    return x;
}

void BitStreamFlush(BitStream *bs)
{
    if (bs->mode == 1 && bs->bit_pos != 0) {
        bs->buffer <<= (8 - bs->bit_pos);
        write(bs->fd, &bs->buffer, 1);
        bs->buffer = 0;
        bs->bit_pos = 0;
    }
}
