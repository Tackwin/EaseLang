#include "Interpreter.hpp"
#include "Expression.hpp"

#include "xstd.hpp"

#include <charconv>

std::any Interpreter::interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx];
	switch (node.kind) {
	case Expressions::AST_Node::Identifier_Kind:          return identifier   (nodes, idx, file);
	case Expressions::AST_Node::Assignement_Kind:         return assignement  (nodes, idx, file);
	case Expressions::AST_Node::Litteral_Kind:            return litteral     (nodes, idx, file);
	case Expressions::AST_Node::Operation_List_Kind:      return list_op      (nodes, idx, file);
	case Expressions::AST_Node::Unary_Operation_Kind:     return unary_op     (nodes, idx, file);
	case Expressions::AST_Node::Expression_Kind:          return expression   (nodes, idx, file);
	case Expressions::AST_Node::Function_Definition_Kind: return function     (nodes, idx, file);
	case Expressions::AST_Node::If_Kind:                  return if_call      (nodes, idx, file);
	case Expressions::AST_Node::Function_Call_Kind:       return function_call(nodes, idx, file);
	case Expressions::AST_Node::Return_Call_Kind:         return return_call  (nodes, idx, file);
	case Expressions::AST_Node::Struct_Definition_Kind:   return struct_def   (nodes, idx, file);
	case Expressions::AST_Node::Initializer_List_Kind:    return init_list    (nodes, idx, file);
	default:                        return nullptr;
	}
}

std::any Interpreter::interpret(
	AST_Nodes nodes, const Function_Definition& f, std::string_view file
) noexcept {
	push_scope();
	defer { pop_scope(); };

	for (auto& [id, x] : f.parameters) variables.back()[(std::string)id] = x;

	std::any v;
	for (size_t idx = f.start_idx; idx; idx = nodes[idx]->next_statement) {
		v = interpret(nodes, idx, file);
		if (typecheck<Return_Call>(v)) break;
	}

	if (!typecheck<Return_Call>(v) && !f.return_types.empty()) {
		printlns("Reached end of non void returning function.");
		return nullptr;
	}
	if (!typecheck<Return_Call>(v)) return Void();
	auto r = std::any_cast<Return_Call>(v);
	if (r.values.size() != f.return_types.size()) {
		println(
			"Trying to return from a function with wrong number of return parameters, "
			"expected %zu got %zu.", f.return_types.size(), r.values.size()
		);
		return nullptr;
	}

	return r.values.empty() ? nullptr : r.values.front();
}

std::any Interpreter::init_list(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Initializer_List_;

	if (node.type_identifier) {
		auto type_name = string_view_from_view(file, node.type_identifier->lexeme);
		auto type = lookup(type_name);
		if (!typecheck<User_Struct>(type)) {
			println("Error expected UserStruct got %s.", type.type().name());
			return nullptr;
		}

		auto user_struct = std::any_cast<User_Struct>(type);
		
		for (
			size_t idx = node.expression_list_idx, i = 0;
			idx;
			idx = nodes[idx]->next_statement, i++
		) user_struct.member_values[i] = interpret(nodes, idx, file);

		return user_struct;
	}
	printlns("Please specify the type explicitely.");

	return nullptr;
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
		case Expressions::Operator::Lt: {
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

			return std::any_cast<long double>(left) < std::any_cast<long double>(right);
		}
		case Expressions::Operator::Leq: {
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

			return std::any_cast<long double>(left) <= std::any_cast<long double>(right);
		}
		case Expressions::Operator::Plus: {
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

			return std::any_cast<long double>(left) + std::any_cast<long double>(right);
		}
		case Expressions::Operator::Minus: {
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

			return std::any_cast<long double>(left) - std::any_cast<long double>(right);
		}
		case Expressions::Operator::Dot: {
			auto root_struct = interpret(nodes, node.operand_idx, file);
			if (!typecheck<User_Struct>(root_struct)) {
				println("Expected a struct but got %s instead.", root_struct.type().name());
				return nullptr;
			}

			auto helper =
			[&] (const User_Struct& user_struct, size_t next, auto& helper) -> std::any {
				auto& next_node = nodes[next];
				if (next_node.kind != Expressions::AST_Node::Identifier_Kind) {
					printlns("Error non identifier in the chain.");
					return nullptr;
				}

				auto name = string_from_view(file, next_node.Identifier_.token.lexeme);
				size_t i = 0;
				while(i < user_struct.member_names.size() && user_struct.member_names[i] != name)
					i++;

				if (i == user_struct.member_names.size()) {
					println("Error %s is not a member of this struct.", name.c_str());
					return nullptr;
				}

				auto& x = user_struct.member_values[i];
				if (typecheck<User_Struct>(x)) {
					return helper(
						std::any_cast<User_Struct>(x), next_node.Identifier_.next_statement, helper
					);
				}

				return x;
			};

			return helper(std::any_cast<User_Struct>(root_struct), node.next_statement, helper);
		}
		default:{
			println("Unsupported operation %s", Expressions::op_to_string(node.op));
			return nullptr;
		}
	}
}

std::any Interpreter::factor(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx];
	#define EA Expressions::AST_Node
	switch (nodes[idx].kind) {
		default:                       return litteral  (nodes, idx, file);
		case EA::Unary_Operation_Kind: return unary_op  (nodes, idx, file);
		case EA::Identifier_Kind:      return identifier(nodes, idx, file);
		case EA::Expression_Kind:      return expression(nodes, idx, file);
		case EA::Function_Call_Kind:   return function_call(nodes, idx, file);

	}
	#undef EA
}

