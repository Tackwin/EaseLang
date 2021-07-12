#include "Bytecode.hpp"
#include "xstd.hpp"

#include <thread>

#define decl(name) size_t name(\
const std::vector<AST::Node>& nodes, size_t idx, Program& program, std::string_view file\
) noexcept

void emit(Program& program, IS::Instruction x) {
	auto* f = &program.code;
	if (program.current_function_idx) f = &program.functions[program.current_function_idx - 1];
	program.annotations[f->size()] = "";
	f->push_back(x);
}

size_t alloc_constant(Program& prog, long double constant) noexcept {
	prog.data.resize(prog.data.size() + sizeof(constant));
	memcpy(prog.data.data() + prog.data.size() - sizeof(constant), &constant, sizeof(constant));
	return prog.data.size() - sizeof(constant);
}
size_t alloc_constant(Program& prog, const std::uint8_t* data, size_t n) noexcept {
	prog.data.resize(prog.data.size() + n);
	memcpy(prog.data.data() + prog.data.size() - n, data, 1);
	return prog.data.size() - n;
}
size_t alloc_constant(Program& prog, const char* data, size_t n) noexcept {
	return alloc_constant(prog, (const std::uint8_t*)data, n);
}


decl(expression   );
decl(identifier   );
decl(declaration  );
decl(litteral     );
decl(list_op      );
decl(unary_op     );
decl(group_expr   );
decl(group_stat   );
decl(if_call      );
decl(for_loop     );
decl(while_loop   );
decl(function_call);
decl(return_call  );
decl(init_list    );
decl(array_access );

decl(statement) {
	size_t old_stack = program.stack_ptr;
	auto ret = expression(nodes, idx, program, file);
	if (program.stack_ptr != old_stack) {
		emit(program, IS::Pop{ program.stack_ptr - old_stack });
		program.stack_ptr = old_stack;
	}
	return ret;
}

decl(expression) {
	auto& node = nodes[idx];

	size_t ret;

	switch (node.kind) {
	case AST::Node::Identifier_Kind:          ret = identifier   (nodes, idx, program, file); break;
	case AST::Node::Litteral_Kind:            ret = litteral     (nodes, idx, program, file); break;
	case AST::Node::Operation_List_Kind:      ret = list_op      (nodes, idx, program, file); break;
	case AST::Node::Unary_Operation_Kind:     ret = unary_op     (nodes, idx, program, file); break;
	case AST::Node::Group_Statement_Kind:     ret = group_stat   (nodes, idx, program, file); break;
	case AST::Node::Group_Expression_Kind:    ret = group_expr   (nodes, idx, program, file); break;
	case AST::Node::If_Kind:                  ret = if_call      (nodes, idx, program, file); break;
	case AST::Node::For_Kind:                 ret = for_loop     (nodes, idx, program, file); break;
	case AST::Node::While_Kind:               ret = while_loop   (nodes, idx, program, file); break;
	case AST::Node::Function_Call_Kind:       ret = function_call(nodes, idx, program, file); break;
	case AST::Node::Return_Call_Kind:         ret = return_call  (nodes, idx, program, file); break;
	case AST::Node::Initializer_List_Kind:    ret = init_list    (nodes, idx, program, file); break;
	case AST::Node::Array_Access_Kind:        ret = array_access (nodes, idx, program, file); break;
	case AST::Node::Declaration_Kind:         ret = declaration  (nodes, idx, program, file); break;
	default:                                  ret = 0;
	}

	return ret;
}


