#include "Interpreter.hpp"
#include "AST.hpp"

#include "xstd.hpp"

#include <charconv>
#include <thread>

using Value = AST_Interpreter::Value;
using Type = AST_Interpreter::Type;

Value AST_Interpreter::interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx];
	switch (node.kind) {
	case AST::Node::Identifier_Kind:          return identifier   (nodes, idx, file);
	case AST::Node::Assignement_Kind:         return assignement  (nodes, idx, file);
	case AST::Node::Litteral_Kind:            return litteral     (nodes, idx, file);
	case AST::Node::Operation_List_Kind:      return list_op      (nodes, idx, file);
	case AST::Node::Unary_Operation_Kind:     return unary_op     (nodes, idx, file);
	case AST::Node::Expression_Kind:          return expression   (nodes, idx, file);
	case AST::Node::If_Kind:                  return if_call      (nodes, idx, file);
	case AST::Node::For_Kind:                 return for_loop     (nodes, idx, file);
	case AST::Node::While_Kind:               return while_loop   (nodes, idx, file);
	case AST::Node::Function_Call_Kind:       return function_call(nodes, idx, file);
	case AST::Node::Return_Call_Kind:         return return_call  (nodes, idx, file);
	case AST::Node::Initializer_List_Kind:    return init_list    (nodes, idx, file);
	case AST::Node::Array_Access_Kind:        return array_access (nodes, idx, file);
	default:                                  return nullptr;
	}
}

Type AST_Interpreter::type_interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx];
	switch(node.kind) {
	case AST::Node::Type_Identifier_Kind:     return type_ident(nodes, idx, file);
	case AST::Node::Struct_Definition_Kind:   return struct_def   (nodes, idx, file);
	case AST::Node::Function_Definition_Kind: return function     (nodes, idx, file);
	default:                                  return nullptr;
	}
}

Value AST_Interpreter::interpret(
	AST_Nodes nodes, const User_Function_Type& f, std::string_view file
) noexcept {
	Value v;
	for (size_t idx = f.start_idx; idx; idx = nodes[idx]->next_statement) {
		v = interpret(nodes, idx, file);
		if (v.typecheck(Value::Return_Call_Kind)) break;
	}

	if (!v.typecheck(Value::Return_Call_Kind) && !f.return_type.empty()) {
		printlns("Reached end of non void returning function.");
		return nullptr;
	}
	if (!v.typecheck(Value::Return_Call_Kind)) return nullptr;
	auto& r = v.Return_Call_;
	if (r.values.size() != f.return_type.size()) {
		println(
			"Trying to return from a function with wrong number of return parameters, "
			"expected %zu got %zu.", f.return_type.size(), r.values.size()
		);
		return nullptr;
	}

	return r.values.empty() ? nullptr : at(r.values.front());
}

