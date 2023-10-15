#pragma once

#include "common.hpp"

class AddressMapper
{
public:
    AddressMapper *Base = nullptr;

    ~AddressMapper()
    {
        if (Base) delete Base;
    }

    class Mapping
    {
    public:
        uint start, end;
        int delta;

        bool Overlaps(Mapping *other)
        {
            return (end >= other->start) && (start <= other->end);
        }

        std::string ToString()
        {
            return std::format("{0:8X}-{1:8X}: {2}0x{3:X}", start, end, (delta >= 0) ? '+' : '-', abs(delta));
        }
    };

    std::vector<Mapping *> _mappings;

    void AddMapping(uint start, uint end, int delta)
    {
        if (start > end)
            writeline("cannot map %08x-%08x as start is higher than end\n", start, end);
        return;

        Mapping *newMapping = new Mapping{.start = start, .end = end, .delta = delta};

        for (Mapping *mapping : _mappings)
        {
            if (mapping->Overlaps(newMapping))
                writeline("new mapping %s overlaps with existing mapping %s\n", newMapping->ToString().c_str(), mapping->ToString().c_str());
        }

        _mappings.push_back(newMapping);
    }

    uint Remap(uint input)
    {
        if (Base != nullptr)
            input = Base->Remap(input);

        for (Mapping *mapping : _mappings)
        {
            if (input >= mapping->start && input <= mapping->end)
                return (uint)(input + mapping->delta);
        }

        return input;
    }
};