decl(identifier) {
	auto& node = nodes[idx].Identifier_;
	auto id = program.interpreter.lookup(string_view_from_view(file, node.token.lexeme));

	auto& type = program.interpreter.types.at(id.Identifier_.type_descriptor_id);
	emit(program, IS::Load({ id.Identifier_.memory_idx, type.get_size() }));

	program.stack_ptr += type.get_size();

	return type.get_unique_id();
}
decl(declaration) {
	auto& node = nodes[idx].Declaration_;
	size_t last_ptr = program.memory_stack_ptr;

	auto name = string_view_from_view(file, node.identifier.lexeme);

	size_t type_hint = 0;
	size_t type_hint_size = 0;

	AST_Interpreter::Identifier id;
	id.memory_idx = program.memory_stack_ptr;
	
	if (node.type_expression_idx) {
		auto t = program.interpreter.type_interpret(nodes, node.type_expression_idx, file);
		type_hint = t.get_unique_id();
		type_hint_size = t.get_size();
		emit(program, IS::Alloc{ type_hint_size });
		program.memory_stack_ptr += type_hint_size;
	}


	if (node.value_expression_idx) {
		// >Type

		type_hint = expression(nodes, node.value_expression_idx, program, file);
		if (!type_hint) { // if expression return false that means that we are dealing with
		                  // a type definition like vec2 := struct { x := 0; y := 0; };

			auto t = program.interpreter.type_interpret(nodes, node.value_expression_idx, file);
			type_hint = t.get_unique_id();
			type_hint_size = t.get_size();

			if (t.typecheck(AST_Interpreter::Type::User_Function_Type_Kind)) {
				id.memory_idx = program.memory_stack_ptr;
				id.type_descriptor_id = t.User_Function_Type_.unique_id;

				emit(program, IS::Alloc{ t.get_size() });
				program.memory_stack_ptr += t.get_size();
				emit(program, IS::Constantf{ program.functions.size() });
				emit(program, IS::Save({ id.memory_idx, t.get_size() }));
				emit(program, IS::Pop{ 8 });
				program.interpreter.new_variable(name, id);

				size_t old_f = program.current_function_idx;
				program.functions.emplace_back();
				program.current_function_idx = program.functions.size();
				program.compile_function(nodes, node.value_expression_idx, file);
				program.current_function_idx = old_f;
			}
			if (t.kind == AST_Interpreter::Type::User_Struct_Type_Kind) {
				program.interpreter.type_name_to_hash[name] = t.User_Struct_Type_.unique_id;
			}

			return 0;
		}

		if (!node.type_expression_idx) {
			type_hint_size = program.interpreter.types.at(type_hint).get_size();
			emit(program, IS::Alloc{ type_hint_size });
			program.memory_stack_ptr += type_hint_size;
		}
		id.type_descriptor_id = type_hint;
	}

	emit(program, IS::Save({ id.memory_idx, type_hint_size }));
	program.interpreter.new_variable(name, id);
	return 0;
}
decl(litteral) {
	auto& node = nodes[idx].Litteral_;
	auto view = node.token.lexeme;

	if (node.token.type == Token::Type::Number) {
		static std::string temp;
		temp.clear();
		temp.resize(view.size);
		memcpy(temp.data(), file.data() + view.i, view.size);
		char* end_ptr = nullptr;
		long double x = std::strtold(temp.c_str(), &end_ptr);

		emit(program, IS::Constant{ alloc_constant(program, x), 8 });
		program.stack_ptr += sizeof(x);
		return AST_Interpreter::Real_Type::unique_id;
	}
	if (node.token.type == Token::Type::True) {
		emit(program, IS::True{});
		program.stack_ptr += sizeof(long double);
		return AST_Interpreter::Bool_Type::unique_id;
	}
	if (node.token.type == Token::Type::False) {
		emit(program, IS::False{});
		program.stack_ptr += sizeof(long double);
		return AST_Interpreter::Bool_Type::unique_id;
	}
	if (node.token.type == Token::Type::String) {
		emit(program, IS::Constant({
			alloc_constant(program, file.data() + view.i + 1, view.size - 2), view.size - 2
		}));
		program.stack_ptr += view.size - 2;
		return program.interpreter.create_array_type(
			AST_Interpreter::Byte_Type::unique_id,
			view.size - 2
		).get_unique_id();
	}

	return 0;
}
decl(list_op) {
	auto& node = nodes[idx].Operation_List_;

	if (node.op == AST::Operator::Assign) {
		size_t before_stack = program.stack_ptr;
		expression(nodes, node.rest_idx, program, file);

		auto& ident = nodes[node.left_idx].Identifier_;
		auto id = program.interpreter.lookup(string_view_from_view(file, ident.token.lexeme));
		emit(program, IS::Save({ id.Identifier_.memory_idx, program.stack_ptr - before_stack }));
		emit(program, IS::Pop{ program.stack_ptr - before_stack });
		program.stack_ptr = before_stack;

		// assignement returns the value of the assignee so we evaluate it at the end.
		return expression(nodes, node.left_idx, program, file);
	}
	if (node.op == AST::Operator::As) {
		expression(nodes, node.left_idx, program, file);
		return program.interpreter.type_interpret(nodes, node.rest_idx, file).get_unique_id();
	}

	// >TODO(Tackwin): handle a (op) b (op) c. For now we handle only a (op) b
	size_t right_type_id = expression(nodes, node.left_idx, program, file);
	size_t left_type_id  = expression(nodes, node.rest_idx, program, file);

	switch (node.op) {
	case AST::Operator::Plus:   emit(program, IS::Add{}); break;
	case AST::Operator::Minus:  emit(program, IS::Sub{}); break;
	case AST::Operator::Star:   emit(program, IS::Mul{}); break;
	case AST::Operator::Eq:     emit(program, IS::Eq{}); break;
	case AST::Operator::Neq:    emit(program, IS::Neq{}); break;
	case AST::Operator::Lt:     emit(program, IS::Lt{}); break;
	case AST::Operator::Leq:    emit(program, IS::Leq{}); break;
	case AST::Operator::Gt:     emit(program, IS::Gt{}); break;
	case AST::Operator::Mod:    emit(program, IS::Mod{}); break;
	case AST::Operator::Div:    emit(program, IS::Div{}); break;
	default: assert("Not supported"); break;
	}

	program.stack_ptr -= sizeof(long double);
	return AST_Interpreter::Real_Type::unique_id;
}
decl(unary_op) {
	auto& node = nodes[idx].Unary_Operation_;

	size_t right_type = 0;
	if (node.op != AST::Operator::Amp)
		right_type = expression(nodes, node.right_idx, program, file);

	switch(node.op) {
		case AST::Operator::Minus:
			emit(program, IS::Neg{});
			return AST_Interpreter::Real_Type::unique_id;
		case AST::Operator::Not:
			emit(program, IS::Not{});
			return AST_Interpreter::Bool_Type::unique_id;
		case AST::Operator::Inc: {
			auto& ident = nodes[node.right_idx].Identifier_;
			auto id = program.interpreter.lookup(string_view_from_view(file, ident.token.lexeme));
			emit(program, IS::Inc{});
			emit(program, IS::Save({ id.Identifier_.memory_idx, 8 }));
			return AST_Interpreter::Real_Type::unique_id;
		}
		case AST::Operator::Star: {
			auto ptr_type = program.interpreter.types.at(right_type);
			assert(ptr_type.kind == AST_Interpreter::Type::Pointer_Type_Kind);
			auto under_type = program.interpreter.types.at(
				ptr_type.Pointer_Type_.user_type_descriptor_idx
			);

			emit(program, IS::Load_At{ under_type.get_size() });
			program.stack_ptr -= 8;
			program.stack_ptr += under_type.get_size();
			return under_type.get_unique_id();
		}
		case AST::Operator::Amp: {
			assert(nodes[node.right_idx].kind == AST::Node::Identifier_Kind);
			auto& ident = nodes[node.right_idx].Identifier_;
			auto id = program.interpreter.lookup(string_view_from_view(file, ident.token.lexeme));
			emit(program, IS::Constant({ alloc_constant(program, id.Identifier_.memory_idx), 8 }));
			return program.interpreter.create_pointer_type(
				id.Identifier_.type_descriptor_id
			).get_unique_id();
		}
		default:
			return 0;
	}

}
decl(group_expr) {
	return expression(nodes, nodes[idx].Group_Expression_.inner_idx, program, file);
}
decl(group_stat) {
	auto& node = nodes[idx].Group_Statement_;

	program.interpreter.push_scope();
	defer { program.interpreter.pop_scope(); };
	for (size_t idx = node.inner_idx; idx; idx = nodes[idx]->next_statement)
		statement(nodes, idx, program, file);
	return 0;
}