std::any Interpreter::if_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].If_;

	auto cond = interpret(nodes, node.condition_idx, file);
	if (!typecheck<bool>(cond)) {
		println("Expected bool on the if-condition got %s.", cond.type().name());
		return nullptr;
	}

	push_scope();
	defer { pop_scope(); };
	if (std::any_cast<bool>(cond)) {
		for (size_t idx = node.if_statement_idx; idx; idx = nodes[idx]->next_statement) {
			auto v = interpret(nodes, idx, file);
			if (typecheck<Return_Call>(v)) return v;
		}
	} else {
		for (size_t idx = node.else_statement_idx; idx; idx = nodes[idx]->next_statement) {
			auto v = interpret(nodes, idx, file);
			if (typecheck<Return_Call>(v)) return v;
		}
	}

	return nullptr;
}

std::any Interpreter::struct_def(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Struct_Definition_;

	User_Struct x;
	for (size_t idx = node.struct_line_idx; idx; idx = nodes[idx]->next_statement) {
		auto& def = nodes[idx].Assignement_;
		auto name = string_from_view(file, def.identifier.lexeme);
		x.member_names.push_back(name);
		x.member_values.push_back(interpret(nodes, def.value_idx, file));
	}

	return x;
}

std::any Interpreter::function(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Function_Definition_;

	Function_Definition f;

	for (size_t idx = node.parameter_list_idx; idx; idx = nodes[idx]->next_statement) {
		auto& param = nodes[idx].Parameter_;
		
		if (string_from_view(file, param.type.lexeme) != "int") {
			printlns("Don't support anything else than intsry :(.");
			return nullptr;
		}

		f.parameter_names.push_back(string_view_from_view(file, param.name.lexeme));
		f.parameters[string_view_from_view(file, param.name.lexeme)];
	}

	for (size_t idx = node.return_list_idx; idx; idx = nodes[idx]->next_statement) {
		auto& ret = nodes[idx].Return_Parameter_;
		f.return_types.push_back(string_from_view(file, ret.type.lexeme));
	}

	f.start_idx = node.statement_list_idx;

	return f;
}

std::any Interpreter::function_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Function_Call_;

	auto id = lookup(string_view_from_view(file, node.identifier.lexeme));
	if (typecheck<Builtin>(id)) {
		std::vector<std::any> args;
		for (size_t idx = node.argument_list_idx, i = 0; idx; idx = nodes[idx]->next_statement, i++) {
			auto& param = nodes[idx].Argument_;
			args.push_back(interpret(nodes, param.value_idx, file));
		}
		return std::any_cast<Builtin>(id).f(std::move(args));
	}
	if (!typecheck<Function_Definition>(id)) return nullptr;

	auto f = std::any_cast<Function_Definition>(&id);
	f->parameters.clear();
	
	for (size_t idx = node.argument_list_idx, i = 0; idx; idx = nodes[idx]->next_statement, i++) {
		auto& param = nodes[idx].Argument_;
		f->parameters[f->parameter_names[i]] = interpret(nodes, param.value_idx, file);
	}

	return interpret(nodes, *f, file);
}

std::any Interpreter::return_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Return_Call_;

	Return_Call r;
	if (node.return_value_idx) {
		r.values.push_back(interpret(nodes, node.return_value_idx, file));
	}

	return r;
}


std::any Interpreter::identifier(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Identifier_;

	return lookup(string_view_from_view(file, node.token.lexeme));
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

	variables.back()[name] = interpret(nodes, node.value_idx, file);

	return variables.back()[name];
}

std::any Interpreter::litteral(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Litteral_;
	auto view = node.token.lexeme;

	if (node.token.type == Token::Type::Number) {
		// When will libc++ and libstdc++ implement from_char :'(
/*
		long double x = 0;
		auto res = std::from_chars(file.data() + view.i, file.data() + view.i + view.size, x);
		if (res.ec == std::errc::invalid_argument) return nullptr;
*/
		static std::string temp;
		temp.clear();
		temp.resize(view.size);
		memcpy(temp.data(), file.data() + view.i, view.size);
		char* end_ptr;
		long double x = std::strtold(temp.c_str(), &end_ptr);
		return x;
	}

	if (node.token.type == Token::Type::String) {
		return std::string(file.data() + view.i + 1, view.size - 2);
	}
	
	return nullptr;
}

std::any Interpreter::expression(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	return interpret(nodes, nodes[idx].Expression_.inner_idx, file);
}

void Interpreter::print_value(const std::any& value) noexcept {
	if (typecheck<std::nullptr_t>(value)) {
		printlns("RIP F in the chat for my boiii");
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

std::any Interpreter::lookup(std::string_view id) noexcept {
	for (size_t i = variables.size() - 1; i + 1 > limit_scope.top(); --i)
		for (auto& [x, v] : variables[i]) if (x == id) return v;
	println("Can not find variable named %.*s.", (int)id.size(), id.data());
	return nullptr;
}

void Interpreter::push_scope() noexcept { variables.emplace_back(); }
void Interpreter::pop_scope()  noexcept { variables.pop_back(); }

void Interpreter::push_builtin() noexcept {
	Builtin print;
	print.f = [&] (std::vector<std::any> values) -> std::any {
		for (auto& x : values) {
			if (typecheck<long double>(x)) printf("%Lf", std::any_cast<long double>(x));
			else if (typecheck<std::string>(x)) printf("%s", std::any_cast<std::string>(x).c_str());
			else printf("Unsupported type to print (%s)", x.type().name());

			printf(" ");
		}
		printf("\n");

		return nullptr;
	};
	variables.back()["print"] = print;
}
