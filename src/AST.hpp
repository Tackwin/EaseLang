#pragma once

#include <new>
#include <tuple>
#include <vector>
#include <string>
#include <optional>


#include "xstd.hpp"
#include "Tokenizer.hpp"

struct AST {
	struct Source_Code_Loc {
		size_t line   = 0;
		size_t offset = 0;
		size_t length = 0;
	};

	struct Statement {
		virtual std::string string(std::string_view, const AST& expressions) const noexcept
		{
			return "statement;\n";
		}

		size_t depth          = 0;
		size_t scope          = 0;
		size_t next_statement = 0;

		Source_Code_Loc loc;
	};

	struct Group_Expression : Statement {
		size_t inner_idx = 0;

		virtual std::string string(std::string_view str, const AST& expr) const noexcept {
			return "(" + expr.nodes[inner_idx]->string(str, expr) + ")";
		}
	};

	struct Group_Statement : Statement {
		size_t inner_idx = 0;
		virtual std::string string(std::string_view str, const AST& expr) const noexcept {
			std::string res = "";
			for (size_t idx = inner_idx; idx; idx = expr.nodes[idx]->next_statement) {
				res += expr.nodes[idx]->string(str, expr) + ";\n";
			}
			append_tab(1, res);
			return std::string("{\n") + res + "}\n";
		}
	};

	enum class Operator {
		Null = 0,
		Minus,
		Plus,
		Geq,
		Leq,
		Gt,
		Lt,
		Eq,
		Neq,
		And,
		Mod,
		Not,
		Div,
		Assign,
		Amp,
		Inc,
		Star,
		Deref,
		Dot,
		As,
		Function_Call,
		Array_Call,
		Or
	};
	static const char* op_to_string(Operator x) noexcept {
		switch (x) {
		case Operator::Minus:    return "-";
		case Operator::Plus:     return "+";
		case Operator::Geq:      return ">=";
		case Operator::Leq:      return "<=";
		case Operator::Gt:       return ">";
		case Operator::Lt:       return "<";
		case Operator::Mod:      return "%";
		case Operator::Eq:       return "==";
		case Operator::Neq:      return "!=";
		case Operator::And:      return "&&";
		case Operator::Or:       return "||";
		case Operator::Not:      return "!";
		case Operator::Inc:      return "++";
		case Operator::Div:      return "/";
		case Operator::Dot:      return ".";
		case Operator::Amp:      return "&";
		case Operator::Star:     return "*";
		case Operator::Deref:    return "*";
		case Operator::Assign:   return "=";
		case Operator::As:       return "as";
		default: return "??";
		}
	}

