
#pragma once

#include <any>
#include <stack>
#include <vector>
#include <functional>
#include <string_view>
#include <unordered_map>
#include "AST.hpp"

// I guess you can't forward decl nested struct in c++ :)))))
// struct AST { struct Node; };
struct AST_Interpreter {

	std::vector<std::any> memory;

	// >PERF(Tackwin): std::string as key of unordered_map :'(
	// Ordered from out most scope to in most scope.
	std::vector<std::unordered_map<std::string, size_t>> variables;
	std::stack<size_t> limit_scope;

	struct Function_Definition {
		std::unordered_map<std::string_view, size_t> parameters;
		std::vector<std::string_view> parameter_names;
		std::vector<std::string> return_types;

		size_t start_idx = 0;
	};

	struct Builtin {
		std::function<size_t(std::vector<size_t>)> f;
	};

	struct Return_Call {
		std::vector<size_t> values;
	};

	struct Void {};

	struct Identifier {
		size_t memory_idx = 0;
	};

	struct Pointer {
		size_t memory_idx = 0;
	};

	struct User_Struct {
		std::vector<std::string> member_names;
		std::vector<size_t>      member_values;
	};

	using AST_Nodes = const std::vector<AST::Node>&;

	std::any litteral     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any unary_op     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any list_op      (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any factor       (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any expression   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any identifier   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any type_ident   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any assignement  (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any if_call      (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any for_loop     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any while_loop   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any function     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any function_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any return_call  (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any struct_def   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any init_list    (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;


	std::any interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any interpret(
		AST_Nodes nodes, const Function_Definition& f, std::string_view file
	) noexcept;

	std::any lookup(std::string_view id) noexcept;
	size_t   lookup_addr(std::string_view id) noexcept;

	size_t alloc(std::any x) noexcept;
	std::any at(size_t idx) noexcept;
	std::any at(Identifier id) noexcept { return at(id.memory_idx); }
	std::any at(Pointer ptr) noexcept { return at(ptr.memory_idx); }
	void push_scope() noexcept;
	void pop_scope() noexcept;

	void print_value(const std::any& value) noexcept;

	template<typename T>
	bool typecheck(const std::any& value) noexcept {
		return value.type() == typeid(std::decay_t<T>);
	}

	void push_builtin() noexcept;

	AST_Interpreter() noexcept { memory.push_back(nullptr); push_scope(); limit_scope.push(0); }
};
