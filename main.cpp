#include "common.hpp"
#include "elf.hpp"
#include "version_info.hpp"
#include "linker.hpp"
#include "kamek_file.hpp"

#define reterr return -__COUNTER__

void ShowHelp()
{
    writeline("Syntax:");
    writeline("  Kamek file1.o [file2.o...] [options]");
    writeline("");
    writeline("Options:");
    writeline("  Build Mode (select one; defaults to -dynamic):");
    writeline("    -dynamic");
    writeline("      generate a dynamically linked Kamek binary for use with the loader");
    writeline("    -static=0x80001900");
    writeline("      generate a blob of code which must be loaded at the specified Wii RAM address");
    writeline("");
    writeline("  Game Configuration:");
    writeline("    -externals=file.txt");
    writeline("      specify the addresses of external symbols that exist in the target game");
    writeline("    -versions=file.txt");
    writeline("      specify the different executable versions that Kamek can link binaries for");
    writeline("    -select-version=key");
    writeline("      build only one version from the versions file, and ignore the rest");
    writeline("      (can be specified multiple times)");
    writeline("");
    writeline("  Outputs (at least one is required; \\$KV\\$ will be replaced with the version name):");
    writeline("    -output-kamek=file.\\$KV\\$.bin");

    writeline("      write a Kamek binary to for use with the loader (-dynamic only)");
    writeline("    -output-riiv=file.\\$KV\\$.xml");
    writeline("      write a Riivolution XML fragment (-static only)");
    writeline("    -output-dolphin=file.\\$KV\\$.ini");
    writeline("      write a Dolphin INI fragment (-static only)");
    writeline("    -output-gecko=file.\\$KV\\$.xml");
    writeline("      write a list of Gecko codes (-static only)");
    writeline("    -output-ar=file.\\$KV\\$.xml");
    writeline("      write a list of Action Replay codes (-static only)");
    writeline("    -input-dol=file.\\$KV\\$.dol -output-dol=file2.\\$KV\\$.dol");
    writeline("      apply these patches and generate a modified DOL (-static only)");
    writeline("    -output-code=file.\\$KV\\$.bin");
    writeline("      write the combined code+data segment to file.bin (for manual injection or debugging)");
};

void ReadExternals(std::map<std::string, uint> dict, std::string path)
{
    std::regex commentRegex("^\\s*#");
    std::regex emptyLineRegex("^\\s*$");
    std::regex assignmentRegex("^\\s*([a-zA-Z0-9_<>@,-\\$]+)\\s*=\\s*0x([a-fA-F0-9]+)\\s*(#.*)?$");

    for (std::string line : File::ReadAllLines(path))
    {
        if (std::regex_match(line, emptyLineRegex))
            continue;
        if (std::regex_match(line, commentRegex))
            continue;

        std::smatch matches;
        if (std::regex_search(line, matches, assignmentRegex))
        {
            dict[matches[1]] = std::stoi(matches[2], 0, 16);
        }
        else
        {
            writeline("unrecognised line in externals file: %s\n", line);
        }
    }
};

std::vector<std::string> split(std::string input, std::string delimiter)
{

    std::vector<std::string> tokens;
    size_t pos = 0;
    std::string token;

    while ((pos = input.find(delimiter)) != std::string::npos)
    {
        token = input.substr(0, pos);
        tokens.push_back(token);
        input.erase(0, pos + 1);
    }

    tokens.push_back(input);

    return tokens;
}

