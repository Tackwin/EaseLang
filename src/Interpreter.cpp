#include "Interpreter.hpp"
#include "AST.hpp"

#include "xstd.hpp"

#include <charconv>
#include <thread>


template<typename T>
T cast(const std::any& x) { return std::any_cast<T>(x); }

std::any AST_Interpreter::interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx];
	switch (node.kind) {
	case AST::Node::Identifier_Kind:          return identifier   (nodes, idx, file);
	case AST::Node::Type_Identifier_Kind:     return type_ident   (nodes, idx, file);
	case AST::Node::Assignement_Kind:         return assignement  (nodes, idx, file);
	case AST::Node::Litteral_Kind:            return litteral     (nodes, idx, file);
	case AST::Node::Operation_List_Kind:      return list_op      (nodes, idx, file);
	case AST::Node::Unary_Operation_Kind:     return unary_op     (nodes, idx, file);
	case AST::Node::Expression_Kind:          return expression   (nodes, idx, file);
	case AST::Node::Function_Definition_Kind: return function     (nodes, idx, file);
	case AST::Node::If_Kind:                  return if_call      (nodes, idx, file);
	case AST::Node::For_Kind:                 return for_loop     (nodes, idx, file);
	case AST::Node::While_Kind:               return while_loop   (nodes, idx, file);
	case AST::Node::Function_Call_Kind:       return function_call(nodes, idx, file);
	case AST::Node::Return_Call_Kind:         return return_call  (nodes, idx, file);
	case AST::Node::Struct_Definition_Kind:   return struct_def   (nodes, idx, file);
	case AST::Node::Initializer_List_Kind:    return init_list    (nodes, idx, file);
	case AST::Node::Array_Access_Kind:        return array_access (nodes, idx, file);
	default:                        return nullptr;
	}
}

std::any AST_Interpreter::interpret(
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

	return r.values.empty() ? nullptr : at(r.values.front());
}

std::any AST_Interpreter::init_list(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Initializer_List_;

	if (node.type_identifier) {
		auto type_name = string_view_from_view(
			file, nodes[*node.type_identifier].Type_Identifier_.identifier.lexeme
		);
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
		) user_struct.member_values[i] = alloc(interpret(nodes, idx, file));

		return user_struct;
	}
	printlns("Please specify the type explicitely.");

	return nullptr;
}

std::any AST_Interpreter::unary_op(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Unary_Operation_;

	switch (node.op) {
		case AST::Operator::Minus: {
			auto x = interpret(nodes, node.right_idx, file);
			if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
			if (typecheck<long double>(x)) {
				println("Type error, expected long double got %s", x.type().name());
				return nullptr;
			}
			return -std::any_cast<long double>(x);
		}
		case AST::Operator::Plus: {
			auto x = interpret(nodes, node.right_idx, file);
			if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
			if (typecheck<long double>(x)) {
				println("Type error, expected long double got %s", x.type().name());
				return nullptr;
			}
			return +std::any_cast<long double>(x);
		}
		case AST::Operator::Inc: {
			auto x = interpret(nodes, node.right_idx, file);
			if (!typecheck<Identifier>(x)) {
				println("Type error, expected identifier got %s", x.type().name());
				return nullptr;
			}

			if (!typecheck<long double>(at(cast<Identifier>(x))))  {
				println("Type error, expected long double got %s", x.type().name());
				return nullptr;
			}

			memory[cast<Identifier>(x).memory_idx] = 1 + cast<long double>(at(cast<Identifier>(x)));

			return cast<long double>(at(cast<Identifier>(x))) + 1;
		}
		case AST::Operator::Amp: {
			auto x = interpret(nodes, node.right_idx, file);
			if (!typecheck<Identifier>(x)) {
				println("Type error, expected Identifier got %s", x.type().name());
				return nullptr;
			}

			return Pointer{ cast<Identifier>(x).memory_idx };
		}
		case AST::Operator::Star: {
			auto x = interpret(nodes, node.right_idx, file);
			if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
			if (!typecheck<Pointer>(x)) {
				println("Type error, expected Pointer got %s", x.type().name());
				return nullptr;
			}

			return at(cast<Pointer>(x));
		}
		case AST::Operator::Not:
		default: return nullptr;
	}
}

