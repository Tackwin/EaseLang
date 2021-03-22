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
	struct Cpy {
		size_t from = 0;
		size_t to = 0;
		size_t n = 0;
	};
	struct True  {};
	struct False {};
	struct Neg {};
	struct Not {};
	struct Add {};
	struct Sub {};
	struct Div {};
	struct Mul {};
	struct Print {};


	#define IS_LIST(X)\
	X(Constant) X(Neg) X(Not) X(Add) X(Sub) X(Mul) X(Div) X(True) X(False) \
	X(Print) X(Push) X(Cpy)

	struct Instruction {
		sum_type(Instruction, IS_LIST);

		void debug() const noexcept {
			printf("%s", name());
			if (typecheck(Constant_Kind)) {
				printf(" const:[%zu]", Constant_.ptr);
			}
			if (typecheck(Push_Kind)) {
				printf(" %zu", Push_.n);
			}
			if (typecheck(Cpy_Kind)) {
				printf(" stack:[%zu], stack:[%zu], %zu ; from, to, n", Cpy_.from, Cpy_.to, Cpy_.n);
			}
			printf("\n");
		}
	};
};

struct Program {
	std::vector<std::uint8_t>    data;
	std::vector<IS::Instruction> code;

	size_t stack_ptr = 0;

	AST_Interpreter interpreter;


	void debug() const noexcept;
};

extern Program compile(
	const std::vector<AST::Node>& nodes,
	std::string_view file
) noexcept;

struct Bytecode_VM {
	std::vector<std::uint8_t> stack;

	size_t immediate_register = 0;

	void execute(const Program& prog) noexcept;

};
