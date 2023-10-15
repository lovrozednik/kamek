#include "sized_array.hpp"

class File
{
public:
    static std::vector<std::string> ReadAllLines(std::string file)
    {
        std::ifstream myFile(file);
        std::vector<std::string> myLines;
        std::copy(std::istream_iterator<std::string>(myFile),
                  std::istream_iterator<std::string>(),
                  std::back_inserter(myLines));
        return myLines;
    }

    static void WriteAllBytes(std::string path, sized_array *bytes)
    {
        FILE *fp = fopen(path.c_str(), "w");
        fwrite(bytes->data, 1, bytes->length, fp);
        fclose(fp);
    }

    static void WriteAllText(std::string path, std::string text)
    {
        FILE *fp = fopen(path.c_str(), "w");
        fwrite(text.data(), 1, text.length(), fp);
        fclose(fp);
    }

    static uint64_t GetFileSize(std::string path)
    {
        return std::filesystem::file_size(path.c_str());
    }

    static sized_array *ReadAllBytes(std::string path)
    {
        sized_array *arr = new sized_array(GetFileSize(path));

        FILE *fp = fopen(path.c_str(), "r");
        fread(arr->data, 1, arr->length, fp);
        fclose(fp);

        return arr;
    }
};