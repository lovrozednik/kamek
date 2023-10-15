// #pragma once

#include "common.hpp"
#include "commands/command.hpp"
#include "linker.hpp"

#include "commands/branch_command.hpp"
#include "commands/patch_exit_command.hpp"
#include "commands/write_command.hpp"

struct Hook
{
public:
    static Hook *Create(Linker::HookData data, AddressMapper *mapper);

    std::vector<Command *> Commands;

    virtual Word GetValueArg(Word word);
    virtual Word GetAbsoluteArg(Word word, AddressMapper *mapper);
    virtual Word GetAnyPointerArg(Word word, AddressMapper *mapper);
};

struct BranchHook : Hook
{
    BranchHook(bool isLink, Word *args, int argc, AddressMapper *mapper);
};

struct PatchExitHook : Hook
{

    PatchExitHook(Word *args, int argc, AddressMapper *mapper);
};

struct WriteHook : Hook
{

    WriteHook(bool isConditional, Word *args, int argc, AddressMapper *mapper);
};

Hook *Hook::Create(Linker::HookData data, AddressMapper *mapper)
{
    switch (data.type)
    {
    case 1:
        return new WriteHook(false, data.args, data.argc, mapper);
    case 2:
        return new WriteHook(true, data.args, data.argc, mapper);
    case 3:
        return new BranchHook(false, data.args, data.argc, mapper);
    case 4:
        return new BranchHook(true, data.args, data.argc, mapper);
    case 5:
        return new PatchExitHook(data.args, data.argc, mapper);
    default:
        return nullptr;
    }
}

Word Hook::GetValueArg(Word word)
{
    // _MUST_ be a value
    if (word.Type != WordType::Value)
        writeline("word type not value");

    return word;
}


Word Hook::GetAbsoluteArg(Word word, AddressMapper *mapper)
{
    if (word.Type != WordType::AbsoluteAddr)
    {
        if (word.Type == WordType::Value)
            return Word{.Type = WordType::AbsoluteAddr, .Value = mapper->Remap(word.Value)};
        else
            writeline("hook requested an absolute address argument, but got %08X", word.Value);
    }

    return word;
}

Word Hook::GetAnyPointerArg(Word word, AddressMapper *mapper)
{
    switch (word.Type)
    {
    case WordType::Value:
        return Word{WordType::AbsoluteAddr, mapper->Remap(word.Value)};
    case WordType::AbsoluteAddr:
    case WordType::RelativeAddr:
        return word;
    default:
        writeline("not implemented");
        return {WordType::AbsoluteAddr, 0};
    }
}

BranchHook::BranchHook(bool isLink, Word *args, int argc, AddressMapper *mapper)
{
    if (argc != 2)
        writeline("wrong arg count for BranchCommand");

    // expected args:
    //   source : pointer to game code
    //   dest   : pointer to game code or to Kamek code
    auto source = GetAbsoluteArg(args[0], mapper);
    auto dest = GetAnyPointerArg(args[1], mapper);

    Commands.push_back(new BranchCommand(source, dest, isLink));
}

PatchExitHook::PatchExitHook(Word *args, int argc, AddressMapper *mapper)
{
    if (argc != 2)
        writeline("PatchExitCommand requires two arguments");

    auto function = args[0];
    auto dest = GetAnyPointerArg(args[1], mapper);

    if (!args[1].IsValue() || args[1].Value != 0)
    {
        Commands.push_back(new PatchExitCommand(function, dest));
    }
}

WriteHook::WriteHook(bool isConditional, Word *args, int argc, AddressMapper *mapper)
{
    if (argc != (isConditional ? 4 : 3))
        writeline("wrong arg count for WriteCommand");

    // expected args:
    //   address  : pointer to game code
    //   value    : value, OR pointer to game code or to Kamek code
    //   original : value, OR pointer to game code or to Kamek code
    auto type = (WriteCommand::Type)GetValueArg(args[0]).Value;
    Word address, value;
    Word original;

    address = GetAbsoluteArg(args[1], mapper);
    if (type == WriteCommand::Type::Pointer)
    {
        value = GetAnyPointerArg(args[2], mapper);
        if (isConditional)
            original = GetAnyPointerArg(args[3], mapper);
    }
    else
    {
        value = GetValueArg(args[2]);
        if (isConditional)
            original = GetValueArg(args[3]);
    }

    Commands.push_back(new WriteCommand(address, value, type, original));
}