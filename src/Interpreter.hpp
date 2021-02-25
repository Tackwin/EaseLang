#pragma once

#include <any>
#include <string_view>
#include "Expression.hpp"

// I guess you can't forward decl nested struct in c++ :)))))
// struct Expressions { struct AST_Node; };
struct Interpreter {
	using AST_Node = const Expressions::AST_Node&;

	std::any litteral(AST_Node node, std::string_view file) noexcept;

	std::any interpret(AST_Node node, std::string_view file) noexcept;

	void print_value(const std::any& value) noexcept;
};