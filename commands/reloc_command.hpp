#include "command.hpp"
#include "common.hpp"
#include "elf.hpp"
#include "kamek_file_def.hpp"

class RelocCommand : public Command
{
public:
    Word Target;

    RelocCommand(Word source, Word target, Elf::Reloc reloc)
        : Command((Ids)reloc, source)
    {
        Target = target;
    }

    void WriteArguments(BinaryWriter *bw) override
    {
        Target.AssertNotAmbiguous();
        bw->WriteBE(Target.Value);
    }

    std::string PackForRiivolution() { return ""; };
    std::string PackForDolphin() { return ""; };
    std::vector<ulong> PackGeckoCodes() { return std::vector<ulong>(); };
    std::vector<ulong> PackActionReplayCodes() { return std::vector<ulong>(); };
    void CalculateAddress(void *f) { _Address = {WordType::AbsoluteAddr, 0}; };

    void ApplyToDol(Dol *dol) override
    {
        _Address.AssertAbsolute();
        Target.AssertAbsolute();

        switch (Id)
        {
        case Ids::Rel24:
        {
            long delta = Target - _Address;
            uint insn = dol->ReadUInt32(_Address.Value) & 0xFC000003;
            insn |= ((uint)delta & 0x3FFFFFC);
            dol->WriteUInt32(_Address.Value, insn);
        }
        break;

        case Ids::Addr32:
            dol->WriteUInt32(_Address.Value, Target.Value);
            break;

        case Ids::Addr16Lo:
            dol->WriteUInt16(_Address.Value, (ushort)(Target.Value & 0xFFFF));
            break;

        case Ids::Addr16Hi:
            dol->WriteUInt16(_Address.Value, (ushort)(Target.Value >> 16));
            break;

        case Ids::Addr16Ha:
        {
            ushort v = (ushort)(Target.Value >> 16);
            if ((Target.Value & 0x8000) == 0x8000)
                v++;
            dol->WriteUInt16(_Address.Value, v);
        }
        break;

        default:
            writeline("unrecognised relocation type");
        }
    }

    bool Apply(void *_f) override
    {
        KamekFile *file = (KamekFile *)_f;
        switch (Id)
        {
        case Ids::Rel24:
            if ((_Address.IsAbsolute() && Target.IsAbsolute()) || (_Address.IsRelative() && Target.IsRelative()))
            {
                long delta = Target - _Address;
                uint insn = file->ReadUInt32(_Address) & 0xFC000003;
                insn |= ((uint)delta & 0x3FFFFFC);
                file->WriteUInt32(_Address, insn);

                return true;
            }
            break;

        case Ids::Addr32:
            if (Target.IsAbsolute())
            {
                file->WriteUInt32(_Address, Target.Value);
                return true;
            }
            break;

        case Ids::Addr16Lo:
            if (Target.IsAbsolute())
            {
                file->WriteUInt16(_Address, (ushort)(Target.Value & 0xFFFF));
                return true;
            }
            break;

        case Ids::Addr16Hi:
            if (Target.IsAbsolute())
            {
                file->WriteUInt16(_Address, (ushort)(Target.Value >> 16));
                return true;
            }
            break;

        case Ids::Addr16Ha:
            if (Target.IsAbsolute())
            {
                ushort v = (ushort)(Target.Value >> 16);
                if ((Target.Value & 0x8000) == 0x8000)
                    v++;
                file->WriteUInt16(_Address, v);
                return true;
            }
            break;

        default:
            writeline("unrecognised relocation type");
        }

        return false;
    }
};