	struct Unary_Operation : Statement {
		Operator op;
		size_t right_idx = 0;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res;
			res += op_to_string(op);
			res += expressions.nodes[right_idx]->string(file, expressions);
			return res;
		}
	};

	struct Operation_List : Statement {
		size_t left_idx = 0;
		size_t rest_idx = 0;
		Operator op = Operator::Null;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res;

			res = expressions.nodes[left_idx]->string(file, expressions);

			for (size_t idx = rest_idx; idx; idx = expressions.nodes[idx]->next_statement) {
				if (op != Operator::Dot) res += " ";
				res += op_to_string(op);
				if (op != Operator::Dot) res += " ";
				res += expressions.nodes[idx]->string(file, expressions);
			}

			return res;
		}
	};


	struct Identifier : Statement {
		Token token;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			return string_from_view(file, token.lexeme);
		}
	};

	struct Type_Identifier : Statement {

		Token identifier;
		bool is_const = false;
		std::optional<size_t> array_to = std::nullopt;
		std::optional<size_t> array_size = std::nullopt;
		std::optional<size_t> pointer_to = std::nullopt;

		bool is_proc = false;
		size_t parameter_type_list_idx = 0;
		size_t return_type_list_idx = 0;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res = "";

			if (pointer_to) {
				res = expressions.nodes[*pointer_to]->string(file, expressions);
				res += "*";
			} else if (array_to) {
				res = expressions.nodes[*array_to]->string(file, expressions);
				res += "[";
				res += expressions.nodes[*array_size]->string(file, expressions);
				res += "]";
			} else if (is_proc) {
				res = "proc";
				char sep = '(';
				for (
					size_t idx = parameter_type_list_idx;
					idx;
					idx = expressions.nodes[idx]->next_statement
				) {
					res += sep;
					res += expressions.nodes[idx]->string(file, expressions);
					sep = ',';
				}
				res += ')';
				if (return_type_list_idx) {
					res += " -> ";
				}
				sep = '(';
				for (
					size_t idx = return_type_list_idx;
					idx;
					idx = expressions.nodes[idx]->next_statement
				) {
					res += sep;
					res += expressions.nodes[idx]->string(file, expressions);
					sep = ',';
				}
				res += ')';
			} else {
				res = string_from_view(file, identifier.lexeme);
			}

			if (is_const) res += " const";
			return res;
		}
	};

	struct Litteral : Statement {
		Token token;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override { return string_from_view(file, token.lexeme); }
	};

	struct Function_Call : Statement {
		size_t identifier_idx = 0;
		size_t argument_list_idx = 0;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res;
			res += expressions.nodes[identifier_idx]->string(file, expressions);
			res += "(";

			auto idx = argument_list_idx;

			while(idx) {
				res += expressions.nodes[idx]->string(file, expressions);
				idx = expressions.nodes[idx]->next_statement;
				if (idx) res += ", ";
			}
			res += ")";
			return res;
		}
	};

	struct Array_Access : Statement {
		size_t identifier_array_idx = 0;
		size_t identifier_acess_idx = 0;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res;
			res += expressions.nodes[identifier_array_idx]->string(file, expressions);
			res += "[";
			res += expressions.nodes[identifier_acess_idx]->string(file, expressions);
			res += "]";
			return res;
		}
	};

	struct Return_Call : Statement {
		size_t return_value_idx = 0;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res = "return ";
			res += expressions.nodes[return_value_idx]->string(file, expressions);
			return res;
		}
	};

	struct Struct_Definition : Statement {
		size_t struct_line_idx = 0;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res;
			for (size_t idx = struct_line_idx; idx; idx = expressions.nodes[idx]->next_statement)
				res += expressions.nodes[idx]->string(file, expressions) + ";\n";

			append_tab(1, res);
			return std::string("struct {\n") + res + "}";
		}
	};

	struct Initializer_List : Statement {
		std::optional<size_t> type_identifier;
		size_t expression_list_idx = 0;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res;
			if (type_identifier)
				res += expressions.nodes[*type_identifier]->string(file, expressions);
			res += "{";

			std::string separator = " ";
			for (
				size_t idx = expression_list_idx; idx; idx = expressions.nodes[idx]->next_statement
			) {
				res += separator + expressions.nodes[idx]->string(file, expressions);
				separator = ", ";
			}
			res += " }";
			return res;
		}
	};

	struct Declaration : Statement {
		Token identifier;
		size_t type_expression_idx = 0;
		size_t value_expression_idx = 0;

		virtual  std::string  string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res;

			res += string_from_view(file, identifier.lexeme);
			res += " :";
			if (type_expression_idx) {
				res += " ";
				res += expressions.nodes[type_expression_idx]->string(file, expressions);
				res += " ";
			}
			if (value_expression_idx) {
				res += "= ";
				res += expressions.nodes[value_expression_idx]->string(file, expressions);
			}
			return res;
		}
	};

	struct Function_Definition : Statement {
		size_t parameter_list_idx = 0;
		size_t return_list_idx = 0;
		size_t statement_list_idx = 0;
		bool is_method = false;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res;
			
			if (is_method) res = "method";
			else           res = "proc";

			size_t idx = 0;
			if (parameter_list_idx) {
				res += " (";

				idx = parameter_list_idx;

				while(idx) {
					res += expressions.nodes[idx]->string(file, expressions);
					idx = expressions.nodes[idx]->next_statement;
					if (idx) res += ", ";
				}
				res += ")";
			}
			
			if (return_list_idx) {
				res += " -> (";
			}

			idx = return_list_idx;
			while(idx) {
				res += expressions.nodes[idx]->string(file, expressions);
				idx = expressions.nodes[idx]->next_statement;
				if (idx) res += ", ";
			}

			if (return_list_idx) res += ")";
			res += " {\n";

			idx = statement_list_idx;
			while(idx) {
				auto str = expressions.nodes[idx]->string(file, expressions) + ";\n";
				append_tab(1, str);
				res += str;
				idx = expressions.nodes[idx]->next_statement;
			}

			res += "}";

			return res;
		}
	};

	struct Argument : Statement {
		size_t value_idx = 0;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			return expressions.nodes[value_idx]->string(file, expressions);
		}
	};

	// >TODO(Tackwin): Do i really need a class just for this ?
	struct Return_Parameter : Statement {
		size_t type_identifier;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			return expressions.nodes[type_identifier]->string(file, expressions);
		}
	};

	struct If : Statement {
		size_t else_statement_idx = 0;
		size_t if_statement_idx = 0;
		size_t condition_idx = 0;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res = "if ";
			res += expressions.nodes[condition_idx]->string(file, expressions) + " ";
			res += expressions.nodes[if_statement_idx]->string(file, expressions);

			if (else_statement_idx) {
				res += "else ";
				res += expressions.nodes[else_statement_idx]->string(file, expressions);
			}

			return res;
		}
	};

	struct For : Statement {
		size_t init_statement_idx = 0;
		size_t cond_statement_idx = 0;
		size_t next_statement_idx = 0;
		size_t loop_statement_idx = 0;

		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res = "for (";

			res += expressions.nodes[init_statement_idx]->string(file, expressions);
			res += "; ";
			res += expressions.nodes[cond_statement_idx]->string(file, expressions);
			res += "; ";
			res += expressions.nodes[next_statement_idx]->string(file, expressions);
			res += ") {\n";
			
			for (size_t idx = loop_statement_idx; idx; idx = expressions.nodes[idx]->next_statement)
			{
				auto line = expressions.nodes[idx]->string(file, expressions);
				append_tab(1, line);
				res += line + ";\n";
			}
			
			res += "}";

			return res;
		}
	};

	struct While : Statement {
		size_t cond_statement_idx = 0;
		size_t loop_statement_idx = 0;
		
		virtual std::string string(
			std::string_view file, const AST& expressions
		) const noexcept override {
			std::string res = "while ";

			res += expressions.nodes[cond_statement_idx]->string(file, expressions);
			res += " {\n";
			
			for (size_t idx = loop_statement_idx; idx; idx = expressions.nodes[idx]->next_statement)
			{
				auto line = expressions.nodes[idx]->string(file, expressions);
				append_tab(1, line);
				res += line + "\n";
			}
			
			res += "}";

			return res;
		}
	};

	#define LIST_AST_TYPE(X)\
		X(Argument           )\
		X(Group_Expression   )\
		X(Group_Statement    )\
		X(Return_Parameter   )\
		X(Identifier         )\
		X(Declaration        )\
		X(Litteral           )\
		X(Array_Access       )\
		X(Function_Call      )\
		X(Operation_List     )\
		X(Unary_Operation    )\
		X(Return_Call        )\
		X(Type_Identifier    )\
		X(If                 )\
		X(For                )\
		X(While              )\
		X(Struct_Definition  )\
		X(Initializer_List   )\
		X(Function_Definition)


	struct Node {
		sum_type(Node, LIST_AST_TYPE);

		const Statement* operator->() const noexcept {
			switch (kind) {
				#define X(x) case x##_Kind: return &x##_;
				LIST_AST_TYPE(X)
				#undef X
				default: return nullptr;
			}
		}
		Statement* operator->() noexcept {
			switch (kind) {
				#define X(x) case x##_Kind: return &x##_;
				LIST_AST_TYPE(X)
				#undef X
				default: return nullptr;
			}
		}
	};

	std::vector<Node> nodes;
};


extern AST parse(
	const std::vector<Token>& tokens, std::string_view file
) noexcept;
