#pragma once

#include "command.hpp"
#include "common.hpp"
#include "elf.hpp"
// #include "kamek_file.hpp"
class WriteCommand : public Command
{
public:
    enum Type
    {
        Pointer = 1,
        Value32 = 2,
        Value16 = 3,
        Value8 = 4
    };

    static Ids
    IdFromType(Type type, bool isConditional)
    {
        if (isConditional)
        {
            switch (type)
            {
            case Type::Pointer:
                return Ids::CondWritePointer;
            case Type::Value32:
                return Ids::CondWrite32;
            case Type::Value16:
                return Ids::CondWrite16;
            case Type::Value8:
                return Ids::CondWrite8;
            default:
                return Ids::Null;
            }
        }
        else
        {
            switch (type)
            {
            case Type::Pointer:
                return Ids::WritePointer;
            case Type::Value32:
                return Ids::Write32;
            case Type::Value16:
                return Ids::Write16;
            case Type::Value8:
                return Ids::Write8;
            default:
                return Ids::Null;
            }
        }
    }

    Type ValueType;
    Word Value;
    Word Original;

    WriteCommand(Word address, Word value, Type valueType, Word original)
        : Command(IdFromType(valueType, original.HasValue()), address)
    {
        Value = value;
        ValueType = valueType;
        Original = original;
    }

    void WriteArguments(BinaryWriter *bw) override
    {
        if (ValueType == Type::Pointer)
            Value.AssertNotAmbiguous();
        else
            Value.AssertValue();

        bw->WriteBE(Value.Value);

        if (Original.HasValue())
        {
            Original.AssertNotRelative();
            bw->WriteBE(Original.Value);
        }
    }

    std::string PackForRiivolution() override
    {
        _Address.AssertAbsolute();
        if (ValueType == Type::Pointer)
            Value.AssertAbsolute();
        else
            Value.AssertValue();

        if (Original.HasValue())
        {
            Original.AssertNotRelative();

            switch (ValueType)
            {
            case Type::Value8:
                return std::format("<memory offset='0x{0:8X}' value='{1:2X}' original='{2:2X}' />", _Address.Value, Value.Value, Original.Value);
            case Type::Value16:
                return std::format("<memory offset='0x{0:8X}' value='{1:4X}' original='{2:4X}' />", _Address.Value, Value.Value, Original.Value);
            case Type::Value32:
            case Type::Pointer:
                return std::format("<memory offset='0x{0:8X}' value='{1:8X}' original='{2:8X}' />", _Address.Value, Value.Value, Original.Value);
            }
        }
        else
        {
            switch (ValueType)
            {
            case Type::Value8:
                return std::format("<memory offset='0x{0:8X}' value='{1:2X}' />", _Address.Value, Value.Value);
            case Type::Value16:
                return std::format("<memory offset='0x{0:8X}' value='{1:4X}' />", _Address.Value, Value.Value);
            case Type::Value32:
            case Type::Pointer:
                return std::format("<memory offset='0x{0:8X}' value='{1:8X}' />", _Address.Value, Value.Value);
            }
        }

        return "";
    }

    std::string PackForDolphin() override
    {
        _Address.AssertAbsolute();
        if (ValueType == Type::Pointer)
            Value.AssertAbsolute();
        else
            Value.AssertValue();

        switch (ValueType)
        {
        case Type::Value8:
            return std::format("0x{0:8X}:byte:0x000000{1:2X}", _Address.Value, Value.Value);
        case Type::Value16:
            return std::format("0x{0:8X}:word:0x0000{1:4X}", _Address.Value, Value.Value);
        case Type::Value32:
        case Type::Pointer:
            return std::format("0x{0:8X}:dword:0x{1:8X}", _Address.Value, Value.Value);
        }

        return "";
    }

