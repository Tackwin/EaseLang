#include "Bytecode.hpp"
#include "xstd.hpp"

#include <thread>

#define decl(name) void name(\
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


decl(identifier) {}  // identifier are effect free expressions in itself
decl(assignement) {
	auto& node = nodes[idx].Assignement_;

	auto name = string_view_from_view(file, node.identifier.lexeme);
	

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
		long double x = std::strtod(temp.c_str(), &end_ptr);

		emit(IS::Constant{ alloc_constant(program, x) });
		return;
	}
	if (node.token.type == Token::Type::True) {
		emit(IS::True{});
		return;
	}
	if (node.token.type == Token::Type::False) {
		emit(IS::False{});
		return;
	}
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
}
decl(unary_op) {
	auto& node = nodes[idx].Unary_Operation_;

	expression(nodes, node.right_idx, program, file);

	switch(node.op) {
		case AST::Operator::Minus: emit(IS::Neg{}); break;
		case AST::Operator::Not:   emit(IS::Not{}); break;
	}
}
decl(expression) {
	auto& node = nodes[idx];
	auto in = nodes[idx].Expression_.inner_idx;

	switch (node.kind) {
	case AST::Node::Identifier_Kind:          identifier   (nodes, idx, program, file); break;
	case AST::Node::Assignement_Kind:         assignement  (nodes, idx, program, file); break;
	case AST::Node::Litteral_Kind:            litteral     (nodes, idx, program, file); break;
	case AST::Node::Operation_List_Kind:      list_op      (nodes, idx, program, file); break;
	case AST::Node::Unary_Operation_Kind:     unary_op     (nodes, idx, program, file); break;
	case AST::Node::Expression_Kind:          expression   (nodes, in , program, file); break;
	case AST::Node::If_Kind:                  if_call      (nodes, idx, program, file); break;
	case AST::Node::For_Kind:                 for_loop     (nodes, idx, program, file); break;
	case AST::Node::While_Kind:               while_loop   (nodes, idx, program, file); break;
	case AST::Node::Function_Call_Kind:       function_call(nodes, idx, program, file); break;
	case AST::Node::Return_Call_Kind:         return_call  (nodes, idx, program, file); break;
	case AST::Node::Initializer_List_Kind:    init_list    (nodes, idx, program, file); break;
	case AST::Node::Array_Access_Kind:        array_access (nodes, idx, program, file); break;
	default:                                  nullptr;
	}
}
decl(if_call) {}
decl(for_loop) {}
decl(while_loop) {}
decl(function_call) {
	auto& node = nodes[idx].Function_Call_;
	auto name = string_view_from_view(file, nodes[node.identifier_idx].Identifier_.token.lexeme);
	if (name == "print") {
		expression(
			nodes, nodes[node.argument_list_idx].Argument_.value_idx, program, file
		);
		emit(IS::Print{});
	}
}
decl(return_call) {}
decl(init_list) {}
decl(array_access) {}

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
		}
	}
}
