#include <vk_ioutils.h>

void IOUtils::loadMap(FILE* f, std::unordered_map<uint32_t, uint32_t>& map)
{
	std::vector<uint32_t> ms;

	uint32_t sz = 0;
	fread(&sz, 1, sizeof(sz), f);

	ms.resize(sz);
	fread(ms.data(), sizeof(int), sz, f);
	for (size_t i = 0; i < (sz / 2); i++)
		map[ms[i * 2 + 0]] = ms[i * 2 + 1];
}

void IOUtils::saveMap(FILE* f, const std::unordered_map<uint32_t, uint32_t>& map)
{
	std::vector<uint32_t> ms;
	ms.reserve(map.size() * 2);
	for (const auto& m : map)
	{
		ms.push_back(m.first);
		ms.push_back(m.second);
	}
	const uint32_t sz = static_cast<uint32_t>(ms.size());
	fwrite(&sz, sizeof(sz), 1, f);
	fwrite(ms.data(), sizeof(int), ms.size(), f);
}

void IOUtils::saveStringList(FILE* f, const std::vector<std::string>& lines)
{
	uint32_t sz = (uint32_t)lines.size();
	fwrite(&sz, sizeof(uint32_t), 1, f);
	for (const auto& s : lines)
	{
		sz = (uint32_t)s.length();
		fwrite(&sz, sizeof(uint32_t), 1, f);
		fwrite(s.c_str(), sz + 1, 1, f);
	}
}

void IOUtils::loadStringList(FILE* f, std::vector<std::string>& lines)
{
	{
		uint32_t sz = 0;
		fread(&sz, sizeof(uint32_t), 1, f);
		lines.resize(sz);
	}
	std::vector<char> inBytes;
	for (auto& s : lines)
	{
		uint32_t sz = 0;
		fread(&sz, sizeof(uint32_t), 1, f);
		inBytes.resize(sz + 1);
		fread(inBytes.data(), sz + 1, 1, f);
		s = std::string(inBytes.data());
	}
}
