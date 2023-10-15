#include "common.hpp"
#include "linker.hpp"

#include "commands/command.hpp"
#include "hooks/hook.hpp"

#include "commands/write_command.hpp"
#include "commands/reloc_command.hpp"

sized_array *KamekFile::PackFrom(Linker *linker)
{
    KamekFile *kf = new KamekFile();
    kf->LoadFromLinker(linker);
    return kf->Pack();
}

ushort KamekFile::ReadUInt16(Word addr)
{
    return Util::ExtractUInt16(_codeBlob->data, addr - _baseAddress);
}
uint KamekFile::ReadUInt32(Word addr)
{
    return Util::ExtractUInt32(_codeBlob->data, addr - _baseAddress);
}
void KamekFile::WriteUInt16(Word addr, ushort value)
{
    Util::InjectUInt16(_codeBlob->data, addr - _baseAddress, value);
}
void KamekFile::WriteUInt32(Word addr, uint value)
{
    Util::InjectUInt32(_codeBlob->data, addr - _baseAddress, value);
}

bool KamekFile::Contains(Word addr)
{
    return (addr >= _baseAddress && addr < (_baseAddress + _codeBlob->length));
}

uint KamekFile::QuerySymbolSize(Word addr)
{
    return _symbolSizes[addr];
}

void KamekFile::LoadFromLinker(Linker *linker)
{
    if (_codeBlob != nullptr)
        writeline("this KamekFile already has stuff : it");

    _mapper = linker->Mapper;

    // Extract _just_ the code/data sections
    _codeBlob = new sized_array(linker->_outputEnd - linker->_outputStart);

    memcpy(_codeBlob, linker->_memory + (linker->_outputStart - linker->_baseAddress), _codeBlob->length);

    _baseAddress = linker->_baseAddress;
    _bssSize = linker->_bssEnd - linker->_bssStart;
    _ctorStart = linker->_ctorStart - linker->_outputStart;
    _ctorEnd = linker->_ctorEnd - linker->_outputStart;

    for (auto pair : linker->_symbolSizes)
        _symbolSizes[pair.first] = pair.second;

    AddRelocsAsCommands(linker->_fixups);

    for (auto cmd : linker->_hooks)
        ApplyHook(cmd);
    ApplyStaticCommands();
}

void KamekFile::AddRelocsAsCommands(std::vector<Linker::Fixup *> relocs)
{
    for (auto rel : relocs)
    {
        if (_commands.contains(rel->source))
            writeline("duplicate commands for address {0}", rel->source);
        Command *cmd = new RelocCommand(rel->source, rel->dest, rel->type);
        cmd->CalculateAddress(this);
        cmd->AssertAddressNonNull();
        _commands[rel->source] = cmd;
    }
}

void KamekFile::ApplyHook(Linker::HookData hookData)
{
    auto hook = Hook::Create(hookData, _mapper);
    for (auto cmd : hook->Commands)
    {
        cmd->CalculateAddress(this);
        cmd->AssertAddressNonNull();
        if (_commands.contains(cmd->_Address))
            writeline("duplicate commands for address {0}", cmd->_Address);
        _commands[cmd->_Address] = cmd;
    }
    _hooks.push_back(hook);
}

void KamekFile::ApplyStaticCommands()
{
    // leave _commands containing just the ones we couldn't apply here
    std::map<Word, Command *> original = _commands;

    for (std::pair<const Word, Command *> cmd : original)
    {
        Command *value = cmd.second;
        if (!value->Apply(this))
        {
            _commands[value->_Address] = value;
        }
    }
}