// CODE
// TEST
// IF_TRUE_JUMP_TO     |
// JUMP_TO_ELSE        v |
// POP                   |
// CODE_IF_SECTION       |
// JUMP_TO_OUT           v |
// POP                     |
// CODE_ELSE_SECTION       v
// CODE_REST
decl(if_call) {
	auto& node = nodes[idx].If_;

	auto cond_type = expression(nodes, node.condition_idx, program, file);
	program.stack_ptr -= 8;

	size_t if_idx = program.get_current_function()->size();
	emit(program, IS::If_Jmp_Rel({ 2 }));
	auto jmp_else = program.get_current_function()->size();
	emit(program, IS::Jmp_Rel({ 0 }));
	emit(program, IS::Pop({ 8 }));
	statement(nodes, node.if_statement_idx, program, file);
	auto jmp_out_idx = program.get_current_function()->size();
	emit(program, IS::Jmp_Rel({ 0 }));
	emit(program, IS::Pop({ 8 }));

	auto jmp_else_offset = program.get_current_function()->size() - jmp_else;
	program.get_current_function()->at(jmp_else).Jmp_Rel_.dt_ip = jmp_else_offset;

	statement(nodes, node.else_statement_idx, program, file);

	auto jmp_out_offset = program.get_current_function()->size() - jmp_out_idx;
	program.get_current_function()->at(jmp_out_idx).If_Jmp_Rel_.dt_ip = jmp_out_offset;

	return 0;
}
decl(for_loop) {
	auto& node = nodes[idx].For_;

	size_t old_stack = program.memory_stack_ptr;
	program.interpreter.push_scope();
	defer {
		program.interpreter.pop_scope();
		program.memory_stack_ptr = old_stack;
	};

	statement(nodes, node.init_statement_idx, program, file);

	size_t top_idx = program.get_current_function()->size();
	expression(nodes, node.cond_statement_idx, program, file);
	program.stack_ptr -= 8;
	emit(program, IS::If_Jmp_Rel({ 3 }));
	emit(program, IS::Pop({ 8 }));
	size_t jmp_idx = program.get_current_function()->size();
	emit(program, IS::Jmp_Rel({ 0 }));
	emit(program, IS::Pop({ 8 }));

	statement(nodes, node.loop_statement_idx, program, file);
	statement(nodes, node.next_statement_idx, program, file);

	int dt = (int)top_idx - (int)program.get_current_function()->size();
	emit(program, IS::Jmp_Rel({ dt }));

	program.get_current_function()->at(jmp_idx).Jmp_Rel_.dt_ip =
		program.get_current_function()->size() - jmp_idx;

	return 0;
}
decl(while_loop) {
	return 0;
}
decl(function_call) {
	auto& node = nodes[idx].Function_Call_;

	auto name = string_view_from_view(file, nodes[node.identifier_idx].Identifier_.token.lexeme);
	if (name == "print") {
		size_t arg_type_id = expression(
			nodes, nodes[node.argument_list_idx].Argument_.value_idx, program, file
		);
		if (
			program.interpreter.types.at(arg_type_id).get_unique_id() ==
			AST_Interpreter::Byte_Type::unique_id
		) {
			emit(program, IS::Print_Byte{});
			return 0;
		} else {
			emit(program, IS::Print{});
			return 0;
		}
	}
	if (name == "sleep") {
		expression(
			nodes, nodes[node.argument_list_idx].Argument_.value_idx, program, file
		);
		emit(program, IS::Sleep{});
		return 0;
	}
	if (name == "int") {
		expression(
			nodes, nodes[node.argument_list_idx].Argument_.value_idx, program, file
		);
		emit(program, IS::Int{});
		return 0;
	}


	auto id = program.interpreter.lookup(name);
	auto& type = program.interpreter.types.at(id.Identifier_.type_descriptor_id);

	size_t old_stack = program.stack_ptr;
	for (size_t i = node.argument_list_idx; i; i = nodes[i]->next_statement) {
		auto& param = nodes[i].Argument_;
		size_t arg_type = expression(nodes, param.value_idx, program, file);
	}

	size_t bef_stack = program.stack_ptr;
	expression(nodes, node.identifier_idx, program, file);
	emit(program, IS::Call_At{bef_stack - old_stack});
	program.stack_ptr = old_stack;

	if (type.kind == AST_Interpreter::Type::User_Function_Type_Kind) {
		for (auto& x : type.User_Function_Type_.return_type) {
			auto& ret_type = program.interpreter.types.at(x);
			program.stack_ptr += ret_type.get_size();
		}
		if (type.User_Function_Type_.return_type.empty()) return 0;
		// >TODO(Tackwin): >Return Handle multiple returns
		return program.interpreter.types.at(
			type.User_Function_Type_.return_type.front()
		).get_unique_id();
	} else if (type.kind == AST_Interpreter::Type::Function_Signature_Kind) {
		for (auto& x : type.Function_Signature_.return_types) {
			auto& ret_type = program.interpreter.types.at(x);
			program.stack_ptr += ret_type.get_size();
		}
		if (type.Function_Signature_.return_types.empty()) return 0;
		return program.interpreter.types.at(
			type.Function_Signature_.return_types.front()
		).get_unique_id();
	}
	return 0;
}
decl(return_call) {
	auto& node = nodes[idx].Return_Call_;

	size_t to_return = 0;
	for (size_t i = node.return_value_idx; i; i = nodes[i]->next_statement) {
		size_t ret_type = expression(nodes, i, program, file);
		to_return += program.interpreter.types.at(ret_type).get_size();
	}

	emit(program, IS::Ret{ to_return });
	return 0;
}
decl(init_list) {
	auto& node = nodes[idx].Initializer_List_;

	// We create an anonymous id (We are not going to register it)
	AST_Interpreter::Identifier new_id;
	new_id.memory_idx = program.stack_ptr;

	// If we have a type hint 'vec{0, 0}' we take that
	if (node.type_identifier) {
		auto type = program.interpreter.type_interpret(nodes, *node.type_identifier, file);

		new_id.type_descriptor_id = type.get_unique_id();
	} else {
		// Else we try to deduce it
		// >TODO(Tackwin): auto deduce type from context.
		assert(false);
	}

	// We alloc enough size to hold the type that we got
	size_t type_size = program.interpreter.types.at(new_id.type_descriptor_id).get_size();
	emit(program, IS::Alloc{type_size});

	size_t running_ptr = 0;

	// we go through every expression in the init list and copy it to the allocated memory section
	// right now we assume that every field is filled but we will change that letter. >TODO(Tackwin)
	for (size_t i = node.expression_list_idx; i; i = nodes[i]->next_statement) {
		auto& n = nodes[i];

		size_t type_idx = expression(nodes, i, program, file);
		size_t running_type_size = program.interpreter.types.at(type_idx).get_size();

		emit(program, IS::Save({ new_id.memory_idx + running_ptr, running_type_size }));
		running_ptr += running_type_size;
	}

	return new_id.type_descriptor_id;
}
decl(array_access) {
	return 0;
}