std::any AST_Interpreter::list_op(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Operation_List_;
	switch(node.op) {
		case AST::Operator::Gt: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);
			
			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));

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
		case AST::Operator::Eq: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);
			
			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));

			if (!typecheck<long double>(left)) {
				println("Error expected long double for the lhs, got %s", left.type().name());
				return nullptr;
			}
			if (!typecheck<long double>(right)) {
				println("Error expected long double for the rhs, got %s", right.type().name());
				return nullptr;
			}

			return std::any_cast<long double>(left) == std::any_cast<long double>(right);
		}
		case AST::Operator::Neq: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);
			
			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));

			if (!typecheck<long double>(left)) {
				println("Error expected long double for the lhs, got %s", left.type().name());
				return nullptr;
			}
			if (!typecheck<long double>(right)) {
				println("Error expected long double for the rhs, got %s", right.type().name());
				return nullptr;
			}

			return std::any_cast<long double>(left) != std::any_cast<long double>(right);
		}
		case AST::Operator::Lt: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));
			
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
		case AST::Operator::Assign: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));
			
			if (!typecheck<Identifier>(left)) {
				println("Error expected Identifier for the lhs, got %s", left.type().name());
				return nullptr;
			}
			if (!typecheck<long double>(right)) {
				println("Error expected long double for the rhs, got %s", right.type().name());
				return nullptr;
			}

			memory[cast<Identifier>(left).memory_idx] = right;

			return right;
		}
		case AST::Operator::Leq: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));
			
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
		case AST::Operator::Plus: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));
			
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
		case AST::Operator::Star: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));
			
			if (!typecheck<long double>(left)) {
				println("Error expected long double for the lhs, got %s", left.type().name());
				return nullptr;
			}
			if (!typecheck<long double>(right)) {
				println("Error expected long double for the rhs, got %s", right.type().name());
				return nullptr;
			}

			return std::any_cast<long double>(left) * std::any_cast<long double>(right);
		}
		case AST::Operator::Div: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));
			
			if (!typecheck<long double>(left)) {
				println("Error expected long double for the lhs, got %s", left.type().name());
				return nullptr;
			}
			if (!typecheck<long double>(right)) {
				println("Error expected long double for the rhs, got %s", right.type().name());
				return nullptr;
			}

			return std::any_cast<long double>(left) / std::any_cast<long double>(right);
		}
		case AST::Operator::Mod: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));
			
			if (!typecheck<long double>(left)) {
				println("Error expected long double for the lhs, got %s", left.type().name());
				return nullptr;
			}
			if (!typecheck<long double>(right)) {
				println("Error expected long double for the rhs, got %s", right.type().name());
				return nullptr;
			}

			return std::fmodl(std::any_cast<long double>(left), std::any_cast<long double>(right));
		}
		case AST::Operator::Minus: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (typecheck<Identifier>(left )) left  = at(cast<Identifier>(left ));
			if (typecheck<Identifier>(right)) right = at(cast<Identifier>(right));
			
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
		case AST::Operator::Dot: {
			auto root_struct = interpret(nodes, node.left_idx, file);
			if (typecheck<Identifier>(root_struct)) root_struct = at(cast<Identifier>(root_struct));
			if (typecheck<Pointer   >(root_struct)) root_struct = at(cast<Pointer   >(root_struct));
			if (!typecheck<User_Struct>(root_struct)) {
				println("Expected a struct but got %s instead.", root_struct.type().name());
				return nullptr;
			}

			auto helper =
			[&] (const User_Struct& user_struct, size_t next, auto& helper) -> std::any {
				auto& next_node = nodes[next];
				if (next_node.kind != AST::Node::Identifier_Kind) {
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

				auto x = at(user_struct.member_values[i]);
				if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
				if (typecheck<Pointer   >(x)) x = at(cast<Pointer   >(x));
				if (typecheck<User_Struct>(x)) {
					return helper(
						std::any_cast<User_Struct>(x), next_node.Identifier_.next_statement, helper
					);
				}

				return x;

			};

			return helper(std::any_cast<User_Struct>(root_struct), node.rest_idx, helper);
		}
		default:{
			println("Unsupported operation %s", AST::op_to_string(node.op));
			return nullptr;
		}
	}
}

std::any AST_Interpreter::if_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].If_;

	auto cond = interpret(nodes, node.condition_idx, file);
	if (typecheck<Identifier>(cond)) cond = at(cast<Identifier>(cond));
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

std::any AST_Interpreter::for_loop(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].For_;

	push_scope();
	defer { pop_scope(); };

	interpret(nodes, node.init_statement_idx, file);

	while (true) {
		auto cond = interpret(nodes, node.cond_statement_idx, file);
		if (typecheck<Identifier>(cond)) cond = at(cast<Identifier>(cond));
		if (!typecheck<bool>(cond)) {
			println("Expected bool on the for-condition got %s.", cond.type().name());
			return nullptr;
		}

		if (!cast<bool>(cond)) break;

		for (size_t idx = node.loop_statement_idx; idx; idx = nodes[idx]->next_statement) {
			auto v = interpret(nodes, idx, file);
			if (typecheck<Return_Call>(v)) return v;
		}

		interpret(nodes, node.next_statement_idx, file);
	}

	return nullptr;
}


