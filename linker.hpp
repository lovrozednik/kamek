#pragma once

#include "common.hpp"
#include "address_mapper.hpp"
#include "Elf.hpp"
#include "word.hpp"

class Linker
{
public:
    bool _linked = false;
    std::vector<Elf *> _modules;
    AddressMapper *Mapper;

    inline static std::vector<std::string> FixedUndefinedSymbols = std::vector<std::string>();

    Word _baseAddress;
    Word _ctorStart, _ctorEnd;
    Word _outputStart, _outputEnd;
    Word _bssStart, _bssEnd;
    Word _kamekStart, _kamekEnd;
    byte *_memory = nullptr;

    Linker(AddressMapper *mapper)
    {
        Mapper = mapper;
    }

    void AddModule(Elf *elf)
    {
        if (_linked)
            writeline("This linker has already been linked");
        if (std::find(_modules.begin(), _modules.end(), elf) != _modules.end())
            writeline("This module is already part of this linker");

        _modules.push_back(elf);
    }

    void DoLink(std::map<std::string, uint> externalSymbols)
    {
        if (_linked)
            writeline("This linker has already been linked");
        _linked = true;

        // _externalSymbols = new std::map<std::string, uint>();
        for (auto pair : externalSymbols)
            _externalSymbols[pair.first] = Mapper->Remap(pair.second);

        CollectSections();
        BuildSymbolTables();
        ProcessRelocations();
        ProcessHooks();
    }

    void LinkStatic(uint baseAddress, std::map<std::string, uint> externalSymbols)
    {
        _baseAddress = {WordType::AbsoluteAddr, Mapper->Remap(baseAddress)};
        DoLink(externalSymbols);
    }
    void LinkDynamic(std::map<std::string, uint> externalSymbols)
    {
        _baseAddress = {WordType::RelativeAddr, 0};
        DoLink(externalSymbols);
    }

    std::vector<sized_array *> _binaryBlobs;
    std::map<Elf::ElfSection *, Word> _sectionBases;

    Word _location;

    void ImportSections(std::string prefix)
    {
        for (Elf *elf : _modules)
        {
            // for (auto s :(from s : elf->_sections
            //                       where s.name.starts_with(prefix)
            //                           select s))

            for (Elf::ElfSection *s : elf->_sections)
            {
                if (!s->name.starts_with(prefix))
                    continue;

                if (s->data != nullptr)
                    _binaryBlobs.push_back(s->data);
                else
                    _binaryBlobs.push_back(new sized_array(s->sh_size));

                _sectionBases[s] = _location;
                _location += s->sh_size;

                // Align to 4 bytes
                if ((_location.Value % 4) != 0)
                {
                    long alignment = 4 - (_location.Value % 4);
                    _binaryBlobs.push_back(new sized_array(alignment));
                    _location += alignment;
                }
            }
        }
    }

    void CollectSections()
    {
        _location = _baseAddress;

        _outputStart = _location;
        ImportSections(".init");
        ImportSections(".fini");
        ImportSections(".text");
        _ctorStart = _location;
        ImportSections(".ctors");
        _ctorEnd = _location;
        ImportSections(".dtors");
        ImportSections(".rodata");
        ImportSections(".data");
        _outputEnd = _location;

        // TODO: maybe should align to 0x20 here?
        _bssStart = _location;
        ImportSections(".bss");
        _bssEnd = _location;

        _kamekStart = _location;
        ImportSections(".kamek");
        _kamekEnd = _location;

        // Create one big blob from this
        _memory = new byte[_location - _baseAddress];
        int position = 0;
        for (sized_array *blob : _binaryBlobs)
        {
            memcpy(_memory, blob + position, blob->length);
            position += blob->length;
        }
    }
    ushort ReadUInt16(Word addr)
    {
        return Util::ExtractUInt16(_memory, addr - _baseAddress);
    }
    uint ReadUInt32(Word addr)
    {
        return Util::ExtractUInt32(_memory, addr - _baseAddress);
    }
    void WriteUInt16(Word addr, ushort value)
    {
        Util::InjectUInt16(_memory, addr - _baseAddress, value);
    }
    void WriteUInt32(Word addr, uint value)
    {
        Util::InjectUInt32(_memory, addr - _baseAddress, value);
    }

    struct Symbol
    {
        Word address;
        uint size;
        bool isWeak;
    };
    struct SymbolName
    {
        std::string name;
        ushort shndx;
    };
    std::map<std::string, Symbol> _globalSymbols;
    std::map<Elf *, std::map<std::string, Symbol>> _localSymbols;
    std::map<Elf::ElfSection *, sized_array *> _symbolTableContents; // sized array of type SymbolName[]
    std::map<std::string, uint> _externalSymbols;
    std::map<Word, uint> _symbolSizes;

