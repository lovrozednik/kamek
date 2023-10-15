#pragma once

#include "common.hpp"
#include "binary_reader.hpp"

class Util
{
public:
    static ushort ReadBigUInt16(BinaryReader *br)
    {
        byte a = br->ReadByte();
        byte b = br->ReadByte();
        return (ushort)((a << 8) | b);
    }

    static uint ReadBigUInt32(BinaryReader *br)
    {
        ushort a = br->ReadBigUInt16();
        ushort b = br->ReadBigUInt16();
        return (uint)((a << 16) | b);
    }

    static int ReadBigInt32(BinaryReader *br)
    {
        ushort a = br->ReadBigUInt16();
        ushort b = br->ReadBigUInt16();
        return (int)((a << 16) | b);
    }

    static void WriteBE(BinaryWriter *bw, ushort value)
    {
        bw->Write((byte)(value >> 8));
        bw->Write((byte)(value & 0xFF));
    }

    static void WriteBE(BinaryWriter *bw, uint value)
    {
        bw->WriteBE((ushort)(value >> 16));
        bw->WriteBE((ushort)(value & 0xFFFF));
    }

    static ushort ExtractUInt16(byte *array, long offset)
    {
        return (ushort)((array[offset] << 8) | array[offset + 1]);
    }

    static uint ExtractUInt32(byte *array, long offset)
    {
        return (uint)((array[offset] << 24) | (array[offset + 1] << 16) |
                      (array[offset + 2] << 8) | array[offset + 3]);
    }

    static void InjectUInt16(byte *array, long offset, ushort value)
    {
        array[offset] = (byte)((value >> 8) & 0xFF);
        array[offset + 1] = (byte)(value & 0xFF);
    }

    static void InjectUInt32(byte *array, long offset, uint value)
    {
        array[offset] = (byte)((value >> 24) & 0xFF);
        array[offset + 1] = (byte)((value >> 16) & 0xFF);
        array[offset + 2] = (byte)((value >> 8) & 0xFF);
        array[offset + 3] = (byte)(value & 0xFF);
    }

    static std::string ExtractNullTerminatedString(byte *table, unsigned int tableLength, int offset)
    {
        if (offset >= 0 && offset < tableLength)
        {
            // find where it ends
            for (int i = offset; i < tableLength; i++)
            {
                if (table[i] == 0)
                {
                    return (char *)(table + offset);
                }
            }
        }

        return "null";
    }

    static void DumpToConsole(byte *array, unsigned int arrayLength)
    {
        int lines = arrayLength / 16;

        for (int offset = 0; offset < arrayLength; offset += 0x10)
        {
            writeline("%08x | ", offset);

            for (int pos = offset; pos < (offset + 0x10) && pos < arrayLength; pos++)
            {
                writeline("%02x ", array[pos]);
            }

            writeline("| ");

            for (int pos = offset; pos < (offset + 0x10) && pos < arrayLength; pos++)
            {
                if (array[pos] >= ' ' && array[pos] <= 0x7F)
                    writeline("%c", (char)array[pos]);
                else
                    writeline(".");
            }

            writeline("\n");
        }
    }
};