std::any AST_Interpreter::while_loop(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].While_;

	push_scope();
	defer { pop_scope(); };

	while (true) {
		auto cond = interpret(nodes, node.cond_statement_idx, file);
		if (typecheck<Identifier>(cond)) cond = at(cast<Identifier>(cond));
		if (!typecheck<bool>(cond)) {
			println("Expected bool on the while-condition got %s.", cond.type().name());
			return nullptr;
		}

		if (!cast<bool>(cond)) break;

		for (size_t idx = node.loop_statement_idx; idx; idx = nodes[idx]->next_statement) {
			auto v = interpret(nodes, idx, file);
			if (typecheck<Return_Call>(v)) return v;
		}
	}

	return nullptr;
}

std::any AST_Interpreter::struct_def(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Struct_Definition_;

	User_Struct x;
	for (size_t idx = node.struct_line_idx; idx; idx = nodes[idx]->next_statement) {
		auto& def = nodes[idx].Assignement_;
		auto name = string_from_view(file, def.identifier.lexeme);
		x.member_names.push_back(name);
		x.member_values.push_back(alloc(interpret(nodes, def.value_idx, file)));
	}

	return x;
}

std::any AST_Interpreter::function(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Function_Definition_;

	Function_Definition f;

	for (size_t idx = node.parameter_list_idx; idx; idx = nodes[idx]->next_statement) {
		auto& param = nodes[idx].Parameter_;
		
		if (string_from_view(
				file, nodes[param.type_identifier].Type_Identifier_.identifier.lexeme
			) != "int"
		) {
			printlns("Don't support anything else than intsry :(.");
			return nullptr;
		}

		f.parameter_names.push_back(string_view_from_view(file, param.name.lexeme));
		f.parameters[string_view_from_view(file, param.name.lexeme)];
	}

	for (size_t idx = node.return_list_idx; idx; idx = nodes[idx]->next_statement) {
		auto& ret = nodes[idx].Return_Parameter_;
		auto type_name =
			string_from_view(file, nodes[ret.type_identifier].Type_Identifier_.identifier.lexeme);
		f.return_types.push_back(type_name);
	}

	f.start_idx = node.statement_list_idx;

	return f;
}

std::any AST_Interpreter::function_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Function_Call_;

	auto id = interpret(nodes, node.identifier_idx, file);
	if (typecheck<Identifier>(id)) id = at(cast<Identifier>(id));

	if (typecheck<Builtin>(id)) {
		std::vector<size_t> args;
		for (size_t idx = node.argument_list_idx, i = 0; idx; idx = nodes[idx]->next_statement, i++)
		{
			auto& param = nodes[idx].Argument_;
			auto x = interpret(nodes, param.value_idx, file);
			if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
			args.push_back(alloc(x));
		}
		return at(std::any_cast<Builtin>(id).f(std::move(args)));
	}
	if (!typecheck<Function_Definition>(id)) return nullptr;

	auto f = std::any_cast<Function_Definition>(&id);
	f->parameters.clear();
	
	for (size_t idx = node.argument_list_idx, i = 0; idx; idx = nodes[idx]->next_statement, i++) {
		auto& param = nodes[idx].Argument_;
		auto x = interpret(nodes, param.value_idx, file);
		if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
		f->parameters[f->parameter_names[i]] = alloc(x);
	}

	return interpret(nodes, *f, file);
}

std::any AST_Interpreter::array_access(AST_Nodes nodes, size_t idx, std::string_view file) noexcept
{
	auto& node = nodes[idx].Array_Access_;

	auto id = interpret(nodes, node.identifier_array_idx, file);
	if (typecheck<Identifier>(id)) id = at(cast<Identifier>(id));
	if (!typecheck<Array>(id)) {
		println("Error expected Array got %s.", id.type().name());
		return nullptr;
	}

	auto access_id = interpret(nodes, node.identifier_acess_idx, file);
	if (typecheck<Identifier>(access_id)) access_id = at(cast<Identifier>(access_id));
	if (!typecheck<long double>(access_id)) {
		println("Error expected long double got %s.", access_id.type().name());
		return nullptr;
	}

	return at(cast<Array>(id).values[std::roundl(cast<long double>(access_id))]);
}

