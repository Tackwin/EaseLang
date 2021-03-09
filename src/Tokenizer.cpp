#include "Tokenizer.hpp"
#include <cctype>
#include <algorithm>

bool string_comp(size_t off, std::string_view str, std::string_view string_source) noexcept {
	return strncmp(
		&string_source[off],
		str.data(),
		std::min(str.size(), string_source.size() - off)
	) == 0;
}

std::vector<Token> tokenize(std::string_view str) noexcept {
	std::vector<Token> tokens;
	tokens.reserve(str.size() / 10);

	size_t start = 0;
	size_t i = 0;
	size_t current_line = 0;
	size_t start_line = 0;

	auto isspace = [] (char c) {
		if (c == 0) return true;
		if (c == 9) return true;
		if (10 <= c && c <= 13) return true;
		if (c == 32) return true;
		return false;
	};

	auto advance = [&] {
		for (; i < str.size() && isspace(str[i]); ++i) {
			if (str[i] == '\n') {
				start_line = i + 1;
				current_line++;
			}
		}
		return i < str.size();
	};

	auto peek = [&] { return str[i + 1]; };
	auto peek_is = [&] (char c) { return i + 1 < str.size() && str[i + 1] == c; };

	while (advance()) {
		Token new_token;
		new_token.col = i - start_line;
		new_token.line = current_line;
		start = i;

		switch (str[i]) {
		case '(': new_token.type = Token::Type::Open_Paran;   i++; break;
		case ')': new_token.type = Token::Type::Close_Paran;  i++; break;
		case '{': new_token.type = Token::Type::Open_Brace;   i++; break;
		case '}': new_token.type = Token::Type::Close_Brace;  i++; break;
		case '[': new_token.type = Token::Type::Open_Brack;   i++; break;
		case ']': new_token.type = Token::Type::Close_Brack;  i++; break;
		case ',': new_token.type = Token::Type::Comma;        i++; break;
		case '.': new_token.type = Token::Type::Dot;          i++; break;
		case '%': new_token.type = Token::Type::Mod;          i++; break;
		case ';': new_token.type = Token::Type::Semicolon;    i++; break;
		case '+': {
			if (peek_is('+')) { new_token.type = Token::Type::Inc;  i += 2; }
			else              { new_token.type = Token::Type::Plus; i ++  ; }
			break;
		}
		case '*': new_token.type = Token::Type::Star;         i++; break;
		case '/': new_token.type = Token::Type::Div;         i++; break;
		case ':': new_token.type = Token::Type::Colon;        i++; break;
		case '<': {
			if (peek_is('=')) { new_token.type = Token::Type::Leq; i += 2; }
			else              { new_token.type = Token::Type::Lt;  i += 1; }
			break;
		}
		case '>': {
			if (peek_is('=')) { new_token.type = Token::Type::Geq; i += 2; }
			else              { new_token.type = Token::Type::Gt;  i += 1; }
			break;
		}
		case '!': {
			if (peek_is('=')) { new_token.type = Token::Type::Neq; i += 2; }
			else              { new_token.type = Token::Type::Not; i += 1; }
			break;
		}
		case '=': {
			if (peek_is('=')) { new_token.type = Token::Type::Eq;    i += 2; }
			else              { new_token.type = Token::Type::Equal; i += 1; }
			break;
		}
		case '-': {
			if (peek_is('>')) {
				new_token.type = Token::Type::Arrow;
				i += 2;
			} else {
				new_token.type = Token::Type::Minus;
				i++;
			}
			break;
		}
		case '"': {
			new_token.type = Token::Type::String;
			for (i++; i < str.size(); ++i) if (
				str[i] == '"' && (i > 0 && str[i-1] != '\\')
			) { i++; break; }
			break;
		}
		case '&': if (peek_is('&')) {
			new_token.type = Token::Type::And; i += 2; break;
		} else {
			new_token.type = Token::Type::Amp; i += 1; break;
		}
		case '|': if (peek_is('|')) { new_token.type = Token::Type::Or;  i += 2; break; }
		default:
		if (string_comp(i, "return", str)) {
			new_token.type = Token::Type::Return;
			i += 6;
		}
		else if (string_comp(i, "struct", str)) {
			new_token.type = Token::Type::Struct;
			i += 6;
		}
		else if (string_comp(i, "if", str)) {
			new_token.type = Token::Type::If;
			i += 2;
		}
		else if (string_comp(i, "else", str)) {
			new_token.type = Token::Type::Else;
			i += 4;
		}
		else if (string_comp(i, "for", str)) {
			new_token.type = Token::Type::For;
			i += 3;
		}
		else if (string_comp(i, "while", str)) {
			new_token.type = Token::Type::While;
			i += 5;
		}
		else if (string_comp(i, "proc", str)) {
			new_token.type = Token::Type::Proc;
			i += 4;
		}
		else if (string_comp(i, "const", str)) {
			new_token.type = Token::Type::Const;
			i += 5;
		}
		else if (isdigit(str[i])) {
			new_token.type = Token::Type::Number;
			for (; i < str.size() && isdigit(str[i]) || str[i] == '.'; i++);
		} else if (isalpha(str[i])) {
			new_token.type = Token::Type::Identifier;
			for (; i < str.size() && isalnum(str[i]) || str[i] == '_'; ++i);
		} else {
			new_token.type = Token::Type::Unknown;
			i++;
		}
		}

		new_token.lexeme = { start, i - start };
		tokens.push_back(new_token);
	}

	return tokens;
}

std::string token_type_to_string(Token::Type type) noexcept {
	switch (type) {
		case Token::Type::Open_Paran: return "Open_Paran";
		case Token::Type::Close_Paran: return "Close_Paran";
		case Token::Type::Open_Brace: return "Open_Brace";
		case Token::Type::Close_Brace: return "Close_Brace";
		case Token::Type::Open_Brack: return "Open_Brack";
		case Token::Type::Close_Brack: return "Close_Brack";
		case Token::Type::Identifier: return "Identifier";
		case Token::Type::Comma: return "Comma";
		case Token::Type::Colon: return "Colon";
		case Token::Type::Dot: return "Dot";
		case Token::Type::Minus: return "Minus";
		case Token::Type::Not: return "Not";
		case Token::Type::Geq: return "Geq";
		case Token::Type::Gt: return "Gt";
		case Token::Type::Leq: return "Leq";
		case Token::Type::Lt: return "Lt";
		case Token::Type::Eq: return "Eq";
		case Token::Type::Neq: return "Neq";
		case Token::Type::And: return "And";
		case Token::Type::Or: return "Or";
		case Token::Type::Amp: return "Amp";
		case Token::Type::Plus: return "Plus";
		case Token::Type::Mod: return "Mod";
		case Token::Type::Inc: return "Inc";
		case Token::Type::Div: return "Div";
		case Token::Type::Semicolon: return "Semicolon";
		case Token::Type::Arrow: return "Arrow";
		case Token::Type::Star: return "Star";
		case Token::Type::Return: return "Return";
		case Token::Type::Proc: return "Proc";
		case Token::Type::For: return "For";
		case Token::Type::While: return "While";
		case Token::Type::Const: return "Const";
		case Token::Type::Struct: return "Struct";
		case Token::Type::If: return "If";
		case Token::Type::Else: return "Else";
		case Token::Type::Equal: return "Equal";
		case Token::Type::Number: return "Number";
		case Token::Type::String: return "String";
		case Token::Type::Count: return "Count";
		default:
		case Token::Type::Unknown: return "Unknown";
	}
}
