#pragma once

#include "common.hpp"
#include "util.hpp"

class Dol
{
public:
    struct Section
    {
        uint LoadAddress;
        sized_array *Data;

        uint EndAddress()
        {
            return (uint)(LoadAddress + Data->length);
        }

        ~Section()
        {
            delete Data;
        }
    };

    Section *Sections;
    uint EntryPoint;
    uint BssAddress, BssSize;

    ~Dol()
    {
        delete[] Sections;
    }

    Dol(byte *input)
    {
        Sections = new Section[18];
        BinaryReader *br = new BinaryReader(input);

        uint *fields = new uint[3 * 18];
        for (int i = 0; i < 3 * 18; i++)
            fields[i] = br->ReadBigUInt32();

        for (int i = 0; i < 18; i++)
        {
            uint fileOffset = fields[i];
            uint size = fields[36 + i];
            Sections[i].LoadAddress = fields[18 + i];
            Sections[i].Data = new sized_array(br->ReadBytes((int)size), size);
        }

        BssAddress = br->ReadBigUInt32();
        BssSize = br->ReadBigUInt32();
        EntryPoint = br->ReadBigUInt32();
    }

    uint64_t Write(byte *output)
    {
        auto bw = new BinaryWriter(output);

        // Generate the header
        auto fields = new uint[3 * 18];
        uint position = 0x100;
        for (int i = 0; i < 18; i++)
        {
            if (Sections[i].Data->length > 0)
            {
                fields[i] = position;
                fields[i + 18] = Sections[i].LoadAddress;
                fields[i + 36] = (uint)Sections[i].Data->length;
                position += (uint)((Sections[i].Data->length + 0x1F) & ~0x1F);
            }
        }

        for (int i = 0; i < (3 * 18); i++)
            bw->WriteBE(fields[i]);
        bw->WriteBE(BssAddress);
        bw->WriteBE(BssSize);
        bw->WriteBE(EntryPoint);
        bw->Write(new sized_array(0x100 - 0xE4));

        // Write all sections
        for (int i = 0; i < 18; i++)
        {
            bw->Write(Sections[i].Data);

            int paddedLength = ((Sections[i].Data->length + 0x1F) & ~0x1F);
            int padding = paddedLength - Sections[i].Data->length;
            if (padding > 0)
                bw->Write(new sized_array(padding));
        }

        return bw->position;
    }

    bool ResolveAddress(uint address, int* sectionID, uint* offset)
    {
        for (int i = 0; i < Sections->Data->length; i++)
        {
            if (address >= Sections[i].LoadAddress && address < Sections[i].EndAddress())
            {
                *sectionID = i;
                *offset = address - Sections[i].LoadAddress;
                return true;
            }
        }

        *sectionID = -1;
        *offset = 0;
        return false;
    }

    uint ReadUInt32(uint address)
    {
        int sectionID;
        uint offset;
        if (!ResolveAddress(address, &sectionID, &offset))
            writeline("address out of range in DOL file");

        return Util::ExtractUInt32(Sections[sectionID].Data->data, offset);
    }

    void WriteUInt32(uint address, uint value)
    {
        int sectionID;
        uint offset;
        if (!ResolveAddress(address, &sectionID, &offset))
            writeline("address out of range in DOL file");

        Util::InjectUInt32(Sections[sectionID].Data->data, offset, value);
    }

    ushort ReadUInt16(uint address)
    {
        int sectionID;
        uint offset;
        if (!ResolveAddress(address, &sectionID, &offset))
            writeline("address out of range in DOL file");

        return Util::ExtractUInt16(Sections[sectionID].Data->data, offset);
    }

    void WriteUInt16(uint address, ushort value)
    {
        int sectionID;
        uint offset;
        if (!ResolveAddress(address, &sectionID, &offset))
            writeline("address out of range in DOL file");

        Util::InjectUInt16(Sections[sectionID].Data->data, offset, value);
    }

    byte ReadByte(uint address)
    {
        int sectionID;
        uint offset;
        if (!ResolveAddress(address, &sectionID, &offset))
            writeline("address out of range in DOL file");

        return Sections[sectionID].Data->data[offset];
    }

    void WriteByte(uint address, byte value)
    {
        int sectionID;
        uint offset;
        if (!ResolveAddress(address, &sectionID, &offset))
            writeline("address out of range in DOL file");

        Sections[sectionID].Data->data[offset] = value;
    }
};