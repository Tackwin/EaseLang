#pragma once
#include <string>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <string_view>
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
static std::string_view string_view_from_view(std::string_view src, View view) noexcept {
	return std::string_view(src.data() + view.i, view.size);
}

namespace details {
	template<typename Callable>
	struct Defer {
		~Defer() noexcept { todo(); }
		Defer(Callable todo) noexcept : todo(todo) {};
	private:
		Callable todo;
	};
};

static double seconds() noexcept {
	auto now     = std::chrono::system_clock::now();
	auto epoch   = now.time_since_epoch();
	auto seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(epoch);

	// return the number of seconds
	return seconds.count() / 1'000'000'000.0;
}


#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)
#define defer details::Defer CONCAT(defer_, __COUNTER__) = [&]