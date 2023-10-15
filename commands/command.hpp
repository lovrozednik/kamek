#pragma once

#include "common.hpp"
#include "word.hpp"
#include "dol.hpp"

class Command
{
public:
    enum Ids : byte
    {
        Null = 0,

        // these deliberately match the ELF relocations
        Addr32 = 1,
        Addr16Lo = 4,
        Addr16Hi = 5,
        Addr16Ha = 6,
        Rel24 = 10,

        // these are new
        WritePointer = 1, // same as Addr32 on purpose
        Write32 = 32,
        Write16 = 33,
        Write8 = 34,
        CondWritePointer = 35,
        CondWrite32 = 36,
        CondWrite16 = 37,
        CondWrite8 = 38,

        Branch = 64,
        BranchLink = 65,
    };

    Ids Id;

    Word _Address;

    Command(Ids id, Word address)
    {
        Id = id;
        _Address = address;
    }

    virtual void WriteArguments(BinaryWriter *bw){};
    virtual bool Apply(void *file) { return false; };
    virtual std::string PackForRiivolution() { return ""; };
    virtual std::string PackForDolphin() { return ""; };
    virtual std::vector<ulong> PackGeckoCodes() { return std::vector<ulong>(); };
    virtual std::vector<ulong> PackActionReplayCodes() { return std::vector<ulong>(); };
    virtual void ApplyToDol(Dol *dol){};
    virtual void CalculateAddress(void *f) { _Address = {WordType::AbsoluteAddr, 0}; };

    void AssertAddressNonNull()
    {
        if (_Address.Value == 0)
            writeline("%s command must have an address in this context", (byte)Id);
    }

    ~Command(){};
};