Value AST_Interpreter::init_list(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Initializer_List_;

	if (node.type_identifier) {
		auto type = type_interpret(nodes, *node.type_identifier, file);

		Identifier new_identifier;
		new_identifier.memory_idx = alloc(type.get_size());
		new_identifier.type_descriptor_id = type.get_unique_id();

		switch (type.kind) {
			case Type::Real_Type_Kind:
				copy(
					interpret(nodes, node.expression_list_idx, file), new_identifier.memory_idx
				);
				break;
			case Type::Array_View_Type_Kind: {
				size_t array_data = alloc(
					type.Array_View_Type_.length *
					types.at(type.Array_View_Type_.user_type_descriptor_idx).get_size()
				);
				
				for (
					size_t idx = node.expression_list_idx, i = 0;
					idx;
					idx = nodes[idx]->next_statement, i++
				) {
					auto underlying_type = types[type.Array_View_Type_.user_type_descriptor_idx];
					copy(
						interpret(nodes, idx, file),
						array_data + i * underlying_type.get_size()
					);
				}

				Array_View x;
				x.length = type.Array_View_Type_.length;
				x.memory_idx = array_data;
				copy(x, new_identifier.memory_idx);
				break;
			}
			case Type::User_Struct_Type_Kind: {
				auto& user_struct = type.User_Struct_Type_;

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

Value AST_Interpreter::unary_op(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Unary_Operation_;

	switch (node.op) {
		case AST::Operator::Minus: {
			auto x = interpret(nodes, node.right_idx, file);
			if (x.typecheck(Value::Identifier_Kind)) x = at(x.cast<Identifier>());
			if (x.typecheck(Value::Real_Kind)) {
				println("Type error, expected long double got %s", x.name());
				return nullptr;
			}
			return Real{ -x.cast<Real>().x };
		}
		case AST::Operator::Plus: {
			auto x = interpret(nodes, node.right_idx, file);
			if (x.typecheck(Value::Identifier_Kind)) x = at(x.cast<Identifier>());
			if (x.typecheck(Value::Real_Kind)) {
				println("Type error, expected long double got %s", x.name());
				return nullptr;
			}
			return Real{ +x.cast<Real>().x };
		}
		case AST::Operator::Inc: {
			auto x = interpret(nodes, node.right_idx, file);
			if (!x.typecheck(Value::Identifier_Kind)) {
				println("Type error, expected identifier got %s", x.name());
				return nullptr;
			}

			if (x.cast<Identifier>().type_descriptor_id != Real_Type::unique_id)  {
				println("Type error, expected long double got %s", x.name());
				return nullptr;
			}

			auto id = x.cast<Identifier>();
			long double v;
			memcpy(&v, memory.data() + id.memory_idx, sizeof(long double));
			v++;
			memcpy(memory.data() + id.memory_idx, &v, sizeof(long double));

			return x.cast<Identifier>();
		}
		case AST::Operator::Amp: {
			auto x = interpret(nodes, node.right_idx, file);
			if (!x.typecheck(Value::Identifier_Kind)) {
				println("Type error, expected Identifier got %s", x.name());
				return nullptr;
			}

			Pointer p;
			p.memory_idx         = x.cast<Identifier>().memory_idx;
			p.type_descriptor_id = x.cast<Identifier>().type_descriptor_id;
			return p;
		}
		case AST::Operator::Star: {
			auto x = interpret(nodes, node.right_idx, file);
			if (x.typecheck(Value::Identifier_Kind)) x = at(x.cast<Identifier>());
			if (!x.typecheck(Value::Pointer_Kind)) {
				println("Type error, expected Pointer got %s", x.name());
				return nullptr;
			}

			Identifier id;
			id.memory_idx = x.cast<Pointer>().memory_idx;
			id.type_descriptor_id = x.cast<Pointer>().type_descriptor_id;
			return id;
		}
		case AST::Operator::Not:
		default: return nullptr;
	}
}

Value AST_Interpreter::list_op(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Operation_List_;
	switch(node.op) {
		case AST::Operator::Gt: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);
			
			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());

			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Bool{ left.cast<Real>().x > right.cast<Real>().x };
		}
		case AST::Operator::Eq: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);
			
			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());

			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Bool{ left.cast<Real>().x == right.cast<Real>().x };
		}
		case AST::Operator::Neq: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);
			
			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());

			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Bool{ left.cast<Real>().x != right.cast<Real>().x };
		}
		case AST::Operator::Lt: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());
			
			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Bool{ left.cast<Real>().x < right.cast<Real>().x };
		}
		case AST::Operator::Assign: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (!left.typecheck(Value::Identifier_Kind)) {
				println("Error expected Identifier for the lhs, got %s", left.name());
				return nullptr;
			}
			if (right.typecheck(Value::Identifier_Kind)) {
				if (
					left .cast<Identifier>().type_descriptor_id !=
					right.cast<Identifier>().type_descriptor_id
				) {
					printlns("Mismatch between left and right type.");
					return nullptr;
				}

				copy(right.cast<Identifier>(), left.cast<Identifier>().memory_idx);
			} else if (right.typecheck(Value::Real_Kind)) {
				if (left.cast<Identifier>().type_descriptor_id != Real_Type::unique_id) {
					println(
						"Mismatch between left and right type %zu, %zu.",
						left.cast<Identifier>().type_descriptor_id,
						Real_Type::unique_id
					);
					return nullptr;
				}

				auto x = right.cast<Real>().x;
				memcpy(memory.data() + left.cast<Identifier>().memory_idx, &x, sizeof(long double));
			}

			return right;
		}
		case AST::Operator::Leq: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());
			
			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Bool{ left.cast<Real>().x <= right.cast<Real>().x };
		}
		case AST::Operator::Plus: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());
			
			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Real{ left.cast<Real>().x + right.cast<Real>().x };
		}
		case AST::Operator::Star: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());
			
			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Real{ left.cast<Real>().x * right.cast<Real>().x };
		}
		case AST::Operator::Div: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());
			
			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Real{ left.cast<Real>().x / right.cast<Real>().x };
		}
		case AST::Operator::Mod: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());
			
			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Real{ std::fmodl(left.cast<Real>().x, right.cast<Real>().x) };
		}
		case AST::Operator::Minus: {
			auto left  = interpret(nodes, node.left_idx, file);
			auto right = interpret(nodes, node.rest_idx, file);

			if (left .typecheck(Value::Identifier_Kind)) left  = at(left .cast<Identifier>());
			if (right.typecheck(Value::Identifier_Kind)) right = at(right.cast<Identifier>());
			
			if (!left.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the lhs, got %s", left.name());
				return nullptr;
			}
			if (!right.typecheck(Value::Real_Kind)) {
				println("Error expected long double for the rhs, got %s", right.name());
				return nullptr;
			}

			return Real{ left.cast<Real>().x - right.cast<Real>().x };
		}
		case AST::Operator::Dot: {
			auto root_struct = interpret(nodes, node.left_idx, file);
			if ( root_struct.typecheck(Value::Pointer_Kind)) {
				Identifier id;
				id.memory_idx = read_ptr(root_struct.cast<Pointer>().memory_idx);
				id.type_descriptor_id = root_struct.cast<Pointer>().type_descriptor_id;
				root_struct = id;
			}
			if (!root_struct.typecheck(Value::Identifier_Kind)) {
				println("Expected an Identifier but got %s instead.", root_struct.name());
				return nullptr;
			}

			auto helper = [&] (Identifier user_struct, size_t next, auto& helper) -> Value {
				if (!next) return user_struct;
				auto type = types.at(user_struct.type_descriptor_id);

				switch (type.kind) {
					case Type::Pointer_Type_Kind: {
						Identifier id;
						id.memory_idx = read_ptr(user_struct.memory_idx);
						id.type_descriptor_id = type.Pointer_Type_.user_type_descriptor_idx;

						return helper(id, next, helper);
						break;
					}
					case Type::User_Struct_Type_Kind: {
						auto& next_node = nodes[next].Identifier_;
						auto name = string_view_from_view(file, next_node.token.lexeme);

						size_t idx = type.User_Struct_Type_.name_to_idx.at(name);
						Identifier id;
						id.memory_idx =
							user_struct.memory_idx + type.User_Struct_Type_.member_offsets[idx];
						id.type_descriptor_id = type.User_Struct_Type_.member_types[idx];

						return helper(id, next_node.next_statement, helper);
					}
					default:
						printlns("Should not happen.");
						return nullptr;
				}
			};

			return helper(root_struct.cast<Identifier>(), node.rest_idx, helper);
		}
		default:{
			println("Unsupported operation %s", AST::op_to_string(node.op));
			return nullptr;
		}
	}
}

