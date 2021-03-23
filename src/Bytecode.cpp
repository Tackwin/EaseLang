#include "Bytecode.hpp"
#include "xstd.hpp"

#include <thread>

#define decl(name) size_t name(\
const std::vector<AST::Node>& nodes, size_t idx, Program& program, std::string_view file\
) noexcept

#define emit(x) program.current_function->push_back(x);

size_t alloc_constant(Program& prog, long double constant) noexcept {
	prog.data.resize(prog.data.size() + sizeof(constant));
	memcpy(prog.data.data() + prog.data.size() - sizeof(constant), &constant, sizeof(constant));
	return prog.data.size() - sizeof(constant);
}


decl(expression   );
decl(identifier   );
decl(assignement  );
decl(litteral     );
decl(list_op      );
decl(unary_op     );
decl(group_expr   );
decl(if_call      );
decl(for_loop     );
decl(while_loop   );
decl(function_call);
decl(return_call  );
decl(init_list    );
decl(array_access );

decl(expression) {
	auto& node = nodes[idx];

	size_t ret;

	switch (node.kind) {
	case AST::Node::Identifier_Kind:          ret = identifier   (nodes, idx, program, file); break;
	case AST::Node::Assignement_Kind:         ret = assignement  (nodes, idx, program, file); break;
	case AST::Node::Litteral_Kind:            ret = litteral     (nodes, idx, program, file); break;
	case AST::Node::Operation_List_Kind:      ret = list_op      (nodes, idx, program, file); break;
	case AST::Node::Unary_Operation_Kind:     ret = unary_op     (nodes, idx, program, file); break;
	case AST::Node::Group_Expression_Kind:    ret = group_expr   (nodes, idx, program, file); break;
	case AST::Node::If_Kind:                  ret = if_call      (nodes, idx, program, file); break;
	case AST::Node::For_Kind:                 ret = for_loop     (nodes, idx, program, file); break;
	case AST::Node::While_Kind:               ret = while_loop   (nodes, idx, program, file); break;
	case AST::Node::Function_Call_Kind:       ret = function_call(nodes, idx, program, file); break;
	case AST::Node::Return_Call_Kind:         ret = return_call  (nodes, idx, program, file); break;
	case AST::Node::Initializer_List_Kind:    ret = init_list    (nodes, idx, program, file); break;
	case AST::Node::Array_Access_Kind:        ret = array_access (nodes, idx, program, file); break;
	default:                                  ret = 0;
	}

	return ret;
}