    void BuildSymbolTables()
    {
        _globalSymbols["__ctor_loc"] = Symbol{.address = _ctorStart};
        _globalSymbols["__ctor_end"] = Symbol{.address = _ctorEnd};

        for (Elf *elf : _modules)
        {
            std::map<std::string, Symbol> locals;
            _localSymbols[elf] = locals;

            for (Elf::ElfSection *s : elf->_sections)

            {
                if (s->sh_type != Elf::ElfSection::Type::SHT_SYMTAB)
                    continue;

                uint strTabIdx = s->sh_link;
                if (strTabIdx <= 0 || strTabIdx >= elf->_sections.size())
                    writeline("Symbol table is not linked to a std::string table");

                auto strtab = elf->_sections[(int)strTabIdx];

                _symbolTableContents[s] = ParseSymbolTable(elf, s, strtab, locals);
            }
        }
    }

    sized_array *ParseSymbolTable(Elf *elf, Elf::ElfSection *symtab, Elf::ElfSection *strtab, std::map<std::string, Symbol> locals)
    {
        if (symtab->sh_entsize != 16)
            writeline("Invalid symbol table format (sh_entsize != 16)");
        if (strtab->sh_type != Elf::ElfSection::Type::SHT_STRTAB)
            writeline("std::string table does not have type SHT_STRTAB");

        std::vector<SymbolName> symbolNames;
        auto reader = new BinaryReader(symtab->data->data);
        int count = symtab->data->length / 16;

        // always ignore the first symbol
        symbolNames.push_back({});
        reader->position = 16;

        for (int i = 1; i < count; i++)
        {
            // Read info from the ELF
            uint st_name = reader->ReadBigUInt32();
            uint st_value = reader->ReadBigUInt32();
            uint st_size = reader->ReadBigUInt32();
            byte st_info = reader->ReadByte();
            byte st_other = reader->ReadByte();
            ushort st_shndx = reader->ReadBigUInt16();

            uint bind = st_info >> 4;
            uint type = st_info & 0xF;

            std::string name = Util::ExtractNullTerminatedString(strtab->data->data, (int)st_name, 0);

            symbolNames.push_back(SymbolName{.name = name, .shndx = st_shndx});
            if (name.length() == 0 || st_shndx == 0)
                continue;

            Word addr;
            if (st_shndx == 0xFFF1)
            {
                // Absolute symbol
                addr = {WordType::AbsoluteAddr, st_value};
            }
            else if (st_shndx < 0xFF00)
            {
                // Part of a section
                auto section = elf->_sections[st_shndx];
                if (!_sectionBases.contains(section))
                    continue; // skips past symbols we don't care about, like DWARF junk
                addr = _sectionBases[section] + st_value;
            }
            else
                writeline("unknown section index found : symbol table");

            switch (bind)
            {
            case Elf::SymBind::STB_LOCAL:
                if (locals.contains(name))
                    writeline("redefinition of local symbol %s\n", name);
                locals[name] = Symbol{.address = addr, .size = st_size};
                _symbolSizes[addr] = st_size;
                break;

            case Elf::SymBind::STB_GLOBAL:
                if (_globalSymbols.contains(name) && !_globalSymbols[name].isWeak)
                {
                    writeline("redefinition of global symbol %s\n", name);
                }
                _globalSymbols[name] = Symbol{.address = addr, .size = st_size};
                _symbolSizes[addr] = st_size;
                break;

            case Elf::SymBind::STB_WEAK:
                if (!_globalSymbols.contains(name))
                {
                    _globalSymbols[name] = Symbol{.address = addr, .size = st_size, .isWeak = true};
                    _symbolSizes[addr] = st_size;
                }
                break;
            }
        }
        return new sized_array((byte *)symbolNames.data(), symbolNames.size() * sizeof(SymbolName));
    };

    Symbol ResolveSymbol(Elf *elf, std::string name)
    {
        auto locals = _localSymbols[elf];

        std::string name_wo_end = name;

        for (std::string item : FixedUndefinedSymbols)
        {
            name_wo_end = std::regex_replace(name_wo_end, std::regex(item), "");
        }

        if (locals.contains(name))
        {
            return locals[name];
        }

        if (_globalSymbols.contains(name))
        {
            return _globalSymbols[name];
        }

        if (_externalSymbols.contains(name_wo_end))
        {
            return Symbol{.address = {WordType::AbsoluteAddr, _externalSymbols[name_wo_end]}};
        }

        if (name.starts_with("__kAutoMap_"))
        {
            auto addr = name.substr(11);
            if (addr.starts_with("0x") || addr.starts_with("0X"))
                addr = addr.substr(2);
            auto parsedAddr = std::stoi(addr, 0, 16);
            auto mappedAddr = Mapper->Remap(parsedAddr);
            return Symbol{.address = {WordType::AbsoluteAddr, mappedAddr}};
        }

        writeline("undefined symbol %s\n", name);
        return Symbol {WordType::Value, 0, 0};
    }
    struct Fixup
    {
        Elf::Reloc type;
        Word source, dest;
    };
    std::vector<Fixup *> _fixups;