Value AST_Interpreter::if_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].If_;

	auto cond = interpret(nodes, node.condition_idx, file);
	if (cond.typecheck(Value::Identifier_Kind)) cond = at(cond.cast<Identifier>());
	if (!cond.typecheck(Value::Bool_Kind)) {
		println("Expected bool on the if-condition got %s.", cond.name());
		return nullptr;
	}

	push_scope();
	defer { pop_scope(); };
	if (cond.Bool_.x) {
		for (size_t idx = node.if_statement_idx; idx; idx = nodes[idx]->next_statement) {
			auto v = interpret(nodes, idx, file);
			if (v.typecheck(Value::Return_Call_Kind)) return v;
		}
	} else {
		for (size_t idx = node.else_statement_idx; idx; idx = nodes[idx]->next_statement) {
			auto v = interpret(nodes, idx, file);
			if (v.typecheck(Value::Return_Call_Kind)) return v;
		}
	}

	return nullptr;
}

Value AST_Interpreter::for_loop(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].For_;

	push_scope();
	defer { pop_scope(); };

	interpret(nodes, node.init_statement_idx, file);

	while (true) {
		auto cond = interpret(nodes, node.cond_statement_idx, file);
		if (cond.typecheck(Value::Identifier_Kind)) cond = at(cond.cast<Identifier>());
		if (!cond.typecheck(Value::Bool_Kind)) {
			println("Expected bool on the for-condition got %s.", cond.name());
			return nullptr;
		}

		if (!cond.cast<Bool>().x) break;

		for (size_t idx = node.loop_statement_idx; idx; idx = nodes[idx]->next_statement) {
			auto v = interpret(nodes, idx, file);
			if (v.typecheck(Value::Return_Call_Kind)) return v;
		}

		interpret(nodes, node.next_statement_idx, file);
	}

	return nullptr;
}