std::any AST_Interpreter::return_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Return_Call_;

	Return_Call r;
	if (node.return_value_idx) {
		auto x = interpret(nodes, node.return_value_idx, file);
		if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
		r.values.push_back(alloc(x));
	}

	return r;
}


std::any AST_Interpreter::identifier(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Identifier_;

	return Identifier{ lookup_addr(string_view_from_view(file, node.token.lexeme)) };
}

std::any AST_Interpreter::type_ident(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Type_Identifier_;

	return lookup(string_view_from_view(file, node.identifier.lexeme));
}


std::any AST_Interpreter::assignement(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Assignement_;

	// >TODO(Tackwin): handle type info.
	if (node.type_identifier) {
	}

	auto name = string_from_view(file, node.identifier.lexeme);

	if (variables.empty()) variables.push_back({});
	if (variables.back().count(name) > 0) {
		println("Error variable %s already assigned.", name.c_str());
		return nullptr;
	}

	auto x = interpret(nodes, node.value_idx, file);
	if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
	variables.back()[name] = alloc(x);

	return at(variables.back()[name]);
}

std::any AST_Interpreter::litteral(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
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

std::any AST_Interpreter::expression(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	return interpret(nodes, nodes[idx].Expression_.inner_idx, file);
}

void AST_Interpreter::print_value(const std::any& value) noexcept {
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

	if (typecheck<Identifier>(value)) {
		auto x = std::any_cast<Identifier>(value);
		print_value(at(x));
		return;
	}
}

std::any AST_Interpreter::lookup(std::string_view id) noexcept {
	for (size_t i = variables.size() - 1; i + 1 > limit_scope.top(); --i)
		for (auto& [x, v] : variables[i]) if (x == id) return at(v);
	println("Can not find variable named %.*s.", (int)id.size(), id.data());
	return nullptr;
}
size_t AST_Interpreter::lookup_addr(std::string_view id) noexcept {
	for (size_t i = variables.size() - 1; i + 1 > limit_scope.top(); --i)
		for (auto& [x, v] : variables[i]) if (x == id) return v;
	println("Can not find variable named %.*s.", (int)id.size(), id.data());
	return 0;
}

void AST_Interpreter::push_scope() noexcept { variables.emplace_back(); }
void AST_Interpreter::pop_scope()  noexcept { variables.pop_back(); }

void AST_Interpreter::push_builtin() noexcept {
	Builtin print;
	print.f = [&] (std::vector<size_t> values) -> size_t {
		for (auto& x : values) {
			auto y = at(x);
			if (typecheck<Identifier>(y)) y = at(cast<Identifier>(y));
			if (typecheck<Pointer>(y)) printf("%zu", cast<Pointer>(y).memory_idx);
			else if (typecheck<long double>(y)) printf("%Lf", std::any_cast<long double>(y));
			else if (typecheck<std::string>(y)) printf("%s", std::any_cast<std::string>(y).c_str());
			else printf("Unsupported type to print (%s)", y.type().name());

			printf(" ");
		}
		printf("\n");

		return 0;
	};
	variables.back()["print"] = alloc(print);

	Builtin sleep;
	sleep.f = [&] (std::vector<size_t> values) -> size_t {
		if (values.size() != 1) {
			println("Sleep expect 1 long double argument got %zu arguments.", values.size());
			return 0;
		}
		auto x = at(values.front());
		if (!typecheck<long double>(x)) {
			println("Sleep expect 1 long double argument got %s.", x.type().name());
			return 0;
		}

		std::this_thread::sleep_for(
			std::chrono::nanoseconds((size_t)(1'000'000'000 * cast<long double>(x)))
		);
		return 0;
	};
	variables.back()["sleep"] = alloc(sleep);

	Builtin len;
	len.f = [&] (std::vector<size_t> values) -> size_t {
		if (values.size() != 1) {
			println("len expect 1 Array argument, got %zu arguments.", values.size());
			return 0;
		}

		auto x = at(values.front());
		if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
		if (!typecheck<Array>(x)) {
			println("Len expect a array argument, got %s.", x.type().name());
			return 0;
		}

		return alloc(cast<Array>(x).values.size());
	};
	variables.back()["len"] = alloc(len);
}


size_t AST_Interpreter::alloc(std::any x) noexcept {
	memory.push_back(std::move(x));
	return memory.size() - 1;
}

std::any AST_Interpreter::at(size_t idx) noexcept {
	if (memory.size() <= idx) {
		println(
			"Tried to access memory %zu. Is out of bounds, memory have size %zu.",
			idx,
			memory.size()
		);
		return nullptr;
	}
	return memory[idx];
}
