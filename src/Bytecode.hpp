#pragma once

#include <new>
#include <any>
#include <vector>

#include "AST.hpp"

namespace Instruction_Set {

	struct Push {
		size_t x = 0;
	};
	struct Pop {};
	struct Jump {
		size_t to = 0;
	};
	struct If_Jump {
		size_t cond = 0;
		size_t to = 0;
	};
	struct Set {
		size_t x = 0;
		size_t dst = 0;
	};
	struct Add {
		size_t ans = 0;
		size_t x = 0;
		size_t y = 0;
	};
	struct Print {
		size_t c = 0;
	};
	struct Sleep {
		size_t t = 0;
	};

	#define IS_LIST\
	X(Push)\
	X(Pop)\
	X(Set)\
	X(If_Jump)\
	X(Jump)\
	X(Add)\
	X(Print)\
	X(Sleep)

	struct Instruction {
		enum Kind {
			None_Kind = 0
			#define X(x), x##_Kind
			IS_LIST
			#undef X
		} kind;

		union {
			#define X(x) x x##_;
			IS_LIST
			#undef X
		};

		#define X(x) Instruction(x y) noexcept { kind = x##_Kind; new (&x##_) x; x##_ = (y); }
		IS_LIST
		#undef X

		Instruction(std::nullptr_t) noexcept { kind = None_Kind; }

		Instruction(Instruction&& that) noexcept {
			kind = that.kind;
			switch (kind) {
				#define X(x) case x##_Kind: new(&x##_) x; x##_ = that.x##_; break;
				IS_LIST
				#undef X
				default: break;
			}
		}

	};
};

extern std::vector<Instruction_Set::Instruction> compile(
	const std::vector<AST::Node>& nodes,
	std::string_view file
) noexcept;

struct Bytecode_VM {
	using Bytecodes = const std::vector<Instruction_Set::Instruction>&;

	std::vector<size_t> stack;

	size_t immediate_register = 0;

	std::any interpret(Bytecodes instr, size_t ip) noexcept;
	void print(std::any x) noexcept;
};
