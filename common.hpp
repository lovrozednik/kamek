
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <format>
#include <map>
#include <regex>
#include <fstream>
#include <iterator>
#include <filesystem>

#include "sized_array.hpp"

#define writeline(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

typedef unsigned char byte;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long long ulong;