Value AST_Interpreter::while_loop(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].While_;

	push_scope();
	defer { pop_scope(); };

	while (true) {
		auto cond = interpret(nodes, node.cond_statement_idx, file);
		if (cond.typecheck(Value::Identifier_Kind)) cond = at(cond.cast<Identifier>());
		if (!cond.typecheck(Value::Bool_Kind)) {
			println("Expected bool on the while-condition got %s.", cond.name());
			return nullptr;
		}

		if (!cond.cast<Bool>().x) break;

		for (size_t idx = node.loop_statement_idx; idx; idx = nodes[idx]->next_statement) {
			auto v = interpret(nodes, idx, file);
			if (v.typecheck(Value::Return_Call_Kind)) return v;
		}
	}

	return nullptr;
}

Type AST_Interpreter::struct_def(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Struct_Definition_;

	User_Struct_Type desc;

	size_t running_offset = 0;
	for (size_t idx = node.struct_line_idx; idx; idx = nodes[idx]->next_statement) {
		auto& def = nodes[idx].Assignement_;
		auto name = string_view_from_view(file, def.identifier.lexeme);

		desc.name_to_idx[name] = desc.member_types.size();
		desc.member_offsets.push_back(running_offset);

		// if we have type information.
		if (def.type_identifier) {
			desc.member_types.push_back(
				type_interpret(nodes, *def.type_identifier, file).get_unique_id()
			);
			running_offset += types.at(desc.member_types.back()).get_size();
		}

		desc.default_values.push_back(nullptr);
		// if we have default value
		if (def.value_idx) {
			auto value = interpret(nodes, *def.value_idx, file);

			desc.default_values.back() = interpret(nodes, *def.value_idx, file);
			// if we _only_ have default value
			if (!def.type_identifier) {
				desc.member_types.push_back(get_type_id(value));
				running_offset += types.at(desc.member_types.back()).get_size();
			}
		}

		desc.unique_id = hash_combine(desc.unique_id, Hasher()(name));
		desc.unique_id = hash_combine(desc.unique_id, desc.member_types.back());
	}
	desc.byte_size = running_offset;

	types[desc.unique_id] = desc;

	return desc;
}

Type AST_Interpreter::function(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
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

Value AST_Interpreter::function_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Function_Call_;
	thread_local std::vector<Identifier> arguments;
	arguments.clear();

	for (size_t idx = node.argument_list_idx, i = 0; idx; idx = nodes[idx]->next_statement, i++) {
		auto& param = nodes[idx].Argument_;
		auto x = interpret(nodes, param.value_idx, file);
		arguments.push_back(create_id(x));
	}

	auto any_id = interpret(nodes, node.identifier_idx, file);
	if (!any_id.typecheck(Value::Identifier_Kind)) {
		return any_id.cast<Builtin>().f(arguments);
	}
	auto id = any_id.cast<Identifier>();

	push_scope();
	defer { pop_scope(); };

	auto f = &types.at(id.type_descriptor_id).User_Function_Type_;
	for (size_t i = 0; i < arguments.size(); ++i) {
		auto name = f->parameter_name[i];
		variables.back()[name] = arguments[i];
	}

	return interpret(nodes, *f, file);
}

