#pragma once

#include <new>
#include <tuple>
#include <vector>
#include <string>
#include <optional>


#include "xstd.hpp"
#include "Tokenizer.hpp"

struct Expressions {
	struct Statement {
		virtual std::string string(std::string_view, const Expressions& expressions) const noexcept
		{
			return "statement;\n";
		}

		size_t depth          = 0;
		size_t scope          = 0;
		size_t next_statement = 0;
	};

	struct Expression : Statement {
		size_t inner_idx = 0;

		virtual std::string string(std::string_view str, const Expressions& expr) const noexcept {
			return expr.nodes[inner_idx]->string(str, expr);
		}
	};

	struct Assignement : Statement {
		Token identifier;
		std::optional<size_t> type_identifier = std::nullopt;
		size_t value_idx = 0;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			std::string res;
			res += string_from_view(file, identifier.lexeme) + " :";
			if (type_identifier) {
				res += " " + expressions.nodes[*type_identifier]->string(file, expressions) + " ";
			}
			res += "= " + expressions.nodes[value_idx]->string(file, expressions);
			return res;
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
		Not,
		Amp,
		Deref,
		Dot,
		Or
	};
	static const char* op_to_string(Operator x) noexcept {
		switch (x) {
		case Operator::Minus: return "-";
		case Operator::Plus:  return "+";
		case Operator::Geq:   return ">=";
		case Operator::Leq:   return "<=";
		case Operator::Gt:    return ">";
		case Operator::Lt:    return "<";
		case Operator::Eq:    return "==";
		case Operator::Neq:   return "!=";
		case Operator::And:   return "&&";
		case Operator::Or:    return "||";
		case Operator::Not:   return "!";
		case Operator::Dot:   return ".";
		case Operator::Amp:   return "&";
		case Operator::Deref: return "*";
		default: return "??";
		}
	}

	struct Unary_Operation : Statement {
		Operator op;
		size_t right_idx = 0;

		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			std::string res;
			res += op_to_string(op);
			res += expressions.nodes[right_idx]->string(file, expressions);
			return res;
		}
	};

	struct Operation_List : Statement {
		size_t operand_idx = 0;
		Operator op = Operator::Null;

		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			std::string res;

			res = expressions.nodes[operand_idx]->string(file, expressions);

			size_t idx = 0;
			while ((idx = (!idx ? next_statement : expressions.nodes[idx]->next_statement))) {
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
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			return string_from_view(file, token.lexeme);
		}
	};

	struct Type_Identifier : Statement {

		Token identifier;
		bool is_const = false;
		std::optional<size_t> pointer_to = std::nullopt;

		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			std::string res = "";
			
			if (pointer_to) {
				res = expressions.nodes[*pointer_to]->string(file, expressions);
				res += "*";
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
			std::string_view file, const Expressions& expressions
		) const noexcept override { return string_from_view(file, token.lexeme); }
	};

	struct Function_Call : Statement {
		Token identifier;
		size_t argument_list_idx = 0;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			std::string res;
			res += string_from_view(file, identifier.lexeme) + "(";

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

	struct Return_Call : Statement {
		size_t return_value_idx = 0;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			std::string res = "return ";
			res += expressions.nodes[return_value_idx]->string(file, expressions);
			return res;
		}
	};

	struct Struct_Definition : Statement {
		size_t struct_line_idx = 0;

		virtual std::string string(
			std::string_view file, const Expressions& expressions
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
			std::string_view file, const Expressions& expressions
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

	struct Function_Definition : Statement {
		size_t parameter_list_idx = 0;
		size_t return_list_idx = 0;
		size_t statement_list_idx = 0;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			std::string res = "proc";

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
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			return expressions.nodes[value_idx]->string(file, expressions);
		}
	};

	struct Parameter : Statement {
		size_t type_identifier;
		Token name;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			return 
				expressions.nodes[type_identifier]->string(file, expressions) + " " +
				string_from_view(file, name.lexeme);
		}
	};

	// >TODO(Tackwin): Do i really need a class just for this ?
	struct Return_Parameter : Statement {
		size_t type_identifier;

		// Gather all the keyword lol
		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			return expressions.nodes[type_identifier]->string(file, expressions);
		}
	};

	struct If : Statement {
		size_t else_statement_idx = 0;
		size_t if_statement_idx = 0;
		size_t condition_idx = 0;

		virtual std::string string(
			std::string_view file, const Expressions& expressions
		) const noexcept override {
			std::string res = "if ";
			res += expressions.nodes[condition_idx]->string(file, expressions) + " {\n";

			size_t idx = if_statement_idx;
			while (idx) {
				auto str = expressions.nodes[idx]->string(file, expressions) + ";\n";
				append_tab(1, str);
				res += str;
				idx  = expressions.nodes[idx]->next_statement;
			}

			res += "}";

			idx = else_statement_idx;
			if (idx) {
				res += " else {\n";

				while (idx) {
					auto str = expressions.nodes[idx]->string(file, expressions) + ";\n";
					append_tab(1, str);
					res += str;
					idx  = expressions.nodes[idx]->next_statement;
				}

				res += "}";
			}

			return res;
		}
	};

	#define LIST_AST_TYPE\
		X(Parameter          )\
		X(Argument           )\
		X(Expression         )\
		X(Return_Parameter   )\
		X(Assignement        )\
		X(Identifier         )\
		X(Litteral           )\
		X(Function_Call      )\
		X(Operation_List     )\
		X(Unary_Operation    )\
		X(Return_Call        )\
		X(Type_Identifier    )\
		X(If                 )\
		X(Struct_Definition  )\
		X(Initializer_List   )\
		X(Function_Definition)

	struct AST_Node {
		enum Kind {
			None_Kind = 0
			#define X(x) , x##_Kind
			LIST_AST_TYPE
			#undef X
		} kind;
		union {
			#define X(x) x x##_;
			LIST_AST_TYPE
			#undef X
		};

		#define X(x) AST_Node(x y) noexcept { kind = x##_Kind; new (&x##_) x; x##_ = (y); }
		LIST_AST_TYPE
		#undef X

		AST_Node(std::nullptr_t) noexcept { kind = None_Kind; }

		AST_Node(AST_Node&& that) noexcept {
			kind = that.kind;
			switch (kind) {
				#define X(x) case x##_Kind: new(&x##_) x; x##_ = that.x##_; break;
				LIST_AST_TYPE
				#undef X
				default: break;
			}
		}

		const Statement* operator->() const noexcept {
			switch (kind) {
				#define X(x) case x##_Kind: return &x##_;
				LIST_AST_TYPE
				#undef X
				default: return nullptr;
			}
		}
		Statement* operator->() noexcept {
			switch (kind) {
				#define X(x) case x##_Kind: return &x##_;
				LIST_AST_TYPE
				#undef X
				default: return nullptr;
			}
		}
	};

	std::vector<AST_Node> nodes;
};


extern Expressions parse(
	const std::vector<Token>& tokens, std::string_view file
) noexcept;