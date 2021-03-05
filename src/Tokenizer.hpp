#pragma once

#include <vector>
#include <string_view>

#include "xstd.hpp"


struct Token {
	enum class Type {
		Open_Paran = 0,
		Close_Paran,
		Open_Brace,
		Close_Brace,
		Open_Brack,
		Close_Brack,
		Identifier,
		Comma,
		Colon,
		Dot,
		Minus,
		Not,
		Geq,
		Gt,
		Leq,
		Lt,
		Eq,
		Neq,
		And,
		Or,
		Amp,
		Plus,

		Semicolon,
		Arrow,
		Star,

//		KEYWORD
		Return,
		Proc,
		Const,
		Struct,
		If,
		Else,

		Equal,
		Number,
		String,
		Unknown,
		Count
	};

	View lexeme;
	size_t line;
	size_t col;

	Type type;
};

extern std::vector<Token> tokenize(std::string_view str) noexcept;