std::vector<IS::Instruction>* Program::get_current_function() noexcept {
	auto* f = &code;
	if (current_function_idx) f = &functions[current_function_idx - 1];
	return f;
}

void Program::compile_function(
	const std::vector<AST::Node>& nodes,
	size_t idx,
	std::string_view file
) noexcept {
	auto& node = nodes[idx].Function_Definition_;

	auto old_memory_stack_ptr = memory_stack_ptr;
	defer { memory_stack_ptr = old_memory_stack_ptr; };

	memory_stack_ptr = 0;

	interpreter.push_scope();
	interpreter.scopes.back().fence = true;

	defer{ interpreter.pop_scope(); };

	size_t running = 0;
	for (size_t i = node.parameter_list_idx; i; i = nodes[i]->next_statement) {
		auto& param = nodes[i].Declaration_;

		auto name = string_view_from_view(file, param.identifier.lexeme);

		AST_Interpreter::Identifier id;
		id.memory_idx = running;
		id.type_descriptor_id =
			interpreter.type_interpret(nodes, param.type_expression_idx, file).get_unique_id();

		running += interpreter.types.at(id.type_descriptor_id).get_size();

		interpreter.new_variable(name, id);
	}

	memory_stack_ptr += running;

	for (size_t i = node.statement_list_idx; i; i = nodes[i]->next_statement) {
		statement(nodes, i, *this, file);
	}

	emit(*this, IS::Ret{});

	memory_stack_ptr -= running;
}

