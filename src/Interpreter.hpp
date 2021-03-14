
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
	struct Hasher {
		size_t operator()(std::string_view str) const noexcept {
			size_t x = 0;
			for (auto& c : str) x = hash_combine(x, c);
			return x;
		}
	};

	struct String { std::string x; };
	struct Bool   { bool          x = 0; };
	struct Real   { long double   x = 0; };
	struct Identifier {
		size_t memory_idx = 0;
		size_t type_descriptor_id = 0;
	};
	struct Pointer {
		size_t memory_idx = 0;
		size_t type_descriptor_id = 0;
	};
	struct Array_View {
		size_t length = 0;
		size_t memory_idx = 0;
		size_t type_descriptor_id = 0;
	};
	struct Return_Call {
		std::vector<Identifier> values;
	};
	struct Builtin {
		std::function<Identifier(std::vector<Identifier>)> f;
	};

	#define LIST_LANG_VALUE(X)\
	X(Identifier) X(Pointer) X(Real) X(Return_Call) X(Bool) X(Builtin) X(Array_View) X(String)

	struct Value { sum_type(Value, LIST_LANG_VALUE); };


	struct Bool_Type   { static constexpr size_t unique_id = 1; };
	struct Real_Type   { static constexpr size_t unique_id = 2; };
	struct Byte_Type   { static constexpr size_t unique_id = 3; };
	struct Nat_Type    { static constexpr size_t unique_id = 4; };
	struct Int_Type    { static constexpr size_t unique_id = 5; };
	struct Void_Type   { static constexpr size_t unique_id = 6; };
	struct Pointer_Type {
		static constexpr size_t combine_id = 0;
		size_t unique_id = 0;
		size_t user_type_descriptor_idx = 0;
	};
	struct Array_View_Type {
		static constexpr size_t combine_id = 1;
		size_t unique_id = 0;
		size_t user_type_descriptor_idx = 0;
		size_t length = 0;
	};
	struct User_Function_Type {
		static constexpr size_t return_separator_id = 0;
		size_t unique_id = 0;
		size_t start_idx = 0;
		size_t byte_size = 8;
		std::vector<size_t>           parameter_type;
		std::vector<std::string_view> parameter_name;

		std::vector<size_t>           return_type;
	};
	struct User_Struct_Type {
		size_t unique_id = 0;
		size_t byte_size = 8;

		std::unordered_map<std::string_view, size_t, Hasher> name_to_idx;
		std::vector<size_t> member_types;
		std::vector<size_t> member_offsets;
		std::vector<Value>  default_values;
	};

	#define LIST_LANG_TYPES(X)\
		X(Nat_Type) X(Real_Type) X(Int_Type) X(User_Struct_Type) X(User_Function_Type)\
		X(Pointer_Type) X(Array_View_Type) X(Bool_Type) X(Byte_Type) X(Void_Type)

	struct Type {
		sum_type(Type, LIST_LANG_TYPES);
	
		size_t get_size() noexcept {
			switch (kind) {
			case Void_Type_Kind: return 0;
			case Bool_Type_Kind: return 1;
			case Byte_Type_Kind: return 1;
			case Nat_Type_Kind: return sizeof(std::uint64_t);
			case Int_Type_Kind: return sizeof(std::int64_t);
			case Real_Type_Kind: return sizeof(long double);
			case Pointer_Type_Kind : return sizeof(size_t);
			case Array_View_Type_Kind   : return sizeof(size_t) + sizeof(size_t);
			case User_Struct_Type_Kind  : return User_Struct_Type_.byte_size;
			case User_Function_Type_Kind: return User_Function_Type_.byte_size;
			default: return 0;
			}
		}

		size_t get_unique_id() noexcept {
			switch (kind) {
			case Void_Type_Kind       : return Void_Type::unique_id;
			case Bool_Type_Kind       : return Bool_Type::unique_id;
			case Byte_Type_Kind       : return Byte_Type::unique_id;
			case Nat_Type_Kind        : return Nat_Type::unique_id;
			case Real_Type_Kind       : return Real_Type::unique_id;
			case Int_Type_Kind        : return Int_Type::unique_id;
			case Pointer_Type_Kind    : return Pointer_Type_.unique_id;
			case Array_View_Type_Kind : return Array_View_Type_.unique_id;
			case User_Struct_Type_Kind: return User_Struct_Type_.unique_id;
			default: return 0;
			}
		}
	};

	std::vector<std::uint8_t> memory;
	std::unordered_map<std::string_view, size_t, Hasher> type_name_to_hash;
	std::unordered_map<size_t, Type> types;

	// Ordered from out most scope to in most scope.
	std::vector<std::unordered_map<std::string_view, Value, Hasher>> variables;

	std::unordered_map<std::string_view, Builtin, Hasher> builtins;

	using AST_Nodes = const std::vector<AST::Node>&;

	Value litteral     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value unary_op     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value list_op      (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value factor       (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value expression   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value identifier   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value assignement  (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value if_call      (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value for_loop     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value while_loop   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value function_call(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value array_access (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value return_call  (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value init_list    (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;

	Type  function     (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Type  type_ident   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Type  struct_def   (AST_Nodes nodes, size_t idx, std::string_view file) noexcept;

	Type  type_interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value      interpret(AST_Nodes nodes, size_t idx, std::string_view file) noexcept;
	Value      interpret(
		AST_Nodes nodes, const User_Function_Type& f, std::string_view file
	) noexcept;

	Type  create_pointer_type(size_t underlying) noexcept;
	Type  create_array_view_type(size_t underlying, size_t size) noexcept;
	Type  type_of(const Value& value) noexcept;
	Type  type_lookup(std::string_view id) noexcept;
	Value      lookup(std::string_view id) noexcept;

	size_t alloc(size_t n_byte) noexcept;
	Identifier create_id(const Value& from) noexcept;
	size_t copy(const Value& from, size_t to) noexcept;

	Value at(Identifier id) noexcept;
	size_t read_ptr(size_t ptr) noexcept;
	void write_ptr(size_t ptr, size_t to) noexcept;

	void push_scope() noexcept;
	void pop_scope() noexcept;

	void print_value(const Value& value) noexcept;

	void push_builtin() noexcept;

	size_t get_type_id(const Value& x) noexcept;

	AST_Interpreter() noexcept { memory.push_back(0); push_scope(); }
};
