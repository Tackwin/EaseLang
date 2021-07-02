#include "Ease.hpp"

/*
clang++ Build.cpp -o Build.exe -std=c++17
*/

EASE_WATCH_ME;

Build build(Flags flags) noexcept {
	if (Env::Win32 && flags.generate_debug) {
		flags.no_default_lib = true;
	}

	auto b = Build::get_default(flags);
	b.name = "EaseLang";
	// for g++ people :eyes
	// b.compiler = "g++";


	b.stack_size *= 64;

	b.add_source_recursively("src/");

	b.no_warnings_win32();
	if (Env::Win32 && flags.generate_debug) {
		if (flags.release) {
			b.add_library("libucrt");
			b.add_library("libvcruntime");
			b.add_library("libcmt");
			b.add_library("libcpmt");
			b.add_library("libconcrt");
			b.add_library("Kernel32");
		} else {
			b.add_debug_defines();
			b.add_library("libucrtd");
			b.add_library("libvcruntimed");
			b.add_library("libcmtd");
			b.add_library("libcpmtd");
			b.add_library("libconcrtd");
			b.add_library("Kernel32");
		}
	}

	return b;
}
