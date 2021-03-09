#include "Interpreter.hpp"
#include "AST.hpp"

#include "xstd.hpp"

#include <charconv>
#include <thread>


template<typename T>
bool typecheck(const std::any& value) noexcept { return value.type() == typeid(std::decay_t<T>); }

template<typename T>
T cast(const std::any& x) noexcept { return std::any_cast<T>(x); }

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
	AST_Nodes nodes, const User_Function_Type& f, std::string_view file
) noexcept {
	std::any v;
	for (size_t idx = f.start_idx; idx; idx = nodes[idx]->next_statement) {
		v = interpret(nodes, idx, file);
		if (typecheck<Return_Call>(v)) break;
	}

	if (!typecheck<Return_Call>(v) && !f.return_type.empty()) {
		printlns("Reached end of non void returning function.");
		return nullptr;
	}
	if (!typecheck<Return_Call>(v)) return Void();
	auto r = std::any_cast<Return_Call>(v);
	if (r.values.size() != f.return_type.size()) {
		println(
			"Trying to return from a function with wrong number of return parameters, "
			"expected %zu got %zu.", f.return_type.size(), r.values.size()
		);
		return nullptr;
	}

	return r.values.empty() ? nullptr : at(r.values.front());
}

std::any AST_Interpreter::init_list(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Initializer_List_;

	if (node.type_identifier) {
		auto any_type = interpret(nodes, *node.type_identifier, file);
		if (!typecheck<Type>(any_type)) {
			println("Error expected User_Type_Descriptor got %s.", any_type.type().name());
			return nullptr;
		}

		auto type = cast<Type>(any_type);

		Identifier new_identifier;
		new_identifier.memory_idx = alloc(type.get_size());
		new_identifier.type_descriptor_id = type.get_unique_id();

		switch (type.kind) {
			case Type::Real_Kind:
				copy(
					interpret(nodes, node.expression_list_idx, file), new_identifier.memory_idx
				);
				break;
			case Type::Array_Type_Kind:
				for (
					size_t idx = node.expression_list_idx, i = 0;
					idx;
					idx = nodes[idx]->next_statement, i++
				) {
					auto underlying_type = types[type.Array_Type_.user_type_descriptor_idx];
					copy(
						interpret(nodes, idx, file),
						new_identifier.memory_idx + i * underlying_type.get_size()
					);
				}
				break;
			case Type::User_Type_Descriptor_Kind: {
				auto& user_struct = type.User_Type_Descriptor_;

				size_t off = new_identifier.memory_idx;
				for (
					size_t idx = node.expression_list_idx, i = 0;
					idx;
					idx = nodes[idx]->next_statement, i++
				) {
					auto v = interpret(nodes, idx, file);
					copy(v, off);
					off += types[user_struct.member_types[i]].get_size();
				}
				break;
			}
			default:
				printlns("TODO");
				break;
		}
		return new_identifier;
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
			return -cast<long double>(x);
		}
		case AST::Operator::Plus: {
			auto x = interpret(nodes, node.right_idx, file);
			if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
			if (typecheck<long double>(x)) {
				println("Type error, expected long double got %s", x.type().name());
				return nullptr;
			}
			return +cast<long double>(x);
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

			auto id = cast<Identifier>(x);
			long double v;
			memcpy(&v, memory.data() + id.memory_idx, sizeof(long double));
			v++;
			memcpy(memory.data() + id.memory_idx, &v, sizeof(long double));

			return cast<Identifier>(x);
		}
		case AST::Operator::Amp: {
			auto x = interpret(nodes, node.right_idx, file);
			if (!typecheck<Identifier>(x)) {
				println("Type error, expected Identifier got %s", x.type().name());
				return nullptr;
			}

			Pointer p;
			p.memory_idx = cast<Identifier>(x).memory_idx;
			p.type_descriptor_id = cast<Identifier>(x).type_descriptor_id;
			return p;
		}
		case AST::Operator::Star: {
			auto x = interpret(nodes, node.right_idx, file);
			if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
			if (!typecheck<Pointer>(x)) {
				println("Type error, expected Pointer got %s", x.type().name());
				return nullptr;
			}

			Identifier id;
			id.memory_idx = cast<Pointer>(x).memory_idx;
			id.type_descriptor_id = cast<Pointer>(x).type_descriptor_id;
			return id;
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

			return cast<long double>(left) > cast<long double>(right);
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

			return cast<long double>(left) == cast<long double>(right);
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

			return cast<long double>(left) != cast<long double>(right);
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

			return cast<long double>(left) < cast<long double>(right);
		}
		case AST::Operator::Assign: {
			auto left = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (!typecheck<Identifier>(left)) {
				println("Error expected Identifier for the lhs, got %s", left.type().name());
				return nullptr;
			}
			if (typecheck<Identifier>(right)) {
				if (
					cast<Identifier>(left).type_descriptor_id !=
					cast<Identifier>(right).type_descriptor_id
				) {
					printlns("Mismatch between left and right type.");
					return nullptr;
				}

				copy(cast<Identifier>(right), cast<Identifier>(left).memory_idx);
			} else if (typecheck<long double>(right)) {
				if (cast<Identifier>(left).type_descriptor_id != Real::unique_id) {
					printlns("Mismatch between left and right type.");
					return nullptr;
				}

				auto x = cast<long double>(right);
				memcpy(memory.data() + cast<Identifier>(right).memory_idx, &x, sizeof(long double));
			}

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

			return cast<long double>(left) <= cast<long double>(right);
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

			return cast<long double>(left) + cast<long double>(right);
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

			return cast<long double>(left) * cast<long double>(right);
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

			return cast<long double>(left) / cast<long double>(right);
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

			return std::fmodl(cast<long double>(left), cast<long double>(right));
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

			return cast<long double>(left) - cast<long double>(right);
		}
		case AST::Operator::Dot: {
			auto root_struct = interpret(nodes, node.left_idx, file);
			if ( typecheck<Pointer   >(root_struct)) root_struct = at(cast<Pointer>(root_struct));
			if (!typecheck<Identifier>(root_struct)) {
				println("Expected an Identifier but got %s instead.", root_struct.type().name());
				return nullptr;
			}

			auto helper = [&] (Identifier user_struct, size_t next, auto& helper) -> std::any {
				if (!next) return user_struct;

				auto& next_node = nodes[next].Identifier_;
				auto name = string_from_view(file, next_node.token.lexeme);
				auto type = types.at(user_struct.type_descriptor_id);

				switch (type.kind) {
					case Type::Pointer_Type_Kind: {
						Identifier id;
						id.memory_idx = cast<size_t>(at(user_struct));
						id.type_descriptor_id = type.Pointer_Type_.user_type_descriptor_idx;

						return helper(id, next_node.next_statement, helper);
						break;
					}
					case Type::User_Type_Descriptor_Kind: {
						Identifier id;
						id.memory_idx =
							user_struct.memory_idx + type.User_Type_Descriptor_.member_offsets[idx];
						id.type_descriptor_id = type.User_Type_Descriptor_.member_types[idx];

						return helper(id, next_node.next_statement, helper);
					}
					default:
						printlns("Should not happen.");
						return nullptr;
				}
			};

			return helper(cast<Identifier>(root_struct), node.rest_idx, helper);
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

	User_Type_Descriptor desc;

	size_t running_offset = 0;
	for (size_t idx = node.struct_line_idx; idx; idx = nodes[idx]->next_statement) {
		auto& def = nodes[idx].Assignement_;
		auto name = string_from_view(file, def.identifier.lexeme);

		desc.name_to_idx[name] = desc.member_types.size();
		Identifier new_member;

		if (def.type_identifier) {
			desc.member_offsets.push_back(running_offset);
			desc.member_types.push_back(
				cast<Type>(interpret(nodes, *def.type_identifier, file)).get_unique_id()
			);
			running_offset += types.at(new_member.type_descriptor_id).get_size();
		}

		desc.default_values.push_back(nullptr);
		if (def.value_idx) desc.default_values.back() = interpret(nodes, def.value_idx, file);
	}
	desc.byte_size = running_offset;
	desc.unique_id = Type_N++;

	types[desc.unique_id] = desc;

	return desc;
}

std::any AST_Interpreter::function(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Function_Definition_;

	User_Function_Type f;
	f.start_idx = node.statement_list_idx;

	for (size_t idx = node.parameter_list_idx; idx; idx = nodes[idx]->next_statement) {
		auto& param = nodes[idx].Parameter_;

		auto type = type_ident(nodes, param.type_identifier, file);
		f.parameter_type.push_back(type.get_unique_id());
		f.parameter_name.push_back(string_view_from_view(file, param.name.lexeme));
	}

	for (size_t idx = node.return_list_idx; idx; idx = nodes[idx]->next_statement) {
		auto& ret = nodes[idx].Return_Parameter_;
		auto type = type_ident(nodes, ret.type_identifier, file);
		f.return_type.push_back(type.get_unique_id());
	}

	f.unique_id = Type_N++;

	types[f.unique_id] = f;

	return f;
}

std::any AST_Interpreter::function_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept
{
	auto& node = nodes[idx].Function_Call_;
	std::vector<Identifier> arguments;
	for (size_t idx = node.argument_list_idx, i = 0; idx; idx = nodes[idx]->next_statement, i++) {
		auto& param = nodes[idx].Argument_;
		auto x = interpret(nodes, param.value_idx, file);
		Identifier new_ident;
		if (typecheck<long double>(x)) {
			new_ident.type_descriptor_id = Real::unique_id;
			new_ident.memory_idx = alloc(sizeof(long double));
		}
		if (typecheck<Pointer>(x)) {
			new_ident.type_descriptor_id = cast<Pointer>(x).type_descriptor_id;
			new_ident.memory_idx = alloc(sizeof(size_t));
		}
		if (typecheck<Identifier>(x)) {
			new_ident.type_descriptor_id = cast<Identifier>(x).type_descriptor_id;
			new_ident.memory_idx = alloc(types.at(new_ident.type_descriptor_id).get_size());
		}
		copy(x, new_ident.memory_idx);
		arguments.push_back(new_ident);
	}

	auto any_id = interpret(nodes, node.identifier_idx, file);
	if (typecheck<Builtin>(any_id)) {
		return cast<Builtin>(any_id).f(arguments);
	}
	auto id = cast<Identifier>(any_id);

	push_scope();
	defer { pop_scope(); };

	auto f = &types.at(id.type_descriptor_id).User_Function_Type_;
	for (size_t i = 0; i < arguments.size(); ++i) {
		auto name = f->parameter_name[i];
		variables.back()[(std::string)name] = arguments[i];
	}

	return interpret(nodes, *f, file);
}

std::any AST_Interpreter::array_access(AST_Nodes nodes, size_t idx, std::string_view file) noexcept
{
	auto& node = nodes[idx].Array_Access_;

	auto id = interpret(nodes, node.identifier_array_idx, file);
	if (!typecheck<Identifier>(id)) {
		println("Error expected Identifier got %s.", id.type().name());
		return nullptr;
	}

	auto access_id = interpret(nodes, node.identifier_acess_idx, file);
	if (typecheck<Identifier>(access_id)) access_id = at(cast<Identifier>(access_id));
	if (!typecheck<long double>(access_id)) {
		println("Error expected long double got %s.", access_id.type().name());
		return nullptr;
	}

	size_t i = (size_t)std::roundl(cast<long double>(access_id));
	size_t size = types.at(cast<Identifier>(id).type_descriptor_id).get_size();

	Identifier identifier;
	identifier.memory_idx = cast<Identifier>(id).memory_idx + i * size;
	identifier.type_descriptor_id = cast<Identifier>(id).type_descriptor_id;

	return identifier;
}

std::any AST_Interpreter::return_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Return_Call_;

	Return_Call r;
	if (node.return_value_idx) {
		auto x = interpret(nodes, node.return_value_idx, file);

		Identifier new_ident;

		if (typecheck<long double>(x)) {
			new_ident.memory_idx = alloc(sizeof(long double));
			new_ident.type_descriptor_id = Real::unique_id;
		}
		if (typecheck<Pointer>(x)) {
			new_ident.memory_idx = alloc(sizeof(size_t));
			new_ident.type_descriptor_id = cast<Pointer>(x).type_descriptor_id;
		}
		if (typecheck<Identifier>(x)) {
			new_ident.memory_idx =
				alloc(types.at(cast<Identifier>(x).type_descriptor_id).get_size());
			new_ident.type_descriptor_id = cast<Identifier>(x).type_descriptor_id;
		}

		copy(x, new_ident.memory_idx);

		r.values.push_back(new_ident);
	}

	return r;
}


