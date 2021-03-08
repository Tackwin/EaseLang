
#pragma once

#include <any>
#include <stack>
#include <vector>
#include <functional>
#include <string_view>
#include <unordered_map>
#include "AST.hpp"
#include "xstd.hpp"

// I guess you can't forward decl nested struct in c++ :)))))
// struct AST { struct Node; };
struct AST_Interpreter {

	struct Identifier {
		size_t memory_idx = 0;
		size_t type_descriptor_id = 0;
	};
	struct Pointer : Identifier { };
	struct Array   : Identifier { size_t length = 0; };

	struct Void { };
	struct Real { };
	struct Int  { };
	struct User_Type_Descriptor {
		size_t unique_id = 0;
		size_t byte_size = 8;

		std::unordered_map<std::string, size_t> name_to_idx;
		std::vector<Identifier> members;
		std::vector<std::any> default_values;
	};
	struct Pointer_Type { size_t user_type_descriptor_idx = 0; };
	struct Array_Type   { size_t user_type_descriptor_idx = 0; size_t length = 0; };

	#define LIST_TYPES(X)\
		X(Void) X(Real) X(Int) X(User_Type_Descriptor) X(Pointer_Type) X(Array_Type)

	struct Type {
		sum_type(Type, LIST_TYPES);
	
		size_t get_size() noexcept {
			switch (kind) {
			case Void_Kind: return 0;
			case Real_Kind: return 8;
			case Int_Kind:  return 8;
			case Pointer_Type_Kind : return 8;
			case Array_Type_Kind   : return 8 + 8;
			case User_Type_Descriptor_Kind:  return User_Type_Descriptor_.byte_size;
			default: return 0;
			}
		}
	};

	std::vector<std::uint8_t> memory;
	std::unordered_map<size_t, Type> types;

	// >PERF(Tackwin): std::string as key of unordered_map :'(
	// Ordered from out most scope to in most scope.
	std::vector<std::unordered_map<std::string, Identifier>> variables;
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
	std::unordered_map<std::string, Builtin> builtins;

	struct Return_Call {
		std::vector<size_t> values;
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
	std::any array_access (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any return_call  (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any struct_def   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any init_list    (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;

	std::any interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any interpret(
		AST_Nodes nodes, const Function_Definition& f, std::string_view file
	) noexcept;

	size_t interpret_type(AST_Nodes nodes, size_t idx, std::string_view file) noexcept { return 0; }

	std::any type_lookup(std::string_view id) noexcept;
	std::any lookup(std::string_view id) noexcept;
	size_t   lookup_addr(std::string_view id) noexcept;

	size_t alloc(std::any x) noexcept;
	size_t alloc(size_t n_bytes) noexcept {return 0;}
	void deep_copy(Identifier from, size_t to) noexcept {}

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

	AST_Interpreter() noexcept { memory.push_back(0); push_scope(); limit_scope.push(0); }
};
