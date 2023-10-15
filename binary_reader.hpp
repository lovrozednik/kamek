#pragma once

#include <string.h>
#include "common.hpp"

class BinaryReader
{
public:
    unsigned int position = 0;

    unsigned char *data;

    unsigned int ReadBigUInt32() { return __builtin_bswap32(*((unsigned int *)(data + (position += 4)))); }
    unsigned char ReadByte() { return *((unsigned char *)(data + (position++))); }
    unsigned int ReadBigUInt16() { return __builtin_bswap16(*((unsigned short *)(data + (position += 2)))); }
    unsigned int ReadBigInt32() { return __builtin_bswap32(*((int *)(data + (position += 2)))); }

    unsigned char *ReadBytes(int size) { return data + position; };

    BinaryReader(unsigned char *input) { data = input; };
};

class BinaryWriter
{
public:
    unsigned int position = 0;

    unsigned char *data;

    void Write(byte x) { data[position++] = x; };
    void Write(sized_array* x) { memcpy(data, x->data, x->length); };
    void WriteBE(ushort x) { data[position += 2] = __builtin_bswap16(x); };
    void WriteBE(uint x) { data[position += 4] = __builtin_bswap32(x); };

    BinaryWriter(unsigned char *_d) { data = _d; };
};