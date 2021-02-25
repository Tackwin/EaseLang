#pragma once
#include <string>
#define println(x, ...) printf(x "\n", __VA_ARGS__)

struct View {
	size_t i = 0;
	size_t size = 0;
};

static void append_tab(size_t n, std::string& str) noexcept {
	str.insert(0, n, '\t');
	for (size_t i = 0; i < str.size() - 1; ++i) if (str[i] == '\n') str.insert(i + 1, n, '\t');
}

static std::string string_from_view(std::string_view src, View view) noexcept {
	std::string str;
	str.resize(view.size);
	memcpy(str.data(), &src[view.i], view.size);
	return str;
}
