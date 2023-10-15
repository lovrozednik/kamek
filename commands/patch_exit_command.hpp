#include "common.hpp"
#include "command.hpp"
#include "kamek_file_def.hpp"

class PatchExitCommand : public Command
{
public:
    Word FunctionStart;
    Word Target;

    PatchExitCommand(Word functionStart, Word target)
        : Command(Ids::Branch, Word{WordType::Value, 0})
    {
        FunctionStart = functionStart;
        Target = target;
    }

    void WriteArguments(BinaryWriter *bw) override
    {
        Target.AssertNotAmbiguous();
        bw->WriteBE(Target.Value);
    }

    void CalculateAddress(void *_f) override
    {
        KamekFile *file = (KamekFile *)_f;
        // Do some reasonableness checks.
        // For now, we'll only work on functions ending in a blr
        auto functionSize = file->QuerySymbolSize(FunctionStart);
        if (functionSize < 4)
        {
            writeline("Function too small!");
        }
        auto functionEnd = FunctionStart + (functionSize - 4);
        if (file->ReadUInt32(functionEnd) != 0x4E800020)
        {
            writeline("Function does not end in blr");
        }

        // Just to be extra sure, are there any other returns in this function?
        for (auto check = FunctionStart; check < functionEnd; check += 4)
        {
            auto insn = file->ReadUInt32(check);
            if ((insn & 0xFC00FFFF) == 0x4C000020)
            {
                writeline("Function contains a return partway through");
            }
        }

        _Address = functionEnd;
    }

    std::string PackForRiivolution() { return ""; };
    std::string PackForDolphin() { return ""; };
    std::vector<ulong> PackGeckoCodes() { return std::vector<ulong>(); };
    std::vector<ulong> PackActionReplayCodes() { return std::vector<ulong>(); };
    void ApplyToDol(Dol *dol) {};

    bool Apply(void *_f) override
    {
        KamekFile *file = (KamekFile *)_f;
        if (_Address.IsAbsolute() && Target.IsAbsolute() && file->Contains(_Address))
        {
            file->WriteUInt32(_Address, GenerateInstruction());
            return true;
        }

        return false;
    }

    uint GenerateInstruction()
    {
        long delta = Target - _Address;
        uint insn = (Id == Ids::BranchLink) ? 0x48000001U : 0x48000000U;
        insn |= ((uint)delta & 0x3FFFFFC);
        return insn;
    }
};
