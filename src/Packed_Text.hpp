#pragma once
#include <string_view>
#include <vector>
#include <span>

// Span is so fucking retarded omg, why do I have to make a class to make the _simplest_
// version of span. Just a pair of index and size.
// Like a vector can grow dumbass i can't garantee the fucking pointer and nobody want
// to deal with the shitty iterator API
struct Vector_View {
	size_t vector_id = 0; // Ok i'm adding an identifier to distinguish different buffer
	                      // but still omg
	size_t offset;
	size_t size;

	bool operator==(const Vector_View& b) const noexcept {
		return offset == b.offset && size == b.size;
	}

	bool operator!=(const Vector_View& b) const noexcept {
		return offset != b.offset || size != b.size;
	}
};
namespace std {
	template<> struct hash<Vector_View> {
		size_t hash_combine(size_t a, size_t b) const noexcept {
			return a ^ b + 0x9e3779b9 + (a << 6) + (a >> 2);
		}
		std::size_t operator()(const Vector_View& s) const noexcept {
			std::size_t h1 = s.vector_id;
			std::size_t h2 = s.offset;
			std::size_t h3 = s.size;
			return hash_combine(hash_combine(h1, h2), h3);
		}
	};
}

struct String_Pool {
	size_t pool_id = 0;
	std::vector<char> bank;

	std::string_view to_string(Vector_View span) const noexcept {
		return { bank.data() + span.offset, span.size };
	}

	const char* to_cstr(Vector_View span) const noexcept {
		return bank.data() + span.offset;
	}

	void clear() noexcept {
		bank.clear();
	}
	size_t size() const noexcept {
		return bank.size();
	}
	const char* data() const noexcept {
		return bank.data();
	}

	void reserve(size_t n) noexcept {
		bank.reserve(n);
	}
	void resize(size_t n) noexcept {
		bank.resize(n);
	}

	Vector_View insert(std::string_view str) noexcept {
		for (size_t i = 0; i + str.size() < bank.size(); ++i) {
			if (memcmp(bank.data() + i, str.data(), str.size()) == 0)
				return { pool_id, i, str.size() };
		}
		bank.resize(bank.size() + str.size() + 1);
		memcpy(bank.data() + bank.size() - str.size() - 1, str.data(), str.size());
		bank.back() = '\0';
		return { pool_id, bank.size() - str.size() - 1, str.size() };
	}
	Vector_View force_insert(std::string_view str) noexcept {
		bank.resize(bank.size() + str.size() + 1);
		memcpy(bank.data() + bank.size() - str.size() - 1, str.data(), str.size());
		bank.back() = '\0';
		return { pool_id, bank.size() - str.size() - 1, str.size() };
	}

	Vector_View insert(const String_Pool& str) noexcept {
		bank.resize(bank.size() + str.size());
		memcpy(bank.data() + bank.size() - str.size(), str.data(), str.size());
		return { pool_id, bank.size() - str.size(), str.size() };
	}
	Vector_View force_insert(const String_Pool& str, Vector_View span) noexcept {
		bank.resize(bank.size() + span.size + 1);
		memcpy(bank.data() + bank.size() - span.size - 1, str.data() + span.offset, span.size);
		bank.back() = '\0';
		return { pool_id, bank.size() - span.size - 1, span.size };
	}
	Vector_View insert(const String_Pool& str, Vector_View span) noexcept {
		for (size_t i = 0; i + span.size < bank.size(); ++i) {
			if (memcmp(bank.data() + i, str.data() + span.offset, span.size) == 0)
				return { pool_id, i, span.size };
		}
		bank.resize(bank.size() + span.size + 1);
		memcpy(bank.data() + bank.size() - span.size - 1, str.data() + span.offset, span.size);
		bank.back() = '\0';
		return { pool_id, bank.size() - span.size - 1, span.size };
	}
};
