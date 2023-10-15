#pragma once

#include "common.hpp"
#include "util.hpp"

class Elf
{
public:
    class ElfHeader
    {
    public:
        uint ei_mag;
        unsigned char ei_class, ei_data, ei_version, ei_osabi, ei_abiversion;
        ushort e_type, e_machine;
        uint e_version, e_entry, e_phoff, e_shoff, e_flags;
        ushort e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;

        static ElfHeader *Read(BinaryReader *reader)
        {
            ElfHeader *h = new ElfHeader();

            h->ei_mag = reader->ReadBigUInt32();
            if (h->ei_mag != 0x7F454C46) // "\x7F" "ELF"
                writeline("Incorrect ELF header");

            h->ei_class = reader->ReadByte();
            if (h->ei_class != 1)
                writeline("Only 32-bit ELF files are supported");

            h->ei_data = reader->ReadByte();
            if (h->ei_data != 2)
                writeline("Only big-endian ELF files are supported");

            h->ei_version = reader->ReadByte();
            if (h->ei_version != 1)
                writeline("Only ELF version 1 is supported [a]");

            h->ei_osabi = reader->ReadByte();
            h->ei_abiversion = reader->ReadByte();

            // reader->BaseStream.Seek(7, SeekOrigin.Current);
            reader->position += 7;

            h->e_type = reader->ReadBigUInt16();
            h->e_machine = reader->ReadBigUInt16();
            h->e_version = reader->ReadBigUInt32();
            if (h->e_version != 1)
                writeline("Only ELF version 1 is supported [b]");

            h->e_entry = reader->ReadBigUInt32();
            h->e_phoff = reader->ReadBigUInt32();
            h->e_shoff = reader->ReadBigUInt32();
            h->e_flags = reader->ReadBigUInt32();
            h->e_ehsize = reader->ReadBigUInt16();
            h->e_phentsize = reader->ReadBigUInt16();
            h->e_phnum = reader->ReadBigUInt16();
            h->e_shentsize = reader->ReadBigUInt16();
            h->e_shnum = reader->ReadBigUInt16();
            h->e_shstrndx = reader->ReadBigUInt16();

            return h;
        }
    };

    class ElfSection
    {
    public:
        enum Type : uint
        {
            SHT_NULL = 0,
            SHT_PROGBITS,
            SHT_SYMTAB,
            SHT_STRTAB,
            SHT_RELA,
            SHT_HASH,
            SHT_DYNAMIC,
            SHT_NOTE,
            SHT_NOBITS,
            SHT_REL,
            SHT_SHLIB,
            SHT_DYNSYM,
            SHT_LOPROC = 0x70000000,
            SHT_HIPROC = 0x7fffffff,
            SHT_LOUSER = 0x80000000,
            SHT_HIUSER = 0xffffffff
        };

        enum Flags : uint
        {
            SHF_WRITE = 1,
            SHF_ALLOC = 2,
            SHF_EXECINSTR = 4,
            SHF_MASKPROC = 0xf0000000
        };

        ElfSection(){};

        uint sh_name;

        Type sh_type;
        Flags sh_flags;

        uint sh_addr, sh_size;
        uint sh_link, sh_info, sh_addralign, sh_entsize;

        std::string name;

        sized_array *data;

        ~ElfSection()
        {
            delete data;
        }

        static ElfSection *Read(BinaryReader *reader)
        {
            ElfSection *s = new ElfSection();

            s->sh_name = reader->ReadBigUInt32();
            s->sh_type = (Type)reader->ReadBigUInt32();
            s->sh_flags = (Flags)reader->ReadBigUInt32();
            s->sh_addr = reader->ReadBigUInt32();
            uint sh_offset = reader->ReadBigUInt32();
            s->sh_size = reader->ReadBigUInt32();
            s->sh_link = reader->ReadBigUInt32();
            s->sh_info = reader->ReadBigUInt32();
            s->sh_addralign = reader->ReadBigUInt32();
            s->sh_entsize = reader->ReadBigUInt32();

            if (s->sh_type != Type::SHT_NULL && s->sh_type != Type::SHT_NOBITS)
            {
                long savePos = reader->position;
                reader->position = sh_offset;

                s->data = new sized_array(reader->data + reader->position, s->sh_size);

                reader->position = savePos;
            }

            return s;
        }
    };

    typedef enum
    {
        STB_LOCAL = 0,
        STB_GLOBAL = 1,
        STB_WEAK = 2,
        STB_LOPROC = 13,
        STB_HIPROC = 15
    } SymBind;
    enum SymType
    {
        STT_NOTYPE = 0,
        STT_OBJECT,
        STT_FUNC,
        STT_SECTION,
        STT_FILE,
        STT_LOPROC = 13,
        STT_HIPROC = 15
    };

    enum Reloc
    {
        R_PPC_ADDR32 = 1,
        R_PPC_ADDR16_LO = 4,
        R_PPC_ADDR16_HI = 5,
        R_PPC_ADDR16_HA = 6,
        R_PPC_REL24 = 10
    };

    ElfHeader *_header;

    std::vector<ElfSection *> _sections;

    Elf(unsigned char *input)
    {
        BinaryReader *reader = new BinaryReader(input);

        _header = ElfHeader::Read(reader);

        if (_header->e_type != 1)
            writeline("Only relocatable objects are supported");
        if (_header->e_machine != 0x14)
            writeline("Only PowerPC is supported");
            
        input += _header->e_shoff;
        for (int i = 0; i < _header->e_shnum; i++)
        {
            _sections.push_back(ElfSection::Read(reader));
        }

        if (_header->e_shstrndx > 0 && _header->e_shstrndx < _sections.size())
        {
            ElfSection *section = _sections[_header->e_shstrndx];
            sized_array *table = section->data;

            for (int i = 0; i < _sections.size(); i++)
            {
                _sections[i]->name = Util::ExtractNullTerminatedString(table->data, table->length, (int)_sections[i]->sh_name);
            }
        }
    }
};