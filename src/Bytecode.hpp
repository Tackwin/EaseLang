#pragma once

#include <new>
#include <any>
#include <vector>
#include <unordered_map>

#include "xstd.hpp"
#include "Interpreter.hpp"

namespace IS {

	struct Constant {
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
	struct Load {
		size_t memory_ptr = 0;
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
	struct Ret {
		size_t n = 0;
	};
	struct Jmp_Rel {
		int dt_ip = 0;
	};
	struct If_Jmp_Rel {
		int dt_ip = 0;
	};
	struct Exit {};
	struct True  {};
	struct False {};
	struct Neg {};
	struct Not {};
	struct Add {};
	struct Sub {};
	struct Eq {};
	struct Lt {};
	struct Div {};
	struct Mul {};
	struct Print {};
	struct Sleep {};


	#define IS_LIST(X)\
	X(Constant) X(Neg) X(Not) X(Add) X(Sub) X(Mul) X(Div) X(True) X(False) \
	X(Print) X(Push) X(Pop) X(Load) X(Save) X(Alloc) X(Call) X(Ret) \
	X(Exit) X(Sleep) X(Eq) X(Lt) X(Jmp_Rel) X(If_Jmp_Rel)

	struct Instruction {
		sum_type(Instruction, IS_LIST);

		void debug() const noexcept {
			printf("%-10s", name());
			if (typecheck(Constant_Kind)) {
				printf(": const:[%zu]", Constant_.ptr);
			}
			if (typecheck(Push_Kind)) {
				printf(": %zu", Push_.n);
			}
			if (typecheck(Call_Kind)) {
				printf(": f:[%zu], %zu", Call_.f_idx, Call_.n);
			}
			if (typecheck(Ret_Kind)) {
				printf(": %zu", Ret_.n);
			}
			if (typecheck(Pop_Kind)) {
				printf(": %zu", Pop_.n);
			}
			if (typecheck(Jmp_Rel_Kind)) {
				printf(": %d", Jmp_Rel_.dt_ip);
			}
			if (typecheck(If_Jmp_Rel_Kind)) {
				printf(": %d", If_Jmp_Rel_.dt_ip);
			}
			if (typecheck(Alloc_Kind)) {
				printf(": %zu", Alloc_.n);
			}
			if (typecheck(Load_Kind)) {
				printf(": mem:[%zu], %zu", Load_.memory_ptr, Load_.n);
			}
			if (typecheck(Save_Kind)) {
				printf(": mem:[%zu], %zu", Save_.memory_ptr, Save_.n);
			}
		}
		void debugln() const noexcept {
			debug();
			printf("\n");
		}
	};
};

struct Program {
	std::vector<std::uint8_t>    data;
	std::vector<IS::Instruction> code;
	std::vector<std::vector<IS::Instruction>> functions;

	std::vector<IS::Instruction>* current_function = &code;
	std::unordered_map<size_t, std::string> annotations;

	size_t stack_ptr = 0;
	size_t memory_stack_ptr = 0;

	AST_Interpreter interpreter;

	void compile_function(
		const std::vector<AST::Node>& nodes,
		size_t idx,
		std::vector<IS::Instruction>& func,
		std::string_view file
	) noexcept;

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