sized_array *KamekFile::Pack()
{
    sized_array *ms = new sized_array(32 * 1024 * 1024);
    BinaryWriter *bw = new BinaryWriter(ms->data);

    bw->WriteBE((uint)0x4B616D65); // 'Kamek\0\0\2'
    bw->WriteBE((uint)0x6B000002);
    bw->WriteBE((uint)_bssSize);
    bw->WriteBE((uint)_codeBlob->length);
    bw->WriteBE((uint)_ctorStart);
    bw->WriteBE((uint)_ctorEnd);
    bw->WriteBE((uint)0);
    bw->WriteBE((uint)0);

    bw->Write(_codeBlob);

    for (auto pair : _commands)
    {
        pair.second->AssertAddressNonNull();
        uint cmdID = (uint)pair.second->Id << 24;
        if (pair.second->_Address.IsRelative())
        {
            if (pair.second->_Address.Value > 0xFFFFFF)
                writeline("Address too high for packed command");

            cmdID |= pair.second->_Address.Value;
            bw->WriteBE(cmdID);
        }
        else
        {
            cmdID |= 0xFFFFFE;
            bw->WriteBE(cmdID);
            bw->WriteBE(pair.second->_Address.Value);
        }
        pair.second->WriteArguments(bw);
    }

    return ms;
}

std::string join(const std::vector<std::string> &sequence, const std::string &separator)
{
    std::string result;
    for (size_t i = 0; i < sequence.size(); ++i)
        result += sequence[i] + ((i != sequence.size() - 1) ? separator : "");
    return result;
}

std::string KamekFile::PackRiivolution()
{
    if (_baseAddress.Type == WordType::RelativeAddr)
        writeline("cannot pack a dynamically linked binary as a Riivolution patch");

    std::vector<std::string> elements;

    if (_codeBlob->length > 0)
    {
        // add the big patch
        // (todo: valuefile support)
        std::string sb = "";
        for (int i = 0; i < _codeBlob->length; i++)
            sb += std::format("{0:2X}", _codeBlob->data[i]);

        elements.push_back(std::format("<memory offset='0x{0:8X}' value='{1}' />", _baseAddress.Value, sb));
    }

    // add individual patches
    for (auto pair : _commands)
        elements.push_back(pair.second->PackForRiivolution());

    return join(elements, "\n");
}

std::string KamekFile::PackDolphin()
{
    if (_baseAddress.Type == WordType::RelativeAddr)
        writeline("cannot pack a dynamically linked binary as a Dolphin patch");

    std::vector<std::string> elements;

    // add the big patch
    int i = 0;
    while (i < _codeBlob->length)
    {
        std::string sb = "";
        sb += std::format("0x{0:8X}:", _baseAddress.Value + i);

        int lineLength;
        switch (_codeBlob->length - i)
        {
        case 1:
            lineLength = 1;
            sb += ("byte:0x000000");
            break;
        case 2:
        case 3:
            lineLength = 2;
            sb += ("word:0x0000");
            break;
        default:
            lineLength = 4;
            sb += ("dword:0x");
            break;
        }

        for (int j = 0; j < lineLength; j++, i++)
            sb+=std::format("{0:2X}", _codeBlob->data[i]);

        elements.push_back(sb);
    }

    // add individual patches
    for (auto pair : _commands)
        elements.push_back(pair.second->PackForDolphin());

    return join(elements, "\n");
}