decl(identifier) {
	auto& node = nodes[idx].Identifier_;
	auto id = program.interpreter.lookup(string_view_from_view(file, node.token.lexeme));

	auto& type = program.interpreter.types.at(id.Identifier_.type_descriptor_id);

	emit(IS::Load({ id.Identifier_.memory_idx, type.get_size() }));
	program.stack_ptr += type.get_size();

	return type.get_unique_id();
}
decl(assignement) {
	auto& node = nodes[idx].Assignement_;
	size_t last_ptr = program.memory_stack_ptr;

	auto name = string_view_from_view(file, node.identifier.lexeme);

	size_t type_hint = 0;
	size_t type_hint_size = 0;

	AST_Interpreter::Identifier id;
	id.memory_idx = program.memory_stack_ptr;
	
	if (node.type_identifier) {
		auto t = program.interpreter.type_interpret(nodes, *node.type_identifier, file);
		type_hint = t.get_unique_id();
		type_hint_size = t.get_size();
	}

	if (node.value_idx) {
		type_hint = expression(nodes, *node.value_idx, program, file);
		if (!type_hint) { // if expression return false that means that we are dealing with
		                  // a type definition like vec2 := struct { x := 0; y := 0; };

			auto t = program.interpreter.type_interpret(nodes, *node.value_idx, file);

			if (t.typecheck(AST_Interpreter::Type::User_Function_Type_Kind)) {
				id.memory_idx = program.functions.size();
				id.type_descriptor_id = t.User_Function_Type_.unique_id;

				program.functions.emplace_back();
				program.compile_function(
					nodes,
					*node.value_idx,
					program.functions.back(),
					file
				);
			}

		} else {
			type_hint_size = program.interpreter.types.at(type_hint).get_size();
			emit(IS::Alloc{ type_hint_size });

			emit(IS::Save({ program.memory_stack_ptr, type_hint_size }));
			id.type_descriptor_id = type_hint;
		}

	} else if (type_hint) {
		emit(IS::Alloc{ type_hint_size });
		id.type_descriptor_id = type_hint;
	}

	program.memory_stack_ptr += type_hint_size;

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
		char* end_ptr;
		long double x = std::strtold(temp.c_str(), &end_ptr);

		emit(IS::Constant{ alloc_constant(program, x) });
		program.stack_ptr += sizeof(x);
		return AST_Interpreter::Real_Type::unique_id;
	}
	if (node.token.type == Token::Type::True) {
		emit(IS::True{});
		program.stack_ptr++;
		return AST_Interpreter::Bool_Type::unique_id;
	}
	if (node.token.type == Token::Type::False) {
		emit(IS::False{});
		program.stack_ptr++;
		return AST_Interpreter::Bool_Type::unique_id;
	}

	return 0;
}
decl(list_op) {
	auto& node = nodes[idx].Operation_List_;

	// >TODO(Tackwin): handle a (op) b (op) c. For now we handle only a (op) b
	expression(nodes, node.left_idx, program, file);
	expression(nodes, node.rest_idx, program, file);

	switch (node.op) {
	case AST::Operator::Plus:  emit(IS::Add{}); break;
	case AST::Operator::Minus: emit(IS::Sub{}); break;
	case AST::Operator::Star:  emit(IS::Mul{}); break;
	}

	program.stack_ptr -= sizeof(long double);
	return AST_Interpreter::Real_Type::unique_id;
}
decl(unary_op) {
	auto& node = nodes[idx].Unary_Operation_;

	expression(nodes, node.right_idx, program, file);

	switch(node.op) {
		case AST::Operator::Minus: emit(IS::Neg{}); return AST_Interpreter::Real_Type::unique_id;
		case AST::Operator::Not:   emit(IS::Not{}); return AST_Interpreter::Bool_Type::unique_id;
		default:                                    return 0;
	}

}
decl(group_expr) {
	return expression(nodes, nodes[idx].Group_Expression_.inner_idx, program, file);
}
decl(if_call) {
	return 0;
}
decl(for_loop) {
	return 0;
}
decl(while_loop) {
	return 0;
}
decl(function_call) {
	auto& node = nodes[idx].Function_Call_;
	auto name = string_view_from_view(file, nodes[node.identifier_idx].Identifier_.token.lexeme);
	if (name == "print") {
		expression(
			nodes, nodes[node.argument_list_idx].Argument_.value_idx, program, file
		);
		emit(IS::Print{});
		return 0;
	}

	for (size_t i = node.argument_list_idx; i; i = nodes[i]->next_statement) {
		expression(nodes, i, program, file);
	}

	auto id = program.interpreter.lookup(name);
	emit(IS::Call{ id.Identifier_.memory_idx });

	auto f = program.interpreter.types.at(id.Identifier_.type_descriptor_id).User_Function_Type_;
	auto return_type = program.interpreter.types.at(f.return_type.front()).get_unique_id();

	return return_type;
}
decl(return_call) {
	auto& node = nodes[idx].Return_Call_;

	size_t program_stack = program.stack_ptr;

	for (size_t i = node.return_value_idx; i; i = nodes[i]->next_statement) {
		expression(nodes, i, program, file);
	}

	emit(IS::Ret{ program.stack_ptr - program_stack });
	return 0;
}
decl(init_list) {
	return 0;
}
decl(array_access) {
	return 0;
}