std::any AST_Interpreter::identifier(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Identifier_;

	return lookup(string_view_from_view(file, node.token.lexeme));
}

AST_Interpreter::Type AST_Interpreter::type_ident(
	AST_Nodes nodes, size_t idx, std::string_view file
) noexcept {
	auto& node = nodes[idx].Type_Identifier_;

	return type_lookup(string_view_from_view(file, node.identifier.lexeme));
}


std::any AST_Interpreter::assignement(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Assignement_;

	// >TODO(Tackwin): handle type info.
	if (node.type_identifier) {
	}

	auto name = string_from_view(file, node.identifier.lexeme);

	if (variables.back().count(name) > 0) {
		println("Error variable %s already assigned.", name.c_str());
		return nullptr;
	}
	variables.back()[name];

	auto x = interpret(nodes, node.value_idx, file);
	if (typecheck<User_Function_Type>(x)) {
		variables.back()[name].memory_idx = alloc(sizeof(size_t));
		variables.back()[name].type_descriptor_id = cast<User_Function_Type>(x).unique_id;

		copy(cast<User_Function_Type>(x).start_idx, variables.back()[name].memory_idx);
	}
	if (typecheck<User_Type_Descriptor>(x)) {
		auto hash = cast<User_Type_Descriptor>(x).unique_id;
		type_name_to_hash[name] = hash;
		types[hash] = cast<User_Type_Descriptor>(x);
		return types[hash];
	}
	if (typecheck<long double>(x)) {
		variables.back()[name].memory_idx = alloc(sizeof(long double));
		variables.back()[name].type_descriptor_id = Real::unique_id;

		copy(x, variables.back()[name].memory_idx);
	}
	if (typecheck<Identifier>(x)) {
		auto id = cast<Identifier>(x);
		variables.back()[name].memory_idx = alloc(types[id.type_descriptor_id].get_size());
		variables.back()[name].type_descriptor_id = id.type_descriptor_id;
		copy(cast<Identifier>(x), variables.back()[name].memory_idx);
	}

	return variables.back()[name];
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
		if (x.type_descriptor_id != Void::unique_id) print_value(at(x));
		return;
	}
}

