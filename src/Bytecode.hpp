#pragma once

#include <new>
#include <any>
#include <vector>
#include <unordered_map>

#include "xstd.hpp"
#include "Interpreter.hpp"

struct Program;
namespace IS {

	struct Constant {
		size_t ptr = 0;
		size_t n = 0;
	};
	struct Constantf {
		size_t ptr = 0;
	};
	struct Push {
		size_t n = 0;
	};
	struct Alloc {
		size_t n = 0;
	};
	struct Pop {
		size_t n = 0;
	};
	struct Stack_Load {
		size_t memory_ptr = 0;
		size_t n = 0;
	};
	struct Load_At {
		size_t n = 0;
	};
	struct Save {
		size_t memory_ptr = 0;
		size_t n = 0;
	};
	struct Call {
		size_t f_idx = 0;
		size_t n = 0;
	};
	struct Call_At {
		size_t n = 0;
	};
	struct Ret {
		size_t n = 0;
	};
	struct Jmp_Rel {
		int dt_ip = 0;
	};
	struct If_Jmp_Rel {
		int dt_ip = 0;
	};
	struct Int {};
	struct Exit {};
	struct True  {};
	struct False {};
	struct Neg {};
	struct Not {};
	struct Inc {};
	struct Add {};
	struct Sub {};
	struct Eq {};
	struct Neq {};
	struct Lt {};
	struct Leq {};
	struct Gt {};
	struct Div {};
	struct Mul {};
	struct Mod {};
	struct Print {};
	struct Print_Byte {};
	struct Sleep {};
	struct CB2R {};
	struct Load_Rsp {};


	#define IS_LIST(X)\
	X(Constant) X(Neg) X(Not) X(Add) X(Sub) X(Mul) X(Div) X(True) X(False) X(Neq) \
	X(Print) X(Push) X(Pop) X(Stack_Load) X(Save) X(Alloc) X(Call) X(Ret) \
	X(Exit) X(Sleep) X(Eq) X(Gt) X(Lt) X(Jmp_Rel) X(If_Jmp_Rel) X(Mod) X(Inc) X(Call_At)\
	X(Constantf) X(Int) X(Leq) X(Load_At) X(Print_Byte) X(CB2R) X(Load_Rsp)

	struct Instruction {
		sum_type(Instruction, IS_LIST);

		void debug(const Program& program) const noexcept;
	};
};

struct Program {
	std::vector<std::uint8_t>    data;
	std::vector<IS::Instruction> code;
	std::vector<std::vector<IS::Instruction>> functions;

	size_t current_function_idx = 0;
	std::unordered_map<size_t, std::string> annotations;

	size_t stack_ptr = 0;
	size_t memory_stack_ptr = 0;

	AST_Interpreter interpreter;

	void compile_function(
		const std::vector<AST::Node>& nodes,
		size_t idx,
		std::string_view file
	) noexcept;
	std::vector<IS::Instruction>* get_current_function() noexcept;

	void debug() const noexcept;
};

extern Program compile(
	const std::vector<AST::Node>& nodes,
	std::string_view file
) noexcept;

struct Bytecode_VM {
	std::vector<std::uint8_t> stack;
	std::vector<std::uint8_t> memory;

	std::vector<size_t> call_stack;
	std::vector<size_t> stack_frame;
	std::vector<size_t> memory_stack_frame;

	size_t immediate_register = 0;

	void execute(const Program& prog) noexcept;

};
