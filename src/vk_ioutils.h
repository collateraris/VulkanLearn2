#pragma once
#include <cstdio>
#include <unordered_map>
#include <string>

class IOUtils
{
public:
	static void loadMap(FILE* f, std::unordered_map<uint32_t, uint32_t>& map);

	static void saveMap(FILE* f, const std::unordered_map<uint32_t, uint32_t>& map);

	static void saveStringList(FILE* f, const std::vector<std::string>& lines);

	static void loadStringList(FILE* f, std::vector<std::string>& lines);
};