extern Program compile(
	const std::vector<AST::Node>& nodes,
	std::string_view file
) noexcept {
	Program program;

	for (size_t idx = 1; idx < nodes.size(); ++idx) if (nodes[idx]->depth == 0) {
		statement(nodes, idx, program, file);
	}

	emit(program, IS::Exit());

	std::unordered_map<size_t, size_t> map_idx;
	std::unordered_map<size_t, size_t> map_cst;
	for (size_t i = 0; i < program.functions.size(); ++i) {
		map_cst[i] = alloc_constant(program, program.code.size());
		map_idx[i] = program.code.size();
		program.code.insert(
			std::end(program.code),
			std::begin(program.functions[i]),
			std::end(program.functions[i])
		);
	}

	for (auto& x : program.code) {
		if (x.typecheck(IS::Instruction::Call_Kind))
			x.Call_.f_idx = map_idx[x.Call_.f_idx];
		if (x.kind == IS::Instruction::Constantf_Kind)
			x = IS::Constant{ map_cst[x.Constantf_.ptr], 8 };
	}

	return program;
}

void Program::debug() const noexcept {
	for (size_t i = 0; i < code.size(); ++i) {
		printf(">% 5d ", i);
		code[i].debug();
		if (annotations.count(i)) printf("%-100s", annotations.at(i).c_str());
		printf("\n");
	}
}


