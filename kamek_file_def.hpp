#pragma once

#include "common.hpp"
#include "word.hpp"
#include "linker.hpp"
#include "dol.hpp"

class Command;
class Hook;
class AddressMapper;

class KamekFile
{
public:
    static sized_array *PackFrom(Linker *linker);

    Word _baseAddress;
    sized_array *_codeBlob;
    long _bssSize;
    long _ctorStart;
    long _ctorEnd;

    ushort ReadUInt16(Word addr);
    uint ReadUInt32(Word addr);
    void WriteUInt16(Word addr, ushort value);
    void WriteUInt32(Word addr, uint value);

    bool Contains(Word addr);

    uint QuerySymbolSize(Word addr);

    std::map<Word, Command *> _commands;
    std::vector<Hook *> _hooks;
    std::map<Word, uint> _symbolSizes;
    AddressMapper *_mapper;

    void LoadFromLinker(Linker *linker);

    void AddRelocsAsCommands(std::vector<Linker::Fixup *> relocs);

    void ApplyHook(Linker::HookData hookData);

    void ApplyStaticCommands();
    sized_array *Pack();

    std::string PackRiivolution();

    std::string PackDolphin();
    std::string PackGeckoCodes();
    std::string PackActionReplayCodes();

    void InjectIntoDol(Dol *dol);

    ~KamekFile()
    {
        delete _codeBlob;
        delete _mapper;
    }
};