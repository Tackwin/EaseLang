
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
	struct Pointer : Identifier {};


	struct Void   { static constexpr size_t unique_id = 0; };
	struct Real   { static constexpr size_t unique_id = 1; };
	struct Int    { static constexpr size_t unique_id = 2; };
	struct Pointer_Type {
		static constexpr size_t unique_id = 3;
		size_t user_type_descriptor_idx = 0;
	};
	struct Array_Type {
		static constexpr size_t unique_id = 4;
		size_t user_type_descriptor_idx = 0;
		size_t length = 0;
	};
	struct User_Function_Type {
		size_t unique_id = 0;
		size_t start_idx = 0;
		std::vector<size_t>           parameter_type;
		std::vector<std::string_view> parameter_name;

		std::vector<size_t>           return_type;
	};
	struct User_Type_Descriptor {
		size_t unique_id = 0;
		size_t byte_size = 8;

		std::unordered_map<std::string, size_t> name_to_idx;
		std::vector<size_t> member_types;
		std::vector<size_t> member_offsets;
		std::vector<std::any> default_values;
	};

	#define LIST_TYPES(X)\
		X(Void) X(Real) X(Int) X(User_Type_Descriptor) X(User_Function_Type)\
		X(Pointer_Type) X(Array_Type)

	struct Type {
		sum_type(Type, LIST_TYPES);
	
		size_t get_size() noexcept {
			switch (kind) {
			case Void_Kind: return 0;
			case Real_Kind: return sizeof(long double);
			case Int_Kind:  return sizeof(std::int64_t);
			case Pointer_Type_Kind : return sizeof(size_t);
			case Array_Type_Kind   : return sizeof(size_t) + sizeof(size_t);
			case User_Type_Descriptor_Kind:  return User_Type_Descriptor_.byte_size;
			default: return 0;
			}
		}

		size_t get_unique_id() noexcept {
			switch (kind) {
			case Void_Kind: return Void::unique_id;
			case Real_Kind: return Real::unique_id;
			case Int_Kind:  return Int::unique_id;
			case Pointer_Type_Kind : return Pointer_Type::unique_id;
			case Array_Type_Kind   : return Array_Type::unique_id;
			case User_Type_Descriptor_Kind:  return User_Type_Descriptor_.unique_id;
			default: return 0;
			}
		}

		const char* name() noexcept {
			#define X(x) case x##_Kind: return #x;
			switch(kind) { LIST_TYPES(X) default: return "??"; };
			#undef X
		}
	};

	std::vector<std::uint8_t> memory;
	std::unordered_map<std::string, size_t> type_name_to_hash;
	std::unordered_map<size_t, Type> types;
	size_t Type_N = 0;

	// >PERF(Tackwin): std::string as key of unordered_map :'(
	// Ordered from out most scope to in most scope.
	std::vector<std::unordered_map<std::string, Identifier>> variables;


	struct Builtin {
		std::function<Identifier(std::vector<Identifier>)> f;
	};
	std::unordered_map<std::string, Builtin> builtins;

	struct Return_Call {
		std::vector<Identifier> values;
	};

	using AST_Nodes = const std::vector<AST::Node>&;

	std::any litteral     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any unary_op     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any list_op      (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any factor       (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any expression   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any identifier   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
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

	Type     type_ident   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;

	std::any interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	std::any interpret(
		AST_Nodes nodes, const User_Function_Type& f, std::string_view file
	) noexcept;

	Type type_lookup(std::string_view id) noexcept;
	std::any lookup(std::string_view id) noexcept;
	size_t   lookup_addr(std::string_view id) noexcept;

	size_t alloc(size_t n_byte) noexcept;
	size_t push(std::any x) noexcept;
	size_t copy(std::any from, size_t to) noexcept;

	std::any at(Identifier id) noexcept;
	size_t read_ptr(size_t at) noexcept;

	void push_scope() noexcept;
	void pop_scope() noexcept;

	void print_value(const std::any& value) noexcept;

	void push_builtin() noexcept;

	AST_Interpreter() noexcept { memory.push_back(0); push_scope(); }
};
