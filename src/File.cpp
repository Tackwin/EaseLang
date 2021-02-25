#include "File.hpp"

#include <stdio.h>
#include <algorithm>

void read_whole_text(
	const std::filesystem::path& path, char* out_buffer, size_t out_buffer_size
) noexcept {
	if (out_buffer_size == 0) return;

	FILE* file = fopen(path.generic_string().c_str(), "r");
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	fread(out_buffer, 1, std::min(size, out_buffer_size - 1), file);

	fclose(file);

	out_buffer[size] = 0;
}

std::string read_whole_text(const std::filesystem::path& path) noexcept {
	std::string result;
	
	FILE* file = fopen(path.generic_string().c_str(), "r");
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	result.resize(size);

	fread(result.data(), 1, result.size(), file);

	fclose(file);
	return result;
}