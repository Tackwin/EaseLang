#include "Interpreter.hpp"
#include "Expression.hpp"

#include "xstd.hpp"

#include <charconv>

std::any Interpreter::interpret(AST_Node node, std::string_view file) noexcept {
	switch (node.kind) {
	case node.Litteral_Kind: return litteral(node, file);
	default:                 return nullptr;
	}
}


std::any Interpreter::litteral(AST_Node node, std::string_view file) noexcept {
	auto view = node.Litteral_.token.lexeme;

	if (node.Litteral_.token.type == Token::Type::Number) {
		long double x = 0;
		auto res = std::from_chars(file.data() + view.i, file.data() + view.i + view.size, x);

		if (res.ec == std::errc::invalid_argument) return nullptr;
		return x;
	}

	if (node.Litteral_.token.type == Token::Type::String) {
		return std::string(file.data() + view.i + 1, view.size - 2);
	}
	
	return nullptr;
}

void Interpreter::print_value(const std::any& value) noexcept {
	if (value.type() == typeid(std::nullptr_t)) {
		println("RIP F in the chat for my boiii");
		return;
	}

	if (value.type() == typeid(long double)) {
		auto x = std::any_cast<long double>(value);
		println("%Lf", x);
		return;
	}

	if (value.type() == typeid(std::string)) {
		auto x = std::any_cast<std::string>(value);
		println("%s", x.c_str());
		return;
	}
}
