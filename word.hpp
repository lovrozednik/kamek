#pragma once

#include "common.hpp"

enum WordType
{
    Value = 1,
    AbsoluteAddr = 2,
    RelativeAddr = 3
};

struct Word
{
public:
    WordType Type;
    uint Value;

    Word operator+(long addend)
    {
        return {Type, (uint)(Value + addend)};
    };
    Word operator+=(long addend)
    {
        Value += addend;
        return {Type, (uint)(Value)};
    };
    long operator-(Word b)
    {
        if (Type != b.Type)
            writeline("cannot perform arithmetic on different kinds of words");
        return Value - b.Value;
    }
    bool operator<(Word b)
    {
        if (Type != b.Type)
            writeline("cannot compare different kinds of words");
        return Value < b.Value;
    }
    bool operator<(const Word b) const { return Value < b.Value; };
    bool operator>(Word b)
    {
        if (Type != b.Type)
            writeline("cannot compare different kinds of words");
        return Value > b.Value;
    }
    bool operator<=(Word b)
    {
        if (Type != b.Type)
            writeline("cannot compare different kinds of words");
        return Value <= b.Value;
    }
    bool operator>=(Word b)
    {
        if (Type != b.Type)
            writeline("cannot compare different kinds of words");
        return Value >= b.Value;
    }

    bool IsAbsolute()
    {
        return (Type == WordType::AbsoluteAddr);
    }
    bool IsRelative()
    {
        return (Type == WordType::RelativeAddr);
    }
    bool IsValue()
    {
        return (Type == WordType::Value);
    }

    void AssertAbsolute()
    {
        if (!IsAbsolute())
            writeline("word %s must be an absolute address in this context\n", ToString());
    }
    void AssertNotRelative()
    {
        if (IsRelative())
            writeline("word %s cannot be a relative address in this context", ToString());
    }
    void AssertValue()
    {
        if (!IsValue())
            writeline("word %s must be a value in this context", ToString());
    }
    void AssertNotAmbiguous()
    {
        // Verifies that this Address can be disambiguated between Absolute
        // and Relative from _just_ the top bit
        if (IsAbsolute() && (Value & 0x80000000) == 0)
            writeline("address is ambiguous: absolute, top bit not set");
        if (IsRelative() && (Value & 0x80000000) != 0)
            writeline("address is ambiguous: relative, top bit set");
    }

    std::string ToString()
    {
        switch (Type)
        {
        case WordType::AbsoluteAddr:
            return std::format("<AbsoluteAddr 0x{0:X}>", Value);
        case WordType::RelativeAddr:
            return std::format("<RelativeAddr Base+0x{0:X}>", Value);
        case WordType::Value:
            return std::format("<Word 0x{0:X}>", Value);
        }
        return "null";
    }

    bool HasValue() { return Value != 0; };
};

class WordUtils
{
    void WriteBE(BinaryWriter *bw, Word word)
    {
        bw->Write((byte)word.Type);
        bw->WriteBE((uint)word.Value);
    }
};
