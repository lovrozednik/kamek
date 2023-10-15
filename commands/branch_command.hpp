#include "command.hpp"
#include "common.hpp"
#include "kamek_file_def.hpp"

class BranchCommand : public Command
{
public:
    Word Target;

    BranchCommand(Word source, Word target, bool isLink)
        : Command(isLink ? Ids::BranchLink : Ids::Branch, source)
    {
        Target = target;
    };

    void WriteArguments(BinaryWriter *bw) override
    {
        bw->WriteBE(Target.Value);
    }

    std::string PackForRiivolution() override
    {
        return std::format("<memory offset=\'0x{0:8X}\' value=\'{1:8X}\' />", _Address.Value, GenerateInstruction());
    }

    std::string PackForDolphin() override
    {
        return std::format("0x{0:8X}:dword:0x{1:8X}", _Address.Value, GenerateInstruction());
    }

    std::vector<ulong> PackGeckoCodes() override
    {
        ulong code = ((ulong)(_Address.Value & 0x1FFFFFF) << 32) | GenerateInstruction();
        code |= 0x4000000ULL << 32;

        return std::vector<ulong>({code});
    }

    std::vector<ulong> PackActionReplayCodes() override
    {
        ulong code = ((ulong)(_Address.Value & 0x1FFFFFF) << 32) | GenerateInstruction();
        code |= 0x4000000ULL << 32;

        return std::vector<ulong>({code});
    }

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

    void ApplyToDol(Dol *dol) override
    {
        dol->WriteUInt32(_Address.Value, GenerateInstruction());
    }

    uint GenerateInstruction()
    {
        long delta = Target - _Address;
        uint insn = (Id == Ids::BranchLink) ? 0x48000001U : 0x48000000U;
        insn |= ((uint)delta & 0x3FFFFFC);
        return insn;
    }

    void CalculateAddress(void* f) {_Address = {WordType::AbsoluteAddr, 0};};
};
