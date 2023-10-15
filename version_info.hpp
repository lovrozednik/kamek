#include "common.hpp"
#include "address_mapper.hpp"
#include "file.hpp"

class VersionInfo
{
public:
    std::map<std::string, AddressMapper *> _mappers;

    VersionInfo()
    {
        _mappers["default"] = new AddressMapper();
    }

    VersionInfo(std::string path)
    {
        std::regex commentRegex("^\\s*#");
        std::regex emptyLineRegex("^\\s*$");
        std::regex sectionRegex("^\\s*\\[([a-zA-Z0-9_.]+)\\]$");
        std::regex extendRegex("^\\s*extend ([a-zA-Z0-9_.]+)\\s*(#.*)?$");
        std::regex mappingRegex("^\\s*([a-fA-F0-9]{8})-((?:[a-fA-F0-9]{8})|\\*)\\s*:\\s*([-+])0x([a-fA-F0-9]+)\\s*(#.*)?$");

        std::string currentVersionName;
        AddressMapper *currentVersion;

        for (std::string line : File::ReadAllLines(path))
        {
            if (std::regex_match(line, emptyLineRegex))
                continue;
            if (std::regex_match(line, commentRegex))
                continue;

            std::smatch matches;
            if (std::regex_search(line, matches, sectionRegex))
            {
                currentVersionName = matches[1];
                if (_mappers.contains(currentVersionName))
                    writeline("versions file contains duplicate version name %s\n", currentVersionName);

                currentVersion = new AddressMapper();
                _mappers[currentVersionName] = currentVersion;
                continue;
            }

            if (currentVersion != nullptr)
            {
                if (std::regex_search(line, matches, extendRegex))
                {
                    std::string baseName = matches[1];
                    if (!_mappers.contains(baseName))
                        writeline("version %s extends unknown version %s\n", currentVersionName, baseName);
                    if (currentVersion->Base != nullptr)
                        writeline("version %s already extends a version\n", currentVersionName);

                    currentVersion->Base = _mappers[baseName];
                    continue;
                }

                if (std::regex_search(line, matches, mappingRegex))
                {
                    uint startAddress, endAddress;
                    int delta;

                    startAddress = std::stoi(matches[1], 0, 16);
                    if (matches[2] == "*")
                        endAddress = 0xFFFFFFFF;
                    else
                        endAddress = std::stoi(matches[2], 0, 16);

                    delta = std::stoi(matches[4], 0, 16);
                    if (matches[3] == "-")
                        delta = -delta;

                    currentVersion->AddMapping(startAddress, endAddress, delta);
                    continue;
                }
            }

            writeline("unrecognised line in versions file: %s\n", line);
        }
    }
};