    std::vector<ulong> PackGeckoCodes() override
    {
        _Address.AssertAbsolute();
        if (ValueType == Type::Pointer)
            Value.AssertAbsolute();
        else
            Value.AssertValue();

        if (_Address.Value >= 0x90000000)
            writeline("MEM2 writes not yet supported for gecko");

        ulong code = (((ulong)_Address.Value & 0x1FFFFFF) << 32) | Value.Value;
        switch (ValueType)
        {
        case Type::Value16:
            code |= 0x2000000ULL << 32;
            break;
        case Type::Value32:
        case Type::Pointer:
            code |= 0x4000000ULL << 32;
            break;
        }

        if (Original.HasValue())
        {
            if (ValueType == Type::Pointer)
                Original.AssertAbsolute();
            else
                Original.AssertValue();

            if (ValueType == Type::Value8)
            {
                // Gecko doesn't natively support conditional 8-bit writes,
                // so we have to implement it manually with a code embedding PPC...

                uint addrTop = (uint)(_Address.Value >> 16);
                uint addrBtm = (uint)(_Address.Value & 0xFFFF);
                uint orig = (uint)Original.Value;
                uint value = (uint)Value.Value;

                // r0 and r3 empirically *seem* to be available, though there's zero documentation on this
                // r4 is definitely NOT available (codehandler dies if you mess with it)
                uint inst1 = 0x3C600000 | addrTop; // lis r3, X
                uint inst2 = 0x60630000 | addrBtm; // ori r3, r3, X
                uint inst3 = 0x88030000;           // lbz r0, 0(r3)
                uint inst4 = 0X2C000000 | orig;    // cmpwi r0, X
                uint inst5 = 0X4082000C;           // bne @end
                uint inst6 = 0x38000000 | value;   // li r0, X
                uint inst7 = 0x98030000;           // stb r0, 0(r3)
                uint inst8 = 0x4E800020;           // @end: blr

                return std::vector<ulong>({(0xC0000000ULL << 32) | 4, // "4" for four lines of instruction data below
                                           ((ulong)inst1 << 32) | inst2,
                                           ((ulong)inst3 << 32) | inst4,
                                           ((ulong)inst5 << 32) | inst6,
                                           ((ulong)inst7 << 32) | inst8});
            }
            else
            {
                // Sandwich the write between "if" and "endif" codes

                ulong if_start = ((ulong)(_Address.Value & 0x1FFFFFF) << 32) | Original.Value;

                switch (ValueType)
                {
                case Type::Value16:
                    if_start |= 0x28000000ULL << 32;
                    break;
                case Type::Value32:
                case Type::Pointer:
                    if_start |= 0x20000000ULL << 32;
                    break;
                }

                ulong if_end = 0xE2000001ULL << 32;

                return std::vector<ulong>({if_start, code, if_end});
            }
        }
        else
        {
            return std::vector<ulong>({code});
        }
    }

    std::vector<ulong> PackActionReplayCodes() override
    {
        _Address.AssertAbsolute();
        if (ValueType == Type::Pointer)
            Value.AssertAbsolute();
        else
            Value.AssertValue();

        if (_Address.Value >= 0x90000000)
            writeline("MEM2 writes not yet supported for action replay");

        ulong code = ((ulong)(_Address.Value & 0x1FFFFFF) << 32) | Value.Value;
        switch (ValueType)
        {
        case Type::Value16:
            code |= 0x2000000ULL << 32;
            break;
        case Type::Value32:
        case Type::Pointer:
            code |= 0x4000000ULL << 32;
            break;
        }

        if (Original.HasValue())
        {
            if (ValueType == Type::Pointer)
                Original.AssertAbsolute();
            else
                Original.AssertValue();

            ulong if_start = ((ulong)(_Address.Value & 0x1FFFFFF) << 32) | Original.Value;
            switch (ValueType)
            {
            case Type::Value8:
                if_start |= 0x08000000ULL << 32;
                break;
            case Type::Value16:
                if_start |= 0x0A000000ULL << 32;
                break;
            case Type::Value32:
            case Type::Pointer:
                if_start |= 0x0C000000ULL << 32;
                break;
            }

            return std::vector<ulong>({if_start, code});
        }
        else
        {
            return std::vector<ulong>({code});
        }
    }

    void ApplyToDol(Dol *dol) override
    {
        _Address.AssertAbsolute();
        if (ValueType == Type::Pointer)
            Value.AssertAbsolute();
        else
            Value.AssertValue();

        if (Original.HasValue())
        {
            bool patchOK = false;
            switch (ValueType)
            {
            case Type::Value8:
                patchOK = (dol->ReadByte(_Address.Value) == Original.Value);
                break;
            case Type::Value16:
                patchOK = (dol->ReadUInt16(_Address.Value) == Original.Value);
                break;
            case Type::Value32:
            case Type::Pointer:
                patchOK = (dol->ReadUInt32(_Address.Value) == Original.Value);
                break;
            }
            if (!patchOK)
                return;
        }

        switch (ValueType)
        {
        case Type::Value8:
            dol->WriteByte(_Address.Value, (byte)Value.Value);
            break;
        case Type::Value16:
            dol->WriteUInt16(_Address.Value, (ushort)Value.Value);
            break;
        case Type::Value32:
        case Type::Pointer:
            dol->WriteUInt32(_Address.Value, Value.Value);
            break;
        }
    }

    bool Apply(void *file) override
    {
        return false;
    }

    void CalculateAddress(void *f) { _Address = {WordType::AbsoluteAddr, 0}; };
};
