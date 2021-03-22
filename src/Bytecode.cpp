#include "Bytecode.hpp"
#include "xstd.hpp"

#include <thread>

#define decl(name) size_t name(\
const std::vector<AST::Node>& nodes, size_t idx, Program& program, std::string_view file\
) noexcept

#define emit(x) program.code.push_back(x);

size_t alloc_constant(Program& prog, long double constant) noexcept {
	prog.data.resize(prog.data.size() + sizeof(constant));
	memcpy(prog.data.data() + prog.data.size() - sizeof(constant), &constant, sizeof(constant));
	return prog.data.size() - sizeof(constant);
}



decl(identifier   );
decl(assignement  );
decl(litteral     );
decl(list_op      );
decl(unary_op     );
decl(expression   );
decl(if_call      );
decl(for_loop     );
decl(while_loop   );
decl(function_call);
decl(return_call  );
decl(init_list    );
decl(array_access );


decl(identifier) {
	auto& node = nodes[idx].Identifier_;
	auto id = program.interpreter.lookup(string_view_from_view(file, node.token.lexeme));

	auto& type = program.interpreter.types.at(id.Identifier_.type_descriptor_id);

	size_t to = program.stack_ptr;
	emit(IS::Push({ type.get_size() }));
	emit(IS::Cpy({ id.Identifier_.memory_idx, to, type.get_size() }));

	program.stack_ptr += type.get_size();

	return type.get_unique_id();
}
decl(assignement) {
	auto& node = nodes[idx].Assignement_;

	auto name = string_view_from_view(file, node.identifier.lexeme);

	size_t type_hint = 0;

	AST_Interpreter::Identifier id;
	if (node.type_identifier) {
		auto t = program.interpreter.type_interpret(nodes, *node.type_identifier, file);
		type_hint = t.get_unique_id();
	}

	if (node.value_idx) {
		size_t type = expression(nodes, *node.value_idx, program, file);

		size_t n = program.interpreter.types.at(type).get_size();
		size_t to = program.stack_ptr;
		size_t from = program.stack_ptr - n;

		id.memory_idx = to;
		id.type_descriptor_id = type_hint;

		emit(IS::Push({ n }));
		emit(IS::Cpy({ from, to, n }));
		program.stack_ptr += n;
	} else if (type_hint) {
		id.memory_idx = program.stack_ptr;
		id.type_descriptor_id = type_hint;
		emit(IS::Push({ program.interpreter.types.at(type_hint).get_size() }));
		program.stack_ptr += program.interpreter.types.at(type_hint).get_size();
	}

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
		program.stack_ptr += 1;
		return AST_Interpreter::Bool_Type::unique_id;
	}
	if (node.token.type == Token::Type::False) {
		emit(IS::False{});
		program.stack_ptr += 1;
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
	case AST::Operator::Plus:  emit(IS::Add{}); program.stack_ptr -= sizeof(long double); break;
	case AST::Operator::Minus: emit(IS::Sub{}); program.stack_ptr -= sizeof(long double); break;
	case AST::Operator::Star:  emit(IS::Mul{}); program.stack_ptr -= sizeof(long double); break;
	}
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
decl(expression) {
	auto& node = nodes[idx];
	auto in = nodes[idx].Expression_.inner_idx;

	switch (node.kind) {
	case AST::Node::Identifier_Kind:          return identifier   (nodes, idx, program, file);
	case AST::Node::Assignement_Kind:         return assignement  (nodes, idx, program, file);
	case AST::Node::Litteral_Kind:            return litteral     (nodes, idx, program, file);
	case AST::Node::Operation_List_Kind:      return list_op      (nodes, idx, program, file);
	case AST::Node::Unary_Operation_Kind:     return unary_op     (nodes, idx, program, file);
	case AST::Node::Expression_Kind:          return expression   (nodes, in , program, file);
	case AST::Node::If_Kind:                  return if_call      (nodes, idx, program, file);
	case AST::Node::For_Kind:                 return for_loop     (nodes, idx, program, file);
	case AST::Node::While_Kind:               return while_loop   (nodes, idx, program, file);
	case AST::Node::Function_Call_Kind:       return function_call(nodes, idx, program, file);
	case AST::Node::Return_Call_Kind:         return return_call  (nodes, idx, program, file);
	case AST::Node::Initializer_List_Kind:    return init_list    (nodes, idx, program, file);
	case AST::Node::Array_Access_Kind:        return array_access (nodes, idx, program, file);
	default:                                  return 0;
	}
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
	}
	return 0;
}
decl(return_call) {
	return 0;
}
decl(init_list) {
	return 0;
}
decl(array_access) {
	return 0;
}

extern Program compile(
	const std::vector<AST::Node>& nodes,
	std::string_view file
) noexcept {
	Program program;

	for (size_t idx = 1; idx < nodes.size(); ++idx) if (nodes[idx]->depth == 0) {
		expression(nodes, idx, program, file);
	}

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
			case IS::Instruction::Cpy_Kind: {
				memcpy(
					stack.data() + inst.Cpy_.to,
					stack.data() + inst.Cpy_.from,
					inst.Cpy_.n
				);
				break;
			}
		}

		printf("|");
		for (size_t i = 0; i < stack.size(); i += sizeof(long double)) {
			printf("%10.5Lf|", *reinterpret_cast<long double*>(stack.data() + i));
		}
		printf("\n");
	}
}
