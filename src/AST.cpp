

#include "AST.hpp"

#include "xstd.hpp"

#include <functional>

using Tokens = const std::vector<Token>&;
using F = std::function<size_t()>;

struct Parser_State {
	AST& exprs;
	Tokens tokens;

	size_t current_scope = 0;
	size_t current_depth = 0;
	size_t i             = 0;

	Parser_State(AST& exprs, Tokens tokens) noexcept : exprs(exprs), tokens(tokens) {}

	bool next_type_is(Token::Type t) noexcept {
		return i + 1 < tokens.size() && tokens[i + 1].type == t;
	};
	bool type_is(Token::Type t) noexcept {
		return i < tokens.size() && tokens[i].type == t;
	};
	bool type_is_any(std::initializer_list<Token::Type> t) noexcept {
		for (auto& x : t) if (type_is(x)) return true;
		return false;
	};

	#define ast_return {\
	auto x_idx = exprs.nodes.size();\
	exprs.nodes.emplace_back(x);\
	return x_idx;\
	}
	#define ast_return_(x) {\
	auto x##_idx = exprs.nodes.size();\
	exprs.nodes.emplace_back(x);\
	return x##_idx;\
	}

	size_t return_call() noexcept {
		AST::Return_Call x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (!type_is(Token::Type::Return)) return 0;
		i++;

		x.return_value_idx = expression();
		if (!x.return_value_idx) return 0;

		ast_return;
	}
	
	size_t argument_list() noexcept {
		AST::Argument x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		x.value_idx = expression();
		if (!x.value_idx) return 0;

		ast_return;
	};

	auto cast_to_op(Token::Type x) noexcept {
		switch(x) {
			case Token::Type::Minus: return AST::Operator::Minus;
			case Token::Type::Plus:  return AST::Operator::Plus;
			case Token::Type::Not:   return AST::Operator::Not;
			case Token::Type::Amp:   return AST::Operator::Amp;
			case Token::Type::Star:  return AST::Operator::Star;
			case Token::Type::Div:   return AST::Operator::Div;
			case Token::Type::Inc:   return AST::Operator::Inc;
			case Token::Type::Geq:   return AST::Operator::Geq;
			case Token::Type::Gt:    return AST::Operator::Gt;
			case Token::Type::Leq:   return AST::Operator::Leq;
			case Token::Type::Lt:    return AST::Operator::Lt;
			case Token::Type::Eq:    return AST::Operator::Eq;
			case Token::Type::Neq:   return AST::Operator::Neq;
			case Token::Type::Mod:   return AST::Operator::Mod;
			case Token::Type::Equal: return AST::Operator::Assign;
			case Token::Type::And:   return AST::Operator::And;
			case Token::Type::Or:    return AST::Operator::Or;
			case Token::Type::Dot:   return AST::Operator::Dot;
			default: return AST::Operator::Minus;
		}
	}
	auto is_prefix_unary_op(Token::Type x) noexcept {
		switch(x) {
			case Token::Type::Minus: return true;
			case Token::Type::Plus:  return true;
			case Token::Type::Not:   return true;
			case Token::Type::Star:  return true;
			case Token::Type::Amp:   return true;
			default: return false;
		}
	}
	auto is_postfix_unary_op(Token::Type x) noexcept {
		switch(x) {
			case Token::Type::Inc:   return true;
			default: return false;
		}
	}

	auto is_binary_op(Token::Type x) noexcept {
		switch (x) {
			case Token::Type::Minus: return true;
			case Token::Type::Plus:  return true;
			case Token::Type::Geq:   return true;
			case Token::Type::Div:   return true;
			case Token::Type::Gt:    return true;
			case Token::Type::Mod:   return true;
			case Token::Type::Leq:   return true;
			case Token::Type::Lt:    return true;
			case Token::Type::Eq:    return true;
			case Token::Type::Neq:   return true;
			case Token::Type::And:   return true;
			case Token::Type::Equal: return true;
			case Token::Type::Or:    return true;
			case Token::Type::Dot:   return true;
			default: return false;
		}
	}

	size_t argument_lists() noexcept {
		if (!type_is(Token::Type::Open_Paran)) return 0;
		i++;
		size_t start_idx = 0;
		size_t idx = 0;
		if (!type_is(Token::Type::Close_Paran)) {
			start_idx = idx = argument_list();
			if (!idx) return 0;
		}
		while (type_is(Token::Type::Comma) && i++){
			idx = exprs.nodes[idx]->next_statement = argument_list();
			if (!idx) return 0;
		}
		if (!type_is(Token::Type::Close_Paran)) return 0;
		i++;
		return start_idx;
	};

	size_t return_list() noexcept {
		AST::Return_Parameter x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		x.type_identifier = type_identifier();
		if (!x.type_identifier) return 0;

		ast_return;
	};

	size_t parameter_list() noexcept {
		AST::Parameter x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		x.type_identifier = type_identifier();
		if (!x.type_identifier) return 0;

		if (!type_is(Token::Type::Identifier)) return 0;
		x.name = tokens[i++];

		ast_return;
	};

	size_t function_definition() noexcept {
		AST::Function_Definition x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (!type_is(Token::Type::Proc)) return 0;
		i++;

		size_t idx = 0;
		if (type_is(Token::Type::Open_Paran)) {
			i++;


			if (!type_is(Token::Type::Close_Paran))
				x.parameter_list_idx = idx = parameter_list();
			while (type_is(Token::Type::Comma) && i++)
				idx = exprs.nodes[idx]->next_statement = parameter_list();

			if (!type_is(Token::Type::Close_Paran)) return 0;
			i++;
		}

		if (type_is(Token::Type::Arrow)) {
			i++;

			if (type_is(Token::Type::Open_Paran)) {
				i++;

				if (!type_is(Token::Type::Close_Paran))
					x.return_list_idx = idx = return_list();
				while (type_is(Token::Type::Comma) && i++)
					idx = exprs.nodes[idx]->next_statement = return_list();
				if (!type_is(Token::Type::Close_Paran)) return 0;
				i++;
			} else if (type_is(Token::Type::Identifier)) {
				x.return_list_idx = return_list();
			}
		}

		if (!type_is(Token::Type::Open_Brace)) return 0;
		i++;

		auto scope = ++current_scope;
		if (!type_is(Token::Type::Close_Brace))
			x.statement_list_idx = idx = statement();
		while (idx && !type_is(Token::Type::Close_Brace) && current_scope == scope)
			idx = exprs.nodes[idx]->next_statement = statement();

		if (!idx) return 0;
			
		i++;
		--current_scope;

		ast_return;
	};

	size_t if_condition() noexcept {
		AST::If x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (!type_is(Token::Type::If)) return 0;
		i++;

		x.condition_idx = expression();

		if (!type_is(Token::Type::Open_Brace)) return 0;
		i++;

		auto my_scope = ++current_scope;
		size_t idx = 0;

		if (!type_is(Token::Type::Close_Brace)) {
			x.if_statement_idx = idx = statement();
			if (!idx) return 0;
		}
		while (!type_is(Token::Type::Close_Brace) && current_scope == my_scope) {
			idx = exprs.nodes[idx]->next_statement = statement();
			if (!idx) return 0;
		}
		i++;
		
		--current_scope;

		if (type_is(Token::Type::Else)) {
			i++;

			if (!type_is(Token::Type::Open_Brace)) return 0;
			i++;

			my_scope = ++current_scope;

			if (!type_is(Token::Type::Close_Brace)) {
				x.else_statement_idx = idx = statement();
				if (!idx) return 0;
			}
			while (!type_is(Token::Type::Close_Brace) && current_scope == my_scope) {
				idx = exprs.nodes[idx]->next_statement = statement();
				if (!idx) return 0;
			}
			i++;

			--current_scope;
		}

		ast_return;
	}

	size_t for_loop() noexcept {

		AST::For x;
		x.scope = current_scope++;
		x.depth = current_depth++;
		defer {
			current_scope--;
			current_depth--;
		};

		if (!type_is(Token::Type::For)) return 0;
		i++;

		if (type_is(Token::Type::Open_Paran)) i++;

		x.init_statement_idx = statement();
		if (!x.init_statement_idx) return 0;


		// >SEE(Tackwin): I need to sit down and write what i want to do with the ;s
		// if (!type_is(Token::Type::Semicolon)) return 0;
		// i++;

		x.cond_statement_idx = statement();
		if (!x.cond_statement_idx) return 0;

		// if (!type_is(Token::Type::Semicolon)) return 0;
		// i++;

		x.next_statement_idx = statement();
		if (!x.next_statement_idx) return 0;

		if (type_is(Token::Type::Close_Paran)) i++;
		if (!type_is(Token::Type::Open_Brace)) return 0;
		i++;

		size_t idx = 0;
		if (!type_is(Token::Type::Close_Brace)) {
			x.loop_statement_idx = idx = statement();
			if (!idx) return 0;
		}
		while (!type_is(Token::Type::Close_Brace) && current_scope == x.scope + 1){
			idx = exprs.nodes[idx]->next_statement = statement();
			if (!idx) return 0;
		}
		i++;

		ast_return;
	}

	size_t while_loop() noexcept {
		AST::While x;
		x.scope = current_scope++;
		x.depth = current_depth++;
		defer {
			current_scope--;
			current_depth--;
		};

		if (!type_is(Token::Type::While)) return 0;
		i++;

		x.cond_statement_idx = expression();
		if (!x.cond_statement_idx) return 0;

		if (!type_is(Token::Type::Open_Brace)) return 0;
		i++;

		size_t idx = 0;
		if (!type_is(Token::Type::Close_Brace)) {
			x.loop_statement_idx = idx = statement();
			if (!idx) return 0;

		}
		while (!type_is(Token::Type::Close_Brace) && current_scope == x.scope + 1) {
			idx = exprs.nodes[idx]->next_statement = statement();
			if (!idx) return 0;
		}
		i++;

		ast_return;
	}

	size_t string_litteral() noexcept {
		AST::Litteral x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (!type_is(Token::Type::String)) return 0;
		x.token = tokens[i++];

		ast_return;
	};

	size_t number_litteral() noexcept {
		AST::Litteral x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (!type_is(Token::Type::Number)) return 0;
		x.token = tokens[i++];

		ast_return;
	};

	size_t litteral() noexcept {

		if (type_is(Token::Type::Proc))       return function_definition();
		if (type_is(Token::Type::Struct))     return struct_definition();
		if (type_is(Token::Type::String))     return string_litteral();
		if (type_is(Token::Type::Number))     return number_litteral();

		return 0;
	};

	size_t struct_definition() noexcept {
		AST::Struct_Definition x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (!type_is(Token::Type::Struct)) return 0;
		i++;

		if (!type_is(Token::Type::Open_Brace)) return 0;
		i++;

		size_t idx = 0;
		while (!type_is(Token::Type::Close_Brace)) {
			if (!idx) x.struct_line_idx = idx = assignement();
			else      idx = exprs.nodes[idx]->next_statement = assignement();
			if (!idx) break;
			
			if (!type_is(Token::Type::Semicolon)) return 0;
			i++;
		}

		if (!type_is(Token::Type::Close_Brace)) return 0;
		i++;

		ast_return;
	}

	size_t assignement() noexcept {
		AST::Assignement x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		x.identifier = tokens[i++];
		if (!type_is(Token::Type::Colon)) return 0;
		i++;
		if (type_is(Token::Type::Identifier)) {
			x.type_identifier = type_identifier();
			if (!x.type_identifier) return 0;
		}
		// If there is no identifier AND no equal (meaning value) then we just have something like
		// x:; which we don't allow.
		else if (!type_is(Token::Type::Equal)) return 0;

		if (type_is(Token::Type::Equal)) {
			i++;
			auto literral_idx = expression();
			if (!literral_idx) return 0;
			x.value_idx = literral_idx;
		}


		ast_return;
	};
	#define TT Token::Type

	size_t expression_helper(
		const std::vector<TT>& ops,
		size_t it
	) noexcept {
		AST::Operation_List x;
		x.scope = current_scope;
		x.depth = current_depth;

		if (it + 1 == ops.size()) x.left_idx = factor();
		else                      x.left_idx = expression_helper(ops, it + 1);


		if (!type_is(ops[it])) return x.left_idx;
		else                       x.op = cast_to_op(tokens[i].type);
		
		size_t idx = 0;
		while ( type_is(ops[it]) && i++) {
			size_t t;
			if (it + 1 == ops.size())  t = factor();
			else                       t = expression_helper(ops, it + 1);

			if (!idx) idx = x.rest_idx = t;
			else      idx = exprs.nodes[idx]->next_statement = t;
			if (!idx) return 0;
		}

		ast_return;
	}

	size_t expression() noexcept {
		std::vector<TT> least_to_most_precedent = {
			TT::Equal,
			TT::Or, TT::And,
			TT::Eq, TT::Neq,
			TT::Gt, TT::Geq, TT::Lt, TT::Leq,
			TT::Mod, TT::Plus, TT::Minus, TT::Star, TT::Div,
			TT::Dot
		};

		return expression_helper(least_to_most_precedent, 0);
	}

	size_t unary_operation() noexcept {
		AST::Unary_Operation x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (!is_prefix_unary_op(tokens[i].type)) return 0;
		x.op = cast_to_op(tokens[i++].type);

		x.right_idx = factor();
		if (!x.right_idx) return 0;

		ast_return;
	}

	size_t initializer_list() noexcept {
		AST::Initializer_List x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (type_is(Token::Type::Identifier)) {
			x.type_identifier = type_identifier();
			if (!x.type_identifier) return 0;
		}

		if (!type_is(Token::Type::Open_Brace)) return 0;
		i++;


		size_t idx = 0;
		while (!type_is(Token::Type::Close_Brace)) {
			if (!idx) idx = x.expression_list_idx = expression();
			else      idx = exprs.nodes[idx]->next_statement = expression();
			if (!idx) return 0;

			if (!type_is(Token::Type::Comma)) break;
			i++;
		}

		if (!type_is(Token::Type::Close_Brace)) return 0;
		i++;

		ast_return;
	}

	size_t prefix_operator() noexcept {
		if (is_prefix_unary_op(tokens[i].type)) return unary_operation();
		return atom();
	}

	size_t postfix_operator() noexcept {
		AST::Unary_Operation x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		x.right_idx = prefix_operator();
		if (!is_postfix_unary_op(tokens[i].type)) {
			if (type_is(Token::Type::Open_Paran)) {
				AST::Function_Call y;
				y.scope = x.scope;
				y.depth = x.depth;
				y.identifier_idx = x.right_idx;
				y.argument_list_idx = argument_lists();
				
				ast_return_(y);
			}
			if (type_is(Token::Type::Open_Brack)) {
				i++;
				AST::Array_Access y;
				y.scope = x.scope;
				y.depth = x.depth;
				y.identifier_array_idx = x.right_idx;
				y.identifier_acess_idx = expression();

				if (!type_is(Token::Type::Close_Brack)) return 0;
				i++;

				ast_return_(y);
			}
			return x.right_idx;
		}

		x.op = cast_to_op(tokens[i++].type);

		ast_return;
	}

	size_t atom() noexcept {
		if (type_is(Token::Type::Open_Paran)) {
			i++;

			AST::Expression x;
			x.scope = current_scope;
			x.depth = current_depth++;
			defer { current_depth--; };
			x.inner_idx = expression();
			if (!x.inner_idx) return 0;
			if (!type_is(Token::Type::Close_Paran)) return 0;
			i++;

			ast_return;
		}
		if (type_is(Token::Type::Identifier)) return identifier();
		return litteral();
	}

	size_t factor() noexcept {
		if (type_is(Token::Type::Identifier) && next_type_is(Token::Type::Open_Brace))
			return initializer_list();

		return postfix_operator();
	}
	#undef TT

	size_t type_identifier() noexcept {
		size_t prev_depth = current_depth;
		AST::Type_Identifier x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth = prev_depth; };

		// always start with an identifier.
		if (!type_is(Token::Type::Identifier)) return 0;
		x.identifier = tokens[i++];

		// then we can have arbitrary nested combinations of const, [], and *
		// every time we have a *, it means there is an indirection. We push our current
		// Type_Identifier and we construct a new one with it's pointer_to pointing to the index
		// of the just pushed Type_Identifier.

		while (type_is_any({
			Token::Type::Const, Token::Type::Star, Token::Type::Open_Brack
		})) {
			if (type_is(Token::Type::Const)) {
				i++;
				x.is_const = true;
			}

			if (type_is(Token::Type::Star)) {
				i++;

				auto x_idx = exprs.nodes.size();
				exprs.nodes.push_back(x);
				x = AST::Type_Identifier();
				x.scope = current_scope;
				x.depth = current_depth++;

				x.pointer_to = x_idx;
			}

			if (type_is(Token::Type::Open_Brack)) {
				i++;

				auto x_idx = exprs.nodes.size();
				exprs.nodes.push_back(std::move(x));
				x = AST::Type_Identifier();
				x.scope = current_scope;
				x.depth = current_depth++;

				x.array_to = x_idx;
				x.array_size = expression();


				if (!type_is(Token::Type::Close_Brack)) return 0;
				i++;
			}
		}

		ast_return;
	}

	size_t identifier() noexcept {
		AST::Identifier x;
		x.scope = current_scope;
		x.depth = current_depth++;
		defer { current_depth--; };

		if (!type_is(Token::Type::Identifier)) return 0;
		x.token = tokens[i++];

		ast_return;
	}

	size_t statement() noexcept {
		if (type_is(Token::Type::Identifier) && next_type_is(Token::Type::Colon)) {
			auto idx = assignement();
			if (!type_is(Token::Type::Semicolon)) return 0;
			i++;
			return idx;
		} else if (type_is(Token::Type::Return)) {
			auto idx = return_call();
			if (!type_is(Token::Type::Semicolon)) return 0;
			i++;
			return idx;
		} else if (type_is(Token::Type::If)) {
			auto idx = if_condition();
			if (type_is(Token::Type::Semicolon)) i++; // Optional semicolon
			return idx;
		} else if (type_is(Token::Type::For)) {
			auto idx = for_loop();
			if (type_is(Token::Type::Semicolon)) i++; // Optional semicolon
			return idx;
		} else if (type_is(Token::Type::While)) {
			auto idx = while_loop();
			if (type_is(Token::Type::Semicolon)) i++; // Optional semicolon
			return idx;
		} else /* assume expression */ {
			auto idx = expression();
			if (!type_is(Token::Type::Semicolon)) return 0;
			i++;
			return idx;
		}
	};
};


AST parse(const std::vector<Token>& tokens, std::string_view file) noexcept {
	AST exprs;
	Parser_State parser(exprs, tokens);
	exprs.nodes.emplace_back(nullptr);

	while (parser.i < tokens.size()) {
		if (auto idx = parser.statement(); !idx) {
			println("Error at token %zu", parser.i);
			return exprs;
		}
	}

	return exprs;
}