template<typename T>
T pop_stack(std::vector<std::uint8_t>& stack) noexcept {
	T x = *reinterpret_cast<T*>(stack.data() + stack.size() - sizeof(T));
	stack.resize(stack.size() - sizeof(T));
	return x;
}
template<typename T>
T peek_stack(std::vector<std::uint8_t>& stack) noexcept {
	T x = *reinterpret_cast<T*>(stack.data() + stack.size() - sizeof(T));
	return x;
}
template<typename T>
void push_stack(T x, std::vector<std::uint8_t>& stack) noexcept {
	stack.resize(stack.size() + sizeof(T));
	memcpy(stack.data() + stack.size() - sizeof(T), &x, sizeof(T));
}
void push_stack(std::vector<std::uint8_t>& stack, const std::uint8_t* data, size_t n) noexcept {
	stack.resize(stack.size() + n);
	memcpy(stack.data() + stack.size() - n, data, n);
}

void Bytecode_VM::execute(const Program& program) noexcept {
	stack_frame.clear();
	stack_frame.push_back(0);

	call_stack.clear();
	call_stack.push_back(0);

	memory_stack_frame.clear();
	memory_stack_frame.push_back(0);

	for (size_t ip = 0, n_max = 0; ip < program.code.size(); ++ip, ++n_max) {
		auto inst = program.code[ip];

		//size_t col = 0;
		//for (size_t i = 0; i < call_stack.size(); ++i) printf("-");
		//col += printf("|");
		//for (size_t i = 0; i < stack.size(); i += sizeof(long double)) {
		//	col +=  printf("%10.5Lf|", *reinterpret_cast<long double*>(stack.data() + i));
		//}
		//for (col %= 100; col < 100; ++col) printf(" ");
		//inst.debugln();
		//fflush(stdout);

		#define BINARY_OP(k, op) case IS::Instruction::k : { \
			auto b = pop_stack<long double>(stack); \
			auto a = pop_stack<long double>(stack); \
			push_stack<long double>(a op b, stack); \
			break; \
		}

		switch(inst.kind) {
			case IS::Instruction::None_Kind: break;
			case IS::Instruction::Constant_Kind: {
				assert(inst.Constant_.ptr + inst.Constant_.n <= program.data.size());
				push_stack(
					stack,
					program.data.data() + inst.Constant_.ptr,
					inst.Constant_.n
				);
				break;
			}
			BINARY_OP(Add_Kind, +);
			BINARY_OP(Sub_Kind, -);
			BINARY_OP(Mul_Kind, *);
			BINARY_OP(Div_Kind, /);
			BINARY_OP(Eq_Kind, ==);
			BINARY_OP(Neq_Kind, !=);
			BINARY_OP(Lt_Kind, < );
			BINARY_OP(Leq_Kind, <=);
			BINARY_OP(Gt_Kind, > );
			case IS::Instruction::Mod_Kind: {
				auto b = pop_stack<long double>(stack);
				auto a = pop_stack<long double>(stack);
				push_stack<long double>(std::fmodl(a, b), stack);
				break;
			}
			case IS::Instruction::Inc_Kind: {
				auto b = pop_stack<long double>(stack);
				push_stack<long double>(b + 1, stack);
				break;
			}
			case IS::Instruction::Not_Kind: {
				auto x = pop_stack<bool>(stack);
				push_stack(!x, stack);
				break;
			}
			case IS::Instruction::Neg_Kind: {
				auto x = pop_stack<long double>(stack);
				push_stack(-x, stack);
				break;
			}
			case IS::Instruction::Print_Kind: {
				auto x = peek_stack<long double>(stack);
				printf("[%zu] %Lf\n", ip, x);
				break;
			}
			case IS::Instruction::Print_Byte_Kind: {
				auto x = peek_stack<char>(stack);
				printf("[%zu] %c\n", ip, x);
				break;
			}
			case IS::Instruction::Sleep_Kind: {
				auto x = peek_stack<long double>(stack);
				std::this_thread::sleep_for(
					std::chrono::nanoseconds((long long)(1'000'000'000 * x))
				);
				break;
			}
			case IS::Instruction::True_Kind: {
				push_stack<bool>(true, stack);
				break;
			}
			case IS::Instruction::False_Kind: {
				push_stack<bool>(false, stack);
				break;
			}
			case IS::Instruction::Push_Kind: {
				stack.resize(stack.size() + inst.Push_.n);
				break;
			}
			case IS::Instruction::Pop_Kind: {
				stack.resize(stack.size() - inst.Pop_.n);
				break;
			}
			case IS::Instruction::Load_Kind: {
				push_stack(
					stack,
					memory.data() + inst.Load_.memory_ptr + memory_stack_frame.back(),
					inst.Load_.n
				);
				break;
			}
			case IS::Instruction::Load_At_Kind: {
				auto ptr = pop_stack<long double>(stack);
				push_stack(
					stack,
					memory.data() + (size_t)ptr + memory_stack_frame.back(),
					inst.Load_At_.n
				);
				break;
			}
			case IS::Instruction::Save_Kind: {
				assert(
					memory.size() >=
					inst.Save_.memory_ptr + inst.Save_.n + memory_stack_frame.back()
				);
				memcpy(
					memory.data() + inst.Save_.memory_ptr + memory_stack_frame.back(),
					stack.data() + stack.size() - inst.Save_.n,
					inst.Save_.n
				);
				break;
			}
			case IS::Instruction::Alloc_Kind: {
				memory.resize(memory.size() + inst.Alloc_.n);
				break;
			}
			case IS::Instruction::Call_Kind: {
				call_stack.push_back(ip + 1);
				stack_frame.push_back(stack.size() - inst.Call_.n);
				memory_stack_frame.push_back(memory.size());

				memory.resize(memory.size() + inst.Call_.n);
				memcpy(
					memory.data() + memory.size() - inst.Call_.n,
					stack .data() + stack .size() - inst.Call_.n,
					inst.Call_.n
				);

				stack.resize(stack.size() - inst.Call_.n);

				ip = inst.Call_.f_idx - 1;
				break;
			}
			case IS::Instruction::Call_At_Kind: {
				call_stack.push_back(ip + 1);
				ip = pop_stack<long double>(stack) - 1;
				stack_frame.push_back(stack.size() - inst.Call_At_.n);
				memory_stack_frame.push_back(memory.size());

				memory.resize(memory.size() + inst.Call_At_.n);
				memcpy(
					memory.data() + memory.size() - inst.Call_At_.n,
					stack .data() + stack .size() - inst.Call_At_.n,
					inst.Call_At_.n
				);

				stack.resize(stack.size() - inst.Call_At_.n);

				break;
			}
			case IS::Instruction::If_Jmp_Rel_Kind: {
				if (peek_stack<long double>(stack)) ip += inst.If_Jmp_Rel_.dt_ip - 1;
				break;
			}
			case IS::Instruction::Jmp_Rel_Kind: {
				ip += inst.Jmp_Rel_.dt_ip - 1;
				break;
			}
			case IS::Instruction::Ret_Kind: {
				thread_local std::vector<std::uint8_t> temp;
				temp.clear();
				temp.insert(
					std::end(temp),
					std::end(stack) - inst.Ret_.n,
					std::end(stack)
				);

				ip = call_stack.back() - 1;
				stack.resize(stack_frame.back());
				memory.resize(memory_stack_frame.back());
				
				call_stack.pop_back();
				stack_frame.pop_back();
				memory_stack_frame.pop_back();

				stack.insert(
					std::end(stack),
					std::begin(temp),
					std::end(temp)
				);

				break;
			}
			case IS::Instruction::Exit_Kind: {
				ip = program.code.size();
				break;
			}
			case IS::Instruction::Int_Kind: {
				auto x = pop_stack<long double>(stack);
				x = (long double)(long long int)x;
				push_stack<long double>(x, stack);
			}
			default: assert("Not supported"); break;
		}
	}
}