Value AST_Interpreter::array_access(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Array_Access_;

	auto id = interpret(nodes, node.identifier_array_idx, file);
	if (!id.typecheck(Value::Identifier_Kind)) {
		println("Error expected Identifier got %s.", id.name());
		return nullptr;
	}

	auto access_id = interpret(nodes, node.identifier_acess_idx, file);
	if (access_id.typecheck(Value::Identifier_Kind)) access_id = at(access_id.cast<Identifier>());
	if (!access_id.typecheck(Value::Real_Kind)) {
		println("Error expected long double got %s.", access_id.name());
		return nullptr;
	}

	size_t i = (size_t)std::roundl(access_id.cast<Real>().x);
	size_t size = types.at(id.cast<Identifier>().type_descriptor_id).get_size();

	Identifier identifier;
	identifier.memory_idx = id.cast<Identifier>().memory_idx + i * size;
	identifier.type_descriptor_id = id.cast<Identifier>().type_descriptor_id;

	return identifier;
}

Value AST_Interpreter::return_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Return_Call_;

	Return_Call r;
	if (node.return_value_idx) {
		auto x = interpret(nodes, node.return_value_idx, file);
		r.values.push_back(create_id(x));
	}

	return r;
}


Value AST_Interpreter::identifier(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Identifier_;

	return lookup(string_view_from_view(file, node.token.lexeme));
}


Type AST_Interpreter::create_pointer_type(size_t underlying) noexcept {
	auto hash = hash_combine(underlying, Pointer_Type::combine_id);
	if (types.count(hash) != 0) return types.at(hash);
	Pointer_Type new_pointer_type;
	new_pointer_type.unique_id = hash;
	new_pointer_type.user_type_descriptor_idx = underlying;
	types[hash] = new_pointer_type;
	return new_pointer_type;
}

Type AST_Interpreter::create_array_view_type(size_t underlying, size_t size) noexcept {
	auto hash = hash_combine(underlying, Array_View_Type::combine_id);
	hash      = hash_combine(hash, size);
	if (types.count(hash) != 0) return types.at(hash);
	Array_View_Type new_array_view_type;
	new_array_view_type.unique_id = hash;
	new_array_view_type.user_type_descriptor_idx = underlying;
	new_array_view_type.length = size;
	types[hash] = new_array_view_type;
	return new_array_view_type;
}

Type AST_Interpreter::type_ident(
	AST_Nodes nodes, size_t idx, std::string_view file
) noexcept {
	auto& node = nodes[idx].Type_Identifier_;
	if (node.pointer_to) {
		auto underlying = type_interpret(nodes, *node.pointer_to, file);
		return create_pointer_type(underlying.get_unique_id());
	}
	if (node.array_to) {
		auto underlying = type_interpret(nodes, *node.array_to, file);
		auto size       = interpret(nodes, *node.array_size, file);
		if (!size.typecheck(Value::Real_Kind)) {
			printlns("Support only constant time array.");
			return nullptr;
		}

		return create_array_view_type(
			underlying.get_unique_id(), (size_t)std::roundl(size.Real_.x)
		);
	}

	return type_lookup(string_view_from_view(file, node.identifier.lexeme));
}