    void ProcessRelocations()
    {
        for (Elf *elf : _modules)
        {
            for (auto s : elf->_sections)
            {
                if (s->sh_type != Elf::ElfSection::Type::SHT_REL)
                    continue;
                writeline("OH SHIT");
            }

            for (auto s : elf->_sections)
            {
                if (s->sh_type != Elf::ElfSection::Type::SHT_RELA)
                    continue;
                // Get the two affected sections
                if (s->sh_info <= 0 || s->sh_info >= elf->_sections.size())
                    writeline("Rela table is not linked to a section");
                if (s->sh_link <= 0 || s->sh_link >= elf->_sections.size())
                    writeline("Rela table is not linked to a symbol table");

                auto affected = elf->_sections[(int)s->sh_info];
                auto symtab = elf->_sections[(int)s->sh_link];

                ProcessRelaSection(elf, s, affected, symtab);
            }
        }
    }

    void ProcessRelaSection(Elf *elf, Elf::ElfSection *relocs, Elf::ElfSection *section, Elf::ElfSection *symtab)
    {
        if (relocs->sh_entsize != 12)
            writeline("Invalid relocs format (sh_entsize != 12)");
        if (symtab->sh_type != Elf::ElfSection::Type::SHT_SYMTAB)
            writeline("Symbol table does not have type SHT_SYMTAB");

        auto reader = new BinaryReader(relocs->data->data);
        int count = relocs->data->length / 12;

        for (int i = 0; i < count; i++)
        {
            uint r_offset = reader->ReadBigUInt32();
            uint r_info = reader->ReadBigUInt32();
            int r_addend = reader->ReadBigInt32();

            Elf::Reloc reloc = (Elf::Reloc)(r_info & 0xFF);
            int symIndex = (int)(r_info >> 8);

            if (symIndex == 0)
                writeline("linking to undefined symbol");
            if (!_sectionBases.contains(section))
                continue; // we don't care about this

            SymbolName symbol = ((SymbolName *)_symbolTableContents[symtab]->data)[symIndex];
            std::string symName = symbol.name;
            // Console.WriteLine("{0,-30} {1}", symName, reloc);

            Word source = _sectionBases[section] + r_offset;
            Word dest = (symName == "" ? _sectionBases[elf->_sections[symbol.shndx]] : ResolveSymbol(elf, symName).address) + r_addend;

            // Console.WriteLine("Linking from 0x{0:X8} to 0x{1:X8}", source.Value, dest.Value);

            if (!KamekUseReloc(reloc, source, dest))
                _fixups.push_back(new Fixup{.type = reloc, .source = source, .dest = dest});
        }
    }
    std::map<Word, Word> _kamekRelocations;

    bool KamekUseReloc(Elf::Reloc type, Word source, Word dest)
    {
        if (source < _kamekStart || source >= _kamekEnd)
            return false;
        if (type != Elf::Reloc::R_PPC_ADDR32)
            writeline("Unsupported relocation type : the Kamek hook data section");

        _kamekRelocations[source] = dest;
        return true;
    }

    struct HookData
    {
        uint type;
        Word *args;
        uint argc;
    };

    std::vector<HookData> _hooks;

    void ProcessHooks()
    {
        for (auto elf : _modules)
        {
            for (auto pair : _localSymbols[elf])
            {
                if (pair.first.starts_with("_kHook"))
                {
                    auto cmdAddr = pair.second.address;

                    auto argCount = ReadUInt32(cmdAddr);
                    auto type = ReadUInt32(cmdAddr + 4);
                    auto args = new Word[argCount];

                    for (int i = 0; i < argCount; i++)
                    {
                        auto argAddr = cmdAddr + (8 + (i * 4));
                        if (_kamekRelocations.contains(argAddr))
                            args[i] = _kamekRelocations[argAddr];
                        else
                            args[i] = {WordType::Value, ReadUInt32(argAddr)};
                    }

                    _hooks.push_back(HookData{.type = type, .args = args, .argc = argCount});
                }
            }
        }
    }
};