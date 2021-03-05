#include "Bytecode.hpp"
#include "xstd.hpp"

#include <thread>

extern std::vector<Instruction_Set::Instruction> compile(
	const std::vector<AST::Node>& nodes,
	std::string_view file
) noexcept {
	// >TODO(Tackwin): lol
	return {};
}

std::any Bytecode_VM::interpret(Bytecodes instr, size_t ip) noexcept {
	while (ip < instr.size()) {
		auto& it = instr[ip];

		switch (it.kind) {
			case Instruction_Set::Instruction::Push_Kind: {
				stack.push_back(it.Push_.x);
				ip++;
				break;
			};
			case Instruction_Set::Instruction::Pop_Kind: {
				if (stack.empty()) {
					printlns("Error, can't pop empty stack.");
					return nullptr;
				}
				stack.pop_back();
				ip++;
				break;
			};
			case Instruction_Set::Instruction::Jump_Kind: {
				ip = it.Jump_.to;
				break;
			};
			case Instruction_Set::Instruction::If_Jump_Kind: {
				if (stack.size() <= it.If_Jump_.cond) {
					println("Error out of bound If-jump instruction (cond %zu).", it.If_Jump_.cond);
					return nullptr;
				}
				if (stack[it.If_Jump_.cond]) ip = it.Jump_.to;
				else                         ip++;
				break;
			};
			case Instruction_Set::Instruction::Set_Kind: {
				if (stack.size() <= it.Set_.dst) {
					println("Error out of bound Set instruction (dst %zu).", it.Set_.dst);
					return nullptr;
				}
				stack[it.Set_.dst] = it.Set_.x;
				ip++;
				break;
			};
			case Instruction_Set::Instruction::Add_Kind: {
				if (stack.size() <= it.Add_.ans) {
					println("Error out of bound Addd instruction (ans %zu).", it.Add_.ans);
					return nullptr;
				}
				if (stack.size() <= it.Add_.x) {
					println("Error out of bound Addd instruction (x %zu).", it.Add_.x);
					return nullptr;
				}
				if (stack.size() <= it.Add_.y) {
					println("Error out of bound Addd instruction (y %zu).", it.Add_.y);
					return nullptr;
				}
				
				stack[it.Add_.ans] = stack[it.Add_.x] + stack[it.Add_.y];
				ip++;
				break;
			}
			case Instruction_Set::Instruction::Print_Kind: {
				printf("%c", (char)it.Print_.c);
				ip++;
				break;
			};
			case Instruction_Set::Instruction::Sleep_Kind: {
				std::this_thread::sleep_for(
					std::chrono::nanoseconds((size_t)(1'000'000'000 * it.Sleep_.t))
				);
				ip++;
				break;
			};
			default:
				printlns("Unsupported operation.");
				break;
		}

	}
	return nullptr;
}

