
#pragma once
#include <string>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <assert.h>
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

template<bool flag = false> void static_no_match() noexcept {
	static_assert(flag, "No match.");
}

#define sum_type_X_Kind(x) , x##_Kind
#define sum_type_X_Union(x) x x##_;
#define sum_type_X_cst(x) else if constexpr (std::is_same_v<T, x>) {\
	kind = x##_Kind; new (&x##_) x; x##_ = (y);\
}
#define sum_type_X_case_cpy(x) case x##_Kind: new(&x##_) x; x##_ = that.x##_; break;
#define sum_type_X_case_mve(x) case x##_Kind: new(&x##_) x; x##_ = std::move(that.x##_); break;
#define sum_type_X_dst(x) case x##_Kind: x##_ .x::~x (); break;
#define sum_type_X_name(x) case x##_Kind: return #x;
#define sum_type_X_cast(x) if constexpr (std::is_same_v<T, x>) { return x##_; }
#define sum_type_X_one_of(x) std::is_same_v<T, x> ||

#define sum_type(n, list)\
		enum Kind { None_Kind = 0 list(sum_type_X_Kind) } kind;\
		union { list(sum_type_X_Union) };\
		n() noexcept { kind = None_Kind; }\
		template<typename T> n(const T& y) noexcept {\
			if constexpr (false);\
			list(sum_type_X_cst)\
			else static_no_match<list(sum_type_X_one_of) false>();\
		}\
		~n() {\
			switch(kind) {\
				list(sum_type_X_dst)\
				default: break;\
			}\
		}\
		n(std::nullptr_t) noexcept { kind = None_Kind; }\
		n(n&& that) noexcept {\
			kind = that.kind;\
			switch (kind) {\
				list(sum_type_X_case_mve)\
				default: break;\
			}\
		}\
		n(const n& that) noexcept {\
			kind = that.kind;\
			switch (kind) {\
				list(sum_type_X_case_cpy)\
				default: break;\
			}\
		}\
		n& operator=(const n& that) noexcept {\
			kind = that.kind;\
			switch (kind) {\
				list(sum_type_X_case_cpy)\
				default: break;\
			}\
			return *this;\
		}\
		n& operator=(n&& that) noexcept {\
			kind = that.kind;\
			switch (kind) {\
				list(sum_type_X_case_mve)\
				default: break;\
			}\
			return *this;\
		}\
		const char* name() const noexcept {\
			switch (kind) {\
				list(sum_type_X_name)\
				default: break;\
			}\
			return "??";\
		}\
		bool typecheck(n::Kind k) const noexcept { return kind == k; }\
		template<typename T>\
		const T& cast() const noexcept {\
			list(sum_type_X_cast)\
			assert("Yeah no.");\
			return *reinterpret_cast<const T*>(this);\
		}\
		template<typename T>\
		T cast() noexcept {\
			list(sum_type_X_cast)\
			assert("Yeah no.");\
			return *reinterpret_cast<T*>(this);\
		}