std::any AST_Interpreter::lookup(std::string_view id) noexcept {
	for (size_t i = variables.size() - 1; i + 1 > 0; --i)
		for (auto& [x, v] : variables[i]) if (x == id) return v;
	for (auto& [x, v] : builtins) if (x == id) return v;
	println("Can not find variable named %.*s.", (int)id.size(), id.data());
	return nullptr;
}
size_t AST_Interpreter::lookup_addr(std::string_view id) noexcept {
	for (size_t i = variables.size() - 1; i + 1 > 0; --i)
		for (auto& [x, v] : variables[i]) if (x == id) return v.memory_idx;
	println("Can not find variable named %.*s.", (int)id.size(), id.data());
	return 0;
}

void AST_Interpreter::push_scope() noexcept { variables.emplace_back(); }
void AST_Interpreter::pop_scope()  noexcept { variables.pop_back(); }

void AST_Interpreter::push_builtin() noexcept {
	Builtin print;
	print.f = [&] (std::vector<Identifier> values) -> Identifier {
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

		return {};
	};
	builtins["print"] = print;

	Builtin sleep;
	sleep.f = [&] (std::vector<Identifier> values) -> Identifier {
		if (values.size() != 1) {
			println("Sleep expect 1 long double argument got %zu arguments.", values.size());
			return {};
		}
		auto x = at(values.front());
		if (!typecheck<long double>(x)) {
			println("Sleep expect 1 long double argument got %s.", x.type().name());
			return {};
		}

		std::this_thread::sleep_for(
			std::chrono::nanoseconds((size_t)(1'000'000'000 * cast<long double>(x)))
		);
		return {};
	};
	builtins["sleep"] = sleep;

	type_name_to_hash["int"] = Int::unique_id;
	type_name_to_hash["void"] = Void::unique_id;
	type_name_to_hash["real"] = Real::unique_id;
	types[Void::unique_id] = Void();
	types[Real::unique_id] = Real();
	types[Int::unique_id] = Int();
	types[Pointer_Type::unique_id] = Pointer_Type();
	types[Array_Type::unique_id] = Array_Type();

	//Builtin len;
	//len.f = [&] (std::vector<Identifier> values) -> Identifier {
	//	if (values.size() != 1) {
	//		println("len expect 1 Array argument, got %zu arguments.", values.size());
	//		return {};
	//	}
//
	//	auto x = at(values.front());
	//	if (typecheck<Identifier>(x)) x = at(cast<Identifier>(x));
	//	if (!typecheck<Array>(x)) {
	//		println("Len expect a array argument, got %s.", x.type().name());
	//		return 0;
	//	}
//
	//	Identifier y;
	//	y.memory_idx = alloc(sizeof(long double));
	//	y.type_descriptor_id = Real::unique_id;
//
	//	//return alloc(cast<Array>(x).values.size());
	//};
	//builtins["len"] = len;
}


AST_Interpreter::Type AST_Interpreter::type_lookup(std::string_view id) noexcept {
	std::string huuuuugh(id.data(), id.size());
	return types.at(type_name_to_hash.at(huuuuugh));
}

size_t AST_Interpreter::copy(std::any from, size_t to) noexcept {
	if (typecheck<long double>(from)) {
		auto x = cast<long double>(from);
		*reinterpret_cast<long double*>(memory.data() + to) = x;
		return sizeof(long double);
	}
	if (typecheck<Pointer>(from)) {
		auto ptr = cast<Pointer>(from).memory_idx;
		memcpy(memory.data() + to, &ptr, sizeof(size_t));
		return sizeof(size_t);
	}
	if (typecheck<Identifier>(from)) {
		auto id = cast<Identifier>(from);
		if (types.at(id.type_descriptor_id).kind != Type::User_Type_Descriptor_Kind) {
			return copy(at(id), to);
		}

		auto user_struct = types.at(id.type_descriptor_id).User_Type_Descriptor_;
		memcpy(memory.data() + to, memory.data() + id.memory_idx, user_struct.byte_size);
		return user_struct.byte_size;
	}
	return 0;
}

size_t AST_Interpreter::alloc(size_t n_byte) noexcept {
	memory.resize(memory.size() + n_byte);
	return memory.size() - n_byte;
}

std::any AST_Interpreter::at(Identifier id) noexcept {
	auto type = types.at(id.type_descriptor_id);

	switch(type.kind) {
		case Type::Real_Kind:
			return *reinterpret_cast<long double*>(memory.data() + id.memory_idx);
		case Type::Pointer_Type_Kind:
			return *reinterpret_cast<size_t*>(memory.data() + id.memory_idx);
		case Type::User_Function_Type_Kind:
			return *reinterpret_cast<size_t*>(memory.data() + id.memory_idx);
		default: break;
	}

	println("Can't acess the value of a %s.", type.name());
	return nullptr;
}