std::string KamekFile::PackGeckoCodes()
{
    if (_baseAddress.Type == WordType::RelativeAddr)
        writeline("cannot pack a dynamically linked binary as a Gecko code");

    std::vector<ulong> codes;

    if (_codeBlob->length > 0)
    {
        // add the big patch
        long paddingSize = 0;
        if ((_codeBlob->length % 8) != 0)
            paddingSize = 8 - (_codeBlob->length % 8);

        ulong header = 0x06000000ULL << 32;
        header |= (ulong)(_baseAddress.Value & 0x1FFFFFF) << 32;
        header |= (ulong)(_codeBlob->length + paddingSize) & 0xFFFFFFFF;
        codes.push_back(header);

        for (int i = 0; i < _codeBlob->length; i += 8)
        {
            ulong bits = 0;
            if (i < _codeBlob->length)
                bits |= (ulong)_codeBlob->data[i] << 56;
            if ((i + 1) < _codeBlob->length)
                bits |= (ulong)_codeBlob->data[i + 1] << 48;
            if ((i + 2) < _codeBlob->length)
                bits |= (ulong)_codeBlob->data[i + 2] << 40;
            if ((i + 3) < _codeBlob->length)
                bits |= (ulong)_codeBlob->data[i + 3] << 32;
            if ((i + 4) < _codeBlob->length)
                bits |= (ulong)_codeBlob->data[i + 4] << 24;
            if ((i + 5) < _codeBlob->length)
                bits |= (ulong)_codeBlob->data[i + 5] << 16;
            if ((i + 6) < _codeBlob->length)
                bits |= (ulong)_codeBlob->data[i + 6] << 8;
            if ((i + 7) < _codeBlob->length)
                bits |= (ulong)_codeBlob->data[i + 7];
            codes.push_back(bits);
        }
    }

    // // add individual patches
    // for (auto pair : _commands)
    //     codes.AddRange(pair.second->PackGeckoCodes());

    // // convert everything
    // auto elements = new string[codes.Count];
    // for (int i = 0; i < codes.Count; i++)
    //     elements[i] = string.Format("{0:X8} {1:X8}", codes[i] >> 32, codes[i] & 0xFFFFFFFF);

    // return string.Join("\n", elements);

    
    // add individual patches
    for (auto pair : _commands)
        for (auto code : pair.second->PackGeckoCodes())
        {
            codes.push_back(code);
        }

    // convert everything
    std::vector<std::string> elements;
    for (int i = 0; i < codes.size(); i++)
        elements.push_back(std::format("{0:8X} {1:8X}", codes[i] >> 32, codes[i] & 0xFFFFFFFF));

    return join(elements, "\n");
}

std::string KamekFile::PackActionReplayCodes()
{
    if (_baseAddress.Type == WordType::RelativeAddr)
        writeline("cannot pack a dynamically linked binary as an Action Replay code");

    std::vector<ulong> codes;

    // add the big patch
    for (int i = 0; i < _codeBlob->length; i += 4)
    {
        ulong bits = 0x04000000ULL << 32;
        bits |= (ulong)((_baseAddress.Value + i) & 0x1FFFFFF) << 32;
        if (i < _codeBlob->length)
            bits |= (ulong)_codeBlob->data[i] << 24;
        if ((i + 1) < _codeBlob->length)
            bits |= (ulong)_codeBlob->data[i + 1] << 16;
        if ((i + 2) < _codeBlob->length)
            bits |= (ulong)_codeBlob->data[i + 2] << 8;
        if ((i + 3) < _codeBlob->length)
            bits |= (ulong)_codeBlob->data[i + 3];
        codes.push_back(bits);
    }

    // add individual patches
    for (auto pair : _commands)
        // codes.AddRange(pair.second->PackActionReplayCodes());
        for (auto code : pair.second->PackActionReplayCodes())
        {
            codes.push_back(code);
        }

    // convert everything
    std::vector<std::string> elements;
    for (int i = 0; i < codes.size(); i++)
        elements.push_back(std::format("{0:8X} {1:8X}", codes[i] >> 32, codes[i] & 0xFFFFFFFF));

    return join(elements, "\n");
}

void KamekFile::InjectIntoDol(Dol *dol)
{
    if (_baseAddress.Type == WordType::RelativeAddr)
        writeline("cannot pack a dynamically linked binary into a DOL");

    if (_codeBlob->length > 0)
    {
        // find an empty text section
        int victimSection = -1;
        for (int i = 17; i >= 0; i--)
        {
            if (dol->Sections[i].Data->length == 0)
            {
                victimSection = i;
                break;
            }
        }

        if (victimSection == -1)
            writeline("cannot find an empty text section : the DOL");

        // throw the code blob into it
        dol->Sections[victimSection].LoadAddress = _baseAddress.Value;
        dol->Sections[victimSection].Data = _codeBlob;
    }

    // apply all patches
    for (auto pair : _commands)
        pair.second->ApplyToDol(dol);
}
