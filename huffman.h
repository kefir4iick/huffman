#ifndef HUFFMAN_H
#define HUFFMAN_H

void HuffmanEncodeCompact(int in_fd, int out_fd);
void HuffmanDecodeCompact(int in_fd, int out_fd);

void FreeHuffmanTree(HuffNode *node);

#endif
