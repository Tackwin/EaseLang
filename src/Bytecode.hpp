#pragma once

#include <new>
#include <any>
#include <vector>
#include <unordered_map>

#include "AST.hpp"
#include "xstd.hpp"

namespace IS {

	struct Constant {
		size_t ptr = 0;
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
	X(Print)

	struct Instruction {
		sum_type(Instruction, IS_LIST);

		void debug() const noexcept {
			printf("%s", name());
			if (typecheck(Constant_Kind)) {
				printf(" const:[%zu]", Constant_.ptr);
			}
			printf("\n");
		}
	};
};

struct Program {
	std::vector<std::uint8_t>    data;
	std::vector<IS::Instruction> code;

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
