
#pragma once
#include <string>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <string_view>
#include <type_traits>
#define println(x, ...) printf(x "\n", __VA_ARGS__)
#define printlns(x) printf(x "\n")

struct View {
	size_t i = 0;
	size_t size = 0;
};

static void append_tab(size_t n, std::string& str) noexcept {
	str.insert(0, n, '\t');
	for (size_t i = 0; i < str.size() - 1; ++i) if (str[i] == '\n') str.insert(i + 1, n, '\t');
}

static size_t hash_combine(size_t a, size_t b) noexcept {
	return a ^ b + 0x9e3779b9 + (a << 6) + (a >> 2);
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

#define sum_type_X_Kind(x) , x##_Kind
#define sum_type_X_Union(x) x x##_;
#define sum_type_X_cst(x) if constexpr (std::is_same_v<T, x>) {\
	kind = x##_Kind; new (&x##_) x; x##_ = (y);\
}
#define sum_type_X_case_cpy(x) case x##_Kind: new(&x##_) x; x##_ = that.x##_; break;
#define sum_type_X_case_mve(x) case x##_Kind: new(&x##_) x; x##_ = std::move(that.x##_); break;
#define sum_type_X_dst(x) case x##_Kind: x##_ .x::~x (); break;

#define sum_type(name, list)\
		enum Kind { None_Kind = 0 list(sum_type_X_Kind) } kind;\
		union { list(sum_type_X_Union) };\
		name() noexcept { kind = None_Kind; }\
		template<typename T> name(const T& y) noexcept { list(sum_type_X_cst) }\
		~name() {\
			switch(kind) {\
				list(sum_type_X_dst)\
				default: break;\
			}\
		}\
		name(std::nullptr_t) noexcept { kind = None_Kind; }\
		name(name&& that) noexcept {\
			kind = that.kind;\
			switch (kind) {\
				list(sum_type_X_case_mve)\
				default: break;\
			}\
		}\
		name(const name& that) noexcept {\
			kind = that.kind;\
			switch (kind) {\
				list(sum_type_X_case_cpy)\
				default: break;\
			}\
		}\
		name& operator=(const name& that) noexcept {\
			kind = that.kind;\
			switch (kind) {\
				list(sum_type_X_case_cpy)\
				default: break;\
			}\
			return *this;\
		}\
		name& operator=(name&& that) noexcept {\
			kind = that.kind;\
			switch (kind) {\
				list(sum_type_X_case_mve)\
				default: break;\
			}\
			return *this;\
		}\

