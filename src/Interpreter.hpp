#pragma once

#include <any>
#include <vector>
#include <string_view>
#include <unordered_map>
#include "Expression.hpp"

// I guess you can't forward decl nested struct in c++ :)))))
// struct Expressions { struct AST_Node; };
struct Interpreter {
	// >PERF(Tackwin): std::string as key of unordered_map :'(
	// Ordered from out most scope to in most scope.
	std::vector<std::unordered_map<std::string, std::any>> variables;

	using AST_Nodes = const std::vector<Expressions::AST_Node>&;

	std::any litteral   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any unary_op   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any list_op    (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any factor     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any expression (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any identifier (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any assignement(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;

	std::any interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;

	void print_value(const std::any& value) noexcept;

	template<typename T>
	bool typecheck(const std::any& value) noexcept {
		return value.type() == typeid(std::decay_t<T>);
	}
};