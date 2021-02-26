#include "Interpreter.hpp"
#include "Expression.hpp"

#include "xstd.hpp"

#include <charconv>

std::any Interpreter::interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx];
	switch (node.kind) {
	case node.Identifier_Kind:      return identifier (nodes, idx, file);
	case node.Assignement_Kind:     return assignement(nodes, idx, file);
	case node.Litteral_Kind:        return litteral   (nodes, idx, file);
	case node.Operation_List_Kind:  return list_op    (nodes, idx, file);
	case node.Unary_Operation_Kind: return unary_op   (nodes, idx, file);
	case node.Expression_Kind:      return expression (nodes, idx, file);
	default:                        return nullptr;
	}
}

std::any Interpreter::unary_op(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Unary_Operation_;

	switch (node.op) {
		case Expressions::Operator::Minus: {
			auto x = interpret(nodes, node.right_idx, file);
			if (typecheck<long double>(x)) {
				println("Type error, expected long double got %s", x.type().name());
				return nullptr;
			}
			return -std::any_cast<long double>(x);
		}
		case Expressions::Operator::Plus: {
			auto x = interpret(nodes, node.right_idx, file);
			if (typecheck<long double>(x)) {
				println("Type error, expected long double got %s", x.type().name());
				return nullptr;
			}
			return +std::any_cast<long double>(x);
		}
		case Expressions::Operator::Not:
		default: return nullptr;
	}
}

std::any Interpreter::list_op(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Operation_List_;
	switch(node.op) {
		case Expressions::Operator::Gt: {
			auto left = interpret(nodes, node.operand_idx, file);
			auto right = interpret(nodes, node.next_statement, file);
			
			if (!typecheck<long double>(left)) {
				println("Error expected long double for the lhs, got %s", left.type().name());
				return nullptr;
			}
			if (!typecheck<long double>(right)) {
				println("Error expected long double for the rhs, got %s", right.type().name());
				return nullptr;
			}

			return std::any_cast<long double>(left) > std::any_cast<long double>(right);
		}
		default: return nullptr;
	}
}

std::any Interpreter::factor(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx];
	#define EA Expressions::AST_Node
	switch (nodes[idx].kind) {
		default:                       return litteral  (nodes, idx, file);
		case EA::Unary_Operation_Kind: return unary_op  (nodes, idx, file);
		case EA::Identifier_Kind:      return identifier(nodes, idx, file);
		case EA::Expression_Kind:
			// return expression(nodes, node.Expression_.next_statement, file);
		case EA::Function_Call_Kind:
			// >TODO(Tackwin):
			return nullptr;

	}
	#undef EA
}

std::any Interpreter::identifier(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Identifier_;

	auto name = string_from_view(file, node.token.lexeme);
	for (size_t i = variables.size() - 1; i + 1 > 0; --i) {
		for (auto& [n, v] : variables[i]) if (n == name) return v;
	}

	println("Can not find variable named %s.", name.c_str());
	return nullptr;
}


std::any Interpreter::assignement(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Assignement_;

	// >TODO(Tackwin): handle type info.
	auto name = string_from_view(file, node.identifier.lexeme);

	if (variables.empty()) variables.push_back({});
	if (variables.back().count(name) > 0) {
		println("Error variable %s already assigned.", name.c_str());
		return nullptr;
	}

	variables.back()[name] = expression(nodes, node.value_idx, file);

	return variables.back()[name];
}

std::any Interpreter::litteral(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Litteral_;
	auto view = node.token.lexeme;

	if (node.token.type == Token::Type::Number) {
		long double x = 0;
		auto res = std::from_chars(file.data() + view.i, file.data() + view.i + view.size, x);

		if (res.ec == std::errc::invalid_argument) return nullptr;
		return x;
	}

	if (node.token.type == Token::Type::String) {
		return std::string(file.data() + view.i + 1, view.size - 2);
	}
	
	return nullptr;
}

std::any Interpreter::expression(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	return interpret(nodes, nodes[idx].Expression_.next_statement, file);
}

void Interpreter::print_value(const std::any& value) noexcept {
	if (typecheck<std::nullptr_t>(value)) {
		println("RIP F in the chat for my boiii");
		return;
	}

	if (typecheck<long double>(value)) {
		auto x = std::any_cast<long double>(value);
		println("%Lf", x);
		return;
	}

	if (typecheck<bool>(value)) {
		auto x = std::any_cast<bool>(value);
		println("%s", x ? "true" : "false");
		return;
	}

	if (typecheck<std::string>(value)) {
		auto x = std::any_cast<std::string>(value);
		println("%s", x.c_str());
		return;
	}
}