int main(int argc, char *argv[])
{
    writeline("Kamek 2.0 by Ninji/Ash Wolf - https://github.com/Treeki/Kamek, ported to C++ by zednik-lovro - https://github.com/zednik-lovro");

    std::vector<Elf *> modules;

    uint baseAddress = 0;

    std::string outputKamekPath = "", outputRiivPath = "", outputDolphinPath = "", outputGeckoPath = "", outputARPath = "", outputCodePath = "";
    std::string inputDolPath = "", outputDolPath = "";

    std::map<std::string, uint> externals;

    VersionInfo *versions = nullptr;
    std::vector<std::string> selectedVersions;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg.starts_with("-"))
        {
            if (arg == "-h" || arg == "-help" || arg == "--help")
            {
                ShowHelp();
                reterr;
            }
            if (arg == "-dynamic")
                baseAddress = 0;
            else if (arg.starts_with("-static=0x"))
                baseAddress = std::stoi(arg.substr(10), 0, 16);
            else if (arg.starts_with("-output-kamek="))
                outputKamekPath = arg.substr(14);
            else if (arg.starts_with("-output-riiv="))
                outputRiivPath = arg.substr(13);
            else if (arg.starts_with("-output-dolphin="))
                outputDolphinPath = arg.substr(16);
            else if (arg.starts_with("-output-gecko="))
                outputGeckoPath = arg.substr(14);
            else if (arg.starts_with("-output-ar="))
                outputARPath = arg.substr(11);
            else if (arg.starts_with("-output-code="))
                outputCodePath = arg.substr(13);
            else if (arg.starts_with("-input-dol="))
                inputDolPath = arg.substr(11);
            else if (arg.starts_with("-output-dol="))
                outputDolPath = arg.substr(12);
            else if (arg.starts_with("-externals="))
                ReadExternals(externals, arg.substr(11));
            else if (arg.starts_with("-versions="))
                versions = new VersionInfo(arg.substr(10));
            else if (arg.starts_with("-select-version="))
                selectedVersions.push_back(arg.substr(16));
            else if (arg.starts_with("-under-sym-mask="))
                Linker::FixedUndefinedSymbols = split(arg.substr(16), ",");
            else
                writeline("warning: unrecognised argument: {0}", arg);
        }
        else
        {
            FILE *fp = fopen(arg.c_str(), "r");
            byte *stream = new byte[std::filesystem::file_size(arg.c_str())];
            fclose(fp);

            writeline("adding %s as object..\n", arg);
            modules.push_back(new Elf(stream));

            delete stream;
        }
    }

    // We need a default VersionList for the loop later
    if (versions == nullptr)
        versions = new VersionInfo();

    // Can we build a thing?
    if (modules.size() == 0)
    {
        writeline("no input files specified");
        reterr;
    }
    if (outputKamekPath == "" && outputRiivPath == "" && outputDolphinPath == "" && outputGeckoPath == "" && outputARPath == "" && outputCodePath == "" && outputDolPath == "")
    {
        writeline("no output path(s) specified");
        reterr;
    }
    if (outputDolPath != "" && inputDolPath == "")
    {
        writeline("input dol path not specified");
        reterr;
    }

    // Do safety checks
    if (versions->_mappers.size() > 1 && selectedVersions.size() != 1)
    {
        bool ambiguousOutputPath = false;
        ambiguousOutputPath |= (outputKamekPath != "" && !outputKamekPath.contains("\\$KV\\$"));
        ambiguousOutputPath |= (outputRiivPath != "" && !outputRiivPath.contains("\\$KV\\$"));
        ambiguousOutputPath |= (outputDolphinPath != "" && !outputDolphinPath.contains("\\$KV\\$"));
        ambiguousOutputPath |= (outputGeckoPath != "" && !outputGeckoPath.contains("\\$KV\\$"));
        ambiguousOutputPath |= (outputARPath != "" && !outputARPath.contains("\\$KV\\$"));
        ambiguousOutputPath |= (outputCodePath != "" && !outputCodePath.contains("\\$KV\\$"));
        ambiguousOutputPath |= (outputDolPath != "" && !outputDolPath.contains("\\$KV\\$"));
        if (ambiguousOutputPath)
        {
            writeline("ERROR: this configuration builds for multiple game versions, and some of the outputs will be overwritten");
            writeline("add the \\$KV\\$ placeholder to your output paths, or use -select-version=.. to only build one version");
            reterr;
        }
    }

    for (auto version : versions->_mappers)
    {
        if (selectedVersions.size() > 0 && std::find(selectedVersions.begin(), selectedVersions.end(), version.first) == selectedVersions.end())
        {
            writeline("(skipping version {0} as it's not selected)", version.first);
            continue;
        }
        writeline("linking version {0}...", version.first);

        Linker *linker = new Linker(version.second);
        for (auto module : modules)
            linker->AddModule(module);

        if (baseAddress != 0)
            linker->LinkStatic(baseAddress, externals);
        else
            linker->LinkDynamic(externals);

        KamekFile *kf = new KamekFile();
        kf->LoadFromLinker(linker);
        if (outputKamekPath != "")
            File::WriteAllBytes(std::regex_replace(outputKamekPath, std::regex("\\$KV\\$"), version.first), kf->Pack());
        if (outputRiivPath != "")
            File::WriteAllText(std::regex_replace(outputRiivPath, std::regex("\\$KV\\$"), version.first), kf->PackRiivolution());
        if (outputDolphinPath != "")
            File::WriteAllText(std::regex_replace(outputDolphinPath, std::regex("\\$KV\\$"), version.first), kf->PackDolphin());
        if (outputGeckoPath != "")
            File::WriteAllText(std::regex_replace(outputGeckoPath, std::regex("\\$KV\\$"), version.first), kf->PackGeckoCodes());
        if (outputARPath != "")
            File::WriteAllText(std::regex_replace(outputARPath, std::regex("\\$KV\\$"), version.first), kf->PackActionReplayCodes());
        if (outputCodePath != "")
            File::WriteAllBytes(std::regex_replace(outputCodePath, std::regex("\\$KV\\$"), version.first), kf->_codeBlob);

        if (outputDolPath != "")
        {
            sized_array *dolBytes = File::ReadAllBytes(std::regex_replace(inputDolPath, std::regex("\\$KV\\$"), version.first));

            Dol *dol = new Dol(dolBytes->data);
            kf->InjectIntoDol(dol);

            std::string outpath = std::regex_replace(outputDolPath, std::regex("\\$KV\\$"), version.first);

            sized_array *outStream = new sized_array(16 * 1024 * 1024);

            uint64_t dolSize = dol->Write(outStream->data);
            File::WriteAllBytes(outpath, outStream);

            delete[] outStream;
        }
    }
};