Value AST_Interpreter::assignement(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	auto& node = nodes[idx].Assignement_;
	auto name = string_view_from_view(file, node.identifier.lexeme);

	if (variables.back().count(name) > 0) {
		println("Error variable %*.s already assigned.", (int)name.size(), name.data());
		return nullptr;
	}

	auto& var = variables.back()[name];

	// >TODO(Tackwin): handle type info.
	if (node.type_identifier) {
//		auto t = type_interpret(nodes, *node.type_identifier, file);
//
//		var = Identifier();
//		var.Identifier_.memory_idx = alloc(t.get_size());
//		memset(memory.data() + var.Identifier_.memory_idx, 0, t.get_size());
//		var.Identifier_.type_descriptor_id = t.get_unique_id();
	}


	if (node.value_idx) {
		auto x = interpret(nodes, *node.value_idx, file);
		if (x.typecheck(Value::None_Kind)) { // if we are defining a type
			auto t = type_interpret(nodes, *node.value_idx, file);

			if (t.typecheck(Type::User_Function_Type_Kind)) {
				var = Identifier();
				var.Identifier_.memory_idx = alloc(sizeof(size_t));
				var.Identifier_.type_descriptor_id = t.cast<User_Function_Type>().unique_id;

				write_ptr(t.cast<User_Function_Type>().start_idx, var.Identifier_.memory_idx);
			}

			if (t.typecheck(Type::User_Struct_Type_Kind)) {
				var = Identifier();
				var.Identifier_.memory_idx = 0;
				var.Identifier_.type_descriptor_id = t.cast<User_Struct_Type>().unique_id;

				auto hash = t.cast<User_Struct_Type>().unique_id;
				type_name_to_hash[name] = hash;
				types[hash] = t.cast<User_Struct_Type>();
			}
		} else { // if we are dfining a value

			var = create_id(x);
		}
	}

	return var;
}

Value AST_Interpreter::litteral(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
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
		return Real{ x };
	}

	if (node.token.type == Token::Type::String) {
		return String{ std::string(file.data() + view.i + 1, view.size - 2) };
	}
	
	return nullptr;
}

Value AST_Interpreter::expression(AST_Nodes nodes, size_t idx, std::string_view file) noexcept {
	return interpret(nodes, nodes[idx].Expression_.inner_idx, file);
}

void AST_Interpreter::print_value(const Value& value) noexcept {
	if (value.typecheck(Value::None_Kind)) {
		printlns("None.");
		return;
	}
	if (value.typecheck(Value::Real_Kind)) {
		println("%Lf", value.Real_.x);
		return;
	}

	if (value.typecheck(Value::Bool_Kind)) {
		println("%s", value.Bool_.x ? "true" : "false");
		return;
	}

	if (value.typecheck(Value::String_Kind)) {
		println("%s", value.String_.x.c_str());
		return;
	}

	//if (typecheck<std::string>(value)) {
	//	auto x = std::any_cast<std::string>(value);
	//	println("%s", x.c_str());
	//	return;
	//}

	if (value.typecheck(Value::Identifier_Kind)) {
		auto x = value.Identifier_;
		if (x.type_descriptor_id) print_value(at(x));
		return;
	}
	printlns("RIP F in the chat for my boiii");
}

Value AST_Interpreter::lookup(std::string_view id) noexcept {
	for (auto it = std::rbegin(variables); it != std::rend(variables); it++)
		for (auto& [x, v] : *it) if (x == id) return v;
	for (auto& [x, v] : builtins) if (x == id) return v;
	println("Can not find variable named %.*s.", (int)id.size(), id.data());
	return nullptr;
}

void AST_Interpreter::push_scope() noexcept { variables.emplace_back(); }
void AST_Interpreter::pop_scope()  noexcept { variables.pop_back(); }

