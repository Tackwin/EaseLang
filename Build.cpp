#include "Ease.hpp"

/*
clang++ Build.cpp -o Build.exe -std=c++17
*/

EASE_WATCH_ME;

Build build(Flags flags) noexcept {
	if (flags.generate_debug) {
		flags.no_default_lib = true;
	}

	auto b = Build::get_default(flags);
	b.name = "EaseLang";

	b.add_source_recursively("src/");

	b.no_warnings_win32();
	if (flags.generate_debug) {
		b.add_debug_defines();
		b.add_library("msvcrtd");
		b.add_library("ucrtd");
		b.add_library("vcruntimed");
		b.add_library("libcpmtd");
		b.add_library("Kernel32");
	}

	return b;
}
