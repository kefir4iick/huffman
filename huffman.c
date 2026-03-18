#include "huffman.h"
#include "bitstream.h"
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

typedef struct HuffNode {
    uint32_t freq;
    int symbol;
    struct HuffNode *left;
    struct HuffNode *right;
} HuffNode;

typedef struct {
    HuffNode *a[512];
    size_t n;
} Heap;

static void HeapPush(Heap *h, HuffNode *x) {
    size_t i = h->n++;
    while (i && h->a[(i-1)/2]->freq > x->freq) {
        h->a[i] = h->a[(i-1)/2];
        i = (i-1)/2;
    }
    h->a[i] = x;
}

static HuffNode *HeapPop(Heap *h) {
    HuffNode *r = h->a[0];
    HuffNode *x = h->a[--h->n];
    size_t i = 0;
    while (2*i + 1 < h->n) {
        size_t j = 2*i + 1;
        if (j+1 < h->n && h->a[j+1]->freq < h->a[j]->freq)
            j++;
        if (h->a[j]->freq >= x->freq)
            break;
        h->a[i] = h->a[j];
        i = j;
    }
    h->a[i] = x;
    return r;
}

static HuffNode *NewNode(uint32_t f, int s, HuffNode *l, HuffNode *r) {
    HuffNode *n = malloc(sizeof(HuffNode));
    n->freq = f;
    n->symbol = s;
    n->left = l;
    n->right = r;
    return n;
}

static HuffNode *BuildTree(uint32_t freq[256]) {
    Heap h = { .n = 0 };
    for (int i = 0; i < 256; i++) {
        if (freq[i])
            HeapPush(&h, NewNode(freq[i], i, NULL, NULL));
    }
    if (h.n == 0)
        return NULL;               
    if (h.n == 1)
        return HeapPop(&h);
    while (h.n > 1) {
        HuffNode *a = HeapPop(&h);
        HuffNode *b = HeapPop(&h);
        HeapPush(&h, NewNode(a->freq + b->freq, -1, a, b));
    }
    return HeapPop(&h);
}

void FreeHuffmanTree(HuffNode *node) {
    if (node == NULL) {
        return;
    }
    FreeHuffmanTree(node->left);
    FreeHuffmanTree(node->right);
    free(node);
}

static void Serialize(HuffNode *n, BitStream *bs) {
    if (!n->left && !n->right) {
        WriteUInt64(bs, 1, 1);
        WriteUInt64(bs, n->symbol, 8);
        return;
    }
    WriteUInt64(bs, 0, 1);
    Serialize(n->left, bs);
    Serialize(n->right, bs);
}

static HuffNode *Deserialize(BitStream *bs) {
    int type = ReadUInt64(bs, 1);
    if (type == 1) {
        int sym = ReadUInt64(bs, 8);
        return NewNode(0, sym, NULL, NULL);
    }
    HuffNode *left = Deserialize(bs);
    HuffNode *right = Deserialize(bs);
    return NewNode(0, -1, left, right);
}

typedef struct {
    uint8_t bits[32];
    size_t len;
} Code; //for build code

static void BuildCodes(HuffNode *n, Code table[256], uint8_t path[32], size_t depth)
{
    if (!n->left && !n->right) {
        table[n->symbol].len = depth;
        memcpy(table[n->symbol].bits, path, (depth + 7) / 8);
        return;
    }
    if (n->left) {
        uint8_t p[32];
        memcpy(p, path, 32);
        p[depth/8] &= ~(1 << (7 - depth % 8));
        BuildCodes(n->left, table, p, depth + 1);
    }
    if (n->right) {
        uint8_t p[32];
        memcpy(p, path, 32);
        p[depth/8] |= (1 << (7 - depth % 8));
        BuildCodes(n->right, table, p, depth + 1);
    }
}


void HuffmanEncodeCompact(int in_fd, int out_fd) {
    uint32_t freq[256] = {0};
    uint8_t buf[4096];
    ssize_t r;

    while ((r = read(in_fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < r; i++)
            freq[buf[i]]++;
    }

    off_t size = lseek(in_fd, 0, SEEK_END);
    if (size == (off_t)-1) {
        perror("lseek");
        exit(1);
    }
    lseek(in_fd, 0, SEEK_SET);

    HuffNode *root = BuildTree(freq);
    if (root == NULL) {  
        BitStream *bs = BitStreamOpenFD(out_fd, 1);
        WriteUInt64(bs, 0, 64);
        BitStreamFlush(bs);
        BitStreamClose(bs);
        return;
    }

    BitStream *bs = BitStreamOpenFD(out_fd, 1);
    WriteUInt64(bs, size, 64);
    BitStreamFlush(bs);   

    Serialize(root, bs);

    Code table[256] = {0};
    uint8_t path[32] = {0};
    BuildCodes(root, table, path, 0);

    lseek(in_fd, 0, SEEK_SET);
    while ((r = read(in_fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < r; i++) {
            WriteBitSequence(bs,
                             table[buf[i]].bits,
                             table[buf[i]].len);
        }
    }

    BitStreamClose(bs);
    FreeHuffmanTree(root);
}


void HuffmanDecodeCompact(int in_fd, int out_fd) {
    BitStream *bs = BitStreamOpenFD(in_fd, 0);
    uint64_t size = ReadUInt64(bs, 64);

    if (size == 0) {
        BitStreamClose(bs);
        return;
    }

    HuffNode *root = Deserialize(bs);

    if (!root->left && !root->right) {
        uint8_t x = root->symbol;
        for (uint64_t i = 0; i < size; i++) {
            if (write(out_fd, &x, 1) != 1) {
                perror("write");
                exit(1);
            }
        }
        FreeHuffmanTree(root);
        BitStreamClose(bs);
        return;
    }

    for (uint64_t i = 0; i < size; i++) {
        HuffNode *c = root;
        while (c->left || c->right) {
            int bit = ReadUInt64(bs, 1);
            c = bit ? c->right : c->left;
            if (!c) {
                fprintf(stderr, "corrupted archive\n");
                FreeHuffmanTree(root);
                BitStreamClose(bs);
                exit(1);
            }
        }
        uint8_t x = c->symbol;
        if (write(out_fd, &x, 1) != 1) {
            perror("write");
            FreeHuffmanTree(root);
            BitStreamClose(bs);
            exit(1);
        }
    }

    FreeHuffmanTree(root);
    BitStreamClose(bs);
}