void Program::compile_function(
	const std::vector<AST::Node>& nodes,
	size_t idx,
	std::vector<IS::Instruction>& func,
	std::string_view file
) noexcept {
	auto& node = nodes[idx].Function_Definition_;

	auto old_memory_stack_ptr = memory_stack_ptr;
	auto old_function = current_function;
	defer { memory_stack_ptr = old_memory_stack_ptr; };
	defer { current_function = old_function; };

	memory_stack_ptr = 0;
	current_function = &func;

	interpreter.push_scope();
	interpreter.scopes.back().fence = true;

	size_t running = 0;
	for (size_t i = node.parameter_list_idx; i; i = nodes[i]->next_statement) {
		auto& param = nodes[i].Parameter_;

		auto name = string_view_from_view(file, param.name.lexeme);

		AST_Interpreter::Identifier id;
		id.memory_idx = running;
		id.type_descriptor_id =
			interpreter.type_interpret(nodes, param.type_identifier, file).get_unique_id();

		running += interpreter.types.at(id.type_descriptor_id).get_size();

		interpreter.new_variable(name, id);
	}

	for (size_t i = node.statement_list_idx; i; i = nodes[i]->next_statement) {
		size_t old_stack_ptr = stack_ptr;
		expression(nodes, i, *this, file);
		func.push_back(IS::Pop({ stack_ptr - old_stack_ptr }));
		stack_ptr = old_stack_ptr;
	}

	func.push_back(IS::Ret{});
}

extern Program compile(
	const std::vector<AST::Node>& nodes,
	std::string_view file
) noexcept {
	Program program;

	for (size_t idx = 1; idx < nodes.size(); ++idx) if (nodes[idx]->depth == 0) {
		size_t stack_ptr = program.stack_ptr;
		expression(nodes, idx, program, file);
		emit(IS::Pop({ program.stack_ptr - stack_ptr }));
		program.stack_ptr = stack_ptr;
	}

	emit(IS::Exit());

	std::unordered_map<size_t, size_t> map_idx;
	for (size_t i = 0; i < program.functions.size(); ++i) {
		map_idx[i] = program.code.size();
		program.code.insert(
			std::end(program.code),
			std::begin(program.functions[i]),
			std::end(program.functions[i])
		);
	}

	for (auto& x : program.code) if (x.typecheck(IS::Instruction::Call_Kind))
		x.Call_.f_idx = map_idx[x.Call_.f_idx];

	return program;
}

void Program::debug() const noexcept {
	for (auto& x : code) x.debug();
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
	for (size_t ip = 0; ip < program.code.size(); ++ip) {
		auto inst = program.code[ip];

		#define BINARY_OP(k, op) case IS::Instruction::k : { \
			auto a = pop_stack<long double>(stack); \
			auto b = pop_stack<long double>(stack); \
			push_stack(a op b, stack); \
			break; \
		}

		switch(inst.kind) {
			case IS::Instruction::Constant_Kind: {
				push_stack(
					*reinterpret_cast<const long double*>(program.data.data() + inst.Constant_.ptr),
					stack
				);
				break;
			}
			BINARY_OP(Add_Kind, +);
			BINARY_OP(Sub_Kind, -);
			BINARY_OP(Mul_Kind, *);
			BINARY_OP(Div_Kind, /);
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
				push_stack(stack, memory.data() + inst.Load_.memory_ptr, inst.Load_.n);
				break;
			}
			case IS::Instruction::Save_Kind: {
				assert(memory.size() >= inst.Save_.memory_ptr + inst.Save_.n);
				memcpy(
					memory.data() + inst.Save_.memory_ptr,
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
				stack_frame.push_back(memory.size());
				ip = inst.Call_.f_idx - 1;
				break;
			}
			case IS::Instruction::Ret_Kind: {
				ip = call_stack.back() - 1;
				memory.resize(stack_frame.back());
				
				call_stack.pop_back();
				stack_frame.pop_back();

				break;
			}
			case IS::Instruction::Exit_Kind: {
				ip = program.code.size();
				break;
			}
		}

		size_t col = 0;
		col += printf("|");
		for (size_t i = 0; i < stack.size(); i += sizeof(long double)) {
			col +=  printf("%10.5Lf|", *reinterpret_cast<long double*>(stack.data() + i));
		}
		for (col %= 100; col < 100; ++col) printf(" ");
		inst.debug();
		fflush(stdout);
	}
}