void AST_Interpreter::push_builtin() noexcept {
	Builtin print;
	print.f = [&] (std::vector<Identifier> values) -> Identifier {
		for (auto& x : values) {
			auto y = at(x);
			if (y.typecheck(Value::Identifier_Kind)) y = at(y.cast<Identifier>());
			if (y.typecheck(Value::Pointer_Kind)) printf("%zu", y.cast<Pointer>().memory_idx);
			else if (y.typecheck(Value::Real_Kind)) printf("%Lf", y.cast<Real>().x);
			else if (y.typecheck(Value::String_Kind)) printf("%s", y.String_.x.c_str());
			else printf("Unsupported type to print (%s)", y.name());

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
		if (!x.typecheck(Value::Real_Kind)) {
			println("Sleep expect 1 long double argument got %s.", x.name());
			return {};
		}

		std::this_thread::sleep_for(
			std::chrono::nanoseconds((size_t)(1'000'000'000 * x.cast<Real>().x))
		);
		return {};
	};
	builtins["sleep"] = sleep;


	type_name_to_hash["nat"] = Nat_Type::unique_id;
	type_name_to_hash["int"] = Int_Type::unique_id;
	type_name_to_hash["void"] = Void_Type::unique_id;
	type_name_to_hash["bool"] = Bool_Type::unique_id;
	type_name_to_hash["real"] = Real_Type::unique_id;
	types[Real_Type::unique_id] = Real_Type();
	types[Bool_Type::unique_id] = Bool_Type();
	types[Void_Type::unique_id] = Void_Type();
	types[Nat_Type::unique_id] = Nat_Type();
	types[Int_Type::unique_id] = Int_Type();

	//Builtin len;
	//len.f = [&] (std::vector<Identifier> values) -> Identifier {
	//	if (values.size() != 1) {
	//		println("len expect 1 Array argument, got %zu arguments.", values.size());
	//		return {};
	//	}
//
	//	auto x = at(values.front());
	//	if (x.typecheck(Value::Identifier_Kind)) x = at(x.cast<Identifier>());
	//	if (!typecheck<Array>(x)) {
	//		println("Len expect a array argument, got %s.", x.type().name());
	//		return 0;
	//	}
//
	//	Identifier y;
	//	y.memory_idx = alloc(sizeof(long double));
	//	y.type_descriptor_id = Real_Type::unique_id;
//
	//	//return alloc(cast<Array>(x).values.size());
	//};
	//builtins["len"] = len;
}


Type AST_Interpreter::type_lookup(std::string_view id) noexcept {
	return types.at(type_name_to_hash.at(id));
}

size_t AST_Interpreter::copy(const Value& from, size_t to) noexcept {
	if (from.typecheck(Value::Bool_Kind)) {
		memory[to] = from.Bool_.x ? 1 : 0;
	}
	if (from.typecheck(Value::String_Kind)) {
		auto str = memory.size();
		auto len = from.String_.x.size();
		for (auto c : from.String_.x) memory.push_back(c);
		memcpy(memory.data() + to                 , &len, sizeof(size_t));
		memcpy(memory.data() + to + sizeof(size_t), &str, sizeof(size_t));
	}
	if (from.typecheck(Value::Array_View_Kind)) {
		auto ptr = from.Array_View_.memory_idx;
		auto len = from.Array_View_.length;
		memcpy(memory.data() + to                 , &len, sizeof(size_t));
		memcpy(memory.data() + to + sizeof(size_t), &ptr, sizeof(size_t));
	}
	if (from.typecheck(Value::Real_Kind)) {
		*reinterpret_cast<long double*>(memory.data() + to) = from.cast<Real>().x;
		return sizeof(long double);
	}
	if (from.typecheck(Value::Pointer_Kind)) {
		auto ptr = read_ptr(from.Pointer_.memory_idx);
		memcpy(memory.data() + to, &ptr, sizeof(size_t));
		return sizeof(size_t);
	}
	if (from.typecheck(Value::Identifier_Kind)) {
		auto id = from.Identifier_;
		if (types.at(id.type_descriptor_id).kind != Type::User_Struct_Type_Kind) {
			return copy(at(id), to);
		}

		auto user_struct = types.at(id.type_descriptor_id).User_Struct_Type_;
		memcpy(memory.data() + to, memory.data() + id.memory_idx, user_struct.byte_size);
		return user_struct.byte_size;
	}
	return 0;
}

size_t AST_Interpreter::alloc(size_t n_byte) noexcept {
	memory.resize(memory.size() + n_byte);
	return memory.size() - n_byte;
}

Value AST_Interpreter::at(Identifier id) noexcept {
	auto type = types.at(id.type_descriptor_id);

	switch(type.kind) {
		case Type::Real_Type_Kind: {
			Real r;
			r.x = *reinterpret_cast<long double*>(memory.data() + id.memory_idx);
			return r;
		}
		case Type::Bool_Type_Kind: {
			Bool b;
			b.x = memory[id.memory_idx] != 0;
			return b;
		}
		case Type::Pointer_Type_Kind: {
			Pointer x;
			x.memory_idx = id.memory_idx;
			x.type_descriptor_id = type.Pointer_Type_.user_type_descriptor_idx;
			return x;
		}
		default: break;
	}

	return nullptr;
}

size_t AST_Interpreter::get_type_id(const Value& x) noexcept {
	if (x.typecheck(Value::Real_Kind))        return Real_Type::unique_id;
	if (x.typecheck(Value::Bool_Kind))        return Bool_Type::unique_id;
	if (x.typecheck(Value::Pointer_Kind))     return x.Pointer_.type_descriptor_id;
	if (x.typecheck(Value::Identifier_Kind))  return x.Identifier_.type_descriptor_id;
	if (x.typecheck(Value::Array_View_Kind))  return x.Array_View_.type_descriptor_id;
	return 0;
}
void AST_Interpreter::write_ptr(size_t ptr, size_t to) noexcept {
	*reinterpret_cast<size_t*>(memory.data() + to) = ptr;
}
size_t AST_Interpreter::read_ptr(size_t ptr) noexcept {
	return *reinterpret_cast<size_t*>(memory.data() + ptr);
}

AST_Interpreter::Identifier AST_Interpreter::create_id(const Value& x) noexcept {
	Identifier new_ident;
	if (x.typecheck(Value::Real_Kind)) {
		new_ident.type_descriptor_id = Real_Type::unique_id;
		new_ident.memory_idx = alloc(sizeof(long double));
	}
	else if (x.typecheck(Value::Bool_Kind)) {
		new_ident.type_descriptor_id = Bool_Type::unique_id;
		new_ident.memory_idx = alloc(1);
	}
	else if (x.typecheck(Value::Pointer_Kind)) {
		new_ident.type_descriptor_id =
			create_pointer_type(x.Pointer_.type_descriptor_id).get_unique_id();
		new_ident.memory_idx = alloc(sizeof(size_t));
	}
	else if (x.typecheck(Value::Identifier_Kind)) {
		new_ident.type_descriptor_id = x.Identifier_.type_descriptor_id;
		new_ident.memory_idx = alloc(types.at(new_ident.type_descriptor_id).get_size());
	}
	else if (x.typecheck(Value::String_Kind)) {
		new_ident.type_descriptor_id =
			create_array_view_type(Byte_Type::unique_id, x.String_.x.size()).get_unique_id();
		new_ident.memory_idx = alloc(sizeof(size_t) + sizeof(size_t));
	}
	else if (x.typecheck(Value::Array_View_Kind)) {
		new_ident.type_descriptor_id =
			create_array_view_type(
				x.Array_View_.type_descriptor_id, x.Array_View_.length
			).get_unique_id();
		new_ident.memory_idx = alloc(sizeof(size_t) + sizeof(size_t));
	}
	else {
		assert("Sadge.");
	}
	copy(x, new_ident.memory_idx);
	return new_ident;
}

Type AST_Interpreter::type_of(const Value& x) noexcept {
	     if (x.typecheck(Value::Bool_Kind)) return Bool_Type();
	else if (x.typecheck(Value::Real_Kind)) return Real_Type();
	else if (x.typecheck(Value::Identifier_Kind))
		return types.at(x.Identifier_.type_descriptor_id);
	else if (x.typecheck(Value::Pointer_Kind))
		return create_pointer_type(x.Pointer_.type_descriptor_id);
	else if (x.typecheck(Value::Array_View_Kind))
		return create_array_view_type(x.Array_View_.type_descriptor_id, x.Array_View_.length);
	else  assert("sadge");
	return nullptr;
}
