#pragma once

#include <string>
#include <filesystem>

extern void read_whole_text(
	const std::filesystem::path& path, char* out_buffer, size_t out_buffer_size
) noexcept;
extern std::string read_whole_text(const std::filesystem::path& path) noexcept;