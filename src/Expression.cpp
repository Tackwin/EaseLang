
#include "Expression.hpp"

#include "xstd.hpp"

#include <functional>

using Tokens = const std::vector<Token>&;
using F = std::function<size_t()>;

struct Parser_State {
	Expressions& exprs;
	Tokens tokens;

	size_t current_scope = 0;
	size_t current_depth = 0;
	size_t i             = 0;

	Parser_State(Expressions& exprs, Tokens tokens) noexcept : exprs(exprs), tokens(tokens) {}

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
	current_depth--;\
	auto x_idx = exprs.nodes.size();\
	exprs.nodes.emplace_back(x);\
	return x_idx;\
	}

	size_t return_call() noexcept {
		Expressions::Return_Call x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (!type_is(Token::Type::Return)) return 0;
		i++;

		x.return_value_idx = litteral();
		if (!x.return_value_idx) return 0;

		ast_return;
	}
	
	size_t argument_list() noexcept {
		Expressions::Argument x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (type_is(Token::Type::Identifier)) {
			x.identifier = tokens[i++];
		} else if (type_is(Token::Type::String)) {
			x.value_idx = litteral();
			if (!x.value_idx) return 0;
		} else return 0;

		ast_return;
	};

	auto cast_to_unary_op(Token::Type x) noexcept {
		switch(x) {
			case Token::Type::Minus: return Expressions::Operator::Minus;
			case Token::Type::Plus:  return Expressions::Operator::Plus;
			case Token::Type::Not:   return Expressions::Operator::Not;
			default: return Expressions::Operator::Minus;
		}
	}
	auto is_unary_op(Token::Type x) noexcept {
		switch(x) {
			case Token::Type::Minus: return true;
			case Token::Type::Plus:  return true;
			case Token::Type::Not:   return true;
			default: return false;
		}
	}

	auto cast_to_binary_op(Token::Type x) noexcept {
		switch (x) {
			case Token::Type::Minus: return Expressions::Operator::Minus;
			case Token::Type::Plus:  return Expressions::Operator::Plus;
			case Token::Type::Geq:   return Expressions::Operator::Geq;
			case Token::Type::Gt:    return Expressions::Operator::Gt;
			case Token::Type::Leq:   return Expressions::Operator::Leq;
			case Token::Type::Lt:    return Expressions::Operator::Lt;
			case Token::Type::Eq:    return Expressions::Operator::Eq;
			case Token::Type::Neq:   return Expressions::Operator::Neq;
			case Token::Type::And:   return Expressions::Operator::And;
			case Token::Type::Or:    return Expressions::Operator::Or;
			default: return Expressions::Operator::And;
		}
	}

	auto is_binary_op(Token::Type x) noexcept {
		switch (x) {
			case Token::Type::Minus: return true;
			case Token::Type::Plus:  return true;
			case Token::Type::Geq:   return true;
			case Token::Type::Gt:    return true;
			case Token::Type::Leq:   return true;
			case Token::Type::Lt:    return true;
			case Token::Type::Eq:    return true;
			case Token::Type::Neq:   return true;
			case Token::Type::And:   return true;
			case Token::Type::Or:    return true;
			default: return false;
		}
	}

	size_t function_call() noexcept {
		Expressions::Function_Call x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (!type_is(Token::Type::Identifier)) return 0;
		x.identifier = tokens[i++];

		if (!type_is(Token::Type::Open_Paran)) return 0;
		i++;

		size_t idx = 0;
		if (!type_is(Token::Type::Close_Paran))
			x.argument_list_idx = idx = argument_list();
		while (type_is(Token::Type::Comma) && i++)
			idx = exprs.nodes[idx]->next_statement = argument_list();

		if (!type_is(Token::Type::Close_Paran)) return 0;
		i++;

		ast_return;
	};

	size_t return_list() noexcept {
		Expressions::Return_Parameter x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (!type_is(Token::Type::Identifier)) return 0;
		x.type = tokens[i++];

		ast_return;
	};

	size_t parameter_list() noexcept {
		Expressions::Parameter x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (!type_is(Token::Type::Identifier)) return 0;
		x.type = tokens[i++];
		if (!type_is(Token::Type::Identifier)) return 0;
		x.name = tokens[i++];

		ast_return;
	};

	size_t function_definition() noexcept {
		Expressions::Function_Definition x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (!type_is(Token::Type::Proc)) return 0;
		i++;

		if (!type_is(Token::Type::Open_Paran)) return 0;
		i++;

		size_t idx = 0;
		if (!type_is(Token::Type::Close_Paran))
			x.parameter_list_idx = idx = parameter_list();
		while (type_is(Token::Type::Comma) && i++)
			idx = exprs.nodes[idx]->next_statement = parameter_list();

		if (!type_is(Token::Type::Close_Paran)) return 0;
		i++;

		if (!type_is(Token::Type::Arrow)) return 0;
		i++;

		if (!type_is(Token::Type::Close_Paran))
			x.return_list_idx = idx = return_list();
		while (type_is(Token::Type::Comma) && i++)
			idx = exprs.nodes[idx]->next_statement = return_list();

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
		Expressions::If x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (!type_is(Token::Type::If)) return 0;
		i++;

		x.condition_idx = expression();

		if (!type_is(Token::Type::Open_Brace)) return 0;
		i++;

		auto my_scope = ++current_scope;
		size_t idx = 0;

		if (!type_is(Token::Type::Close_Brace))
			x.if_statement_idx = idx = statement();
		while (!type_is(Token::Type::Close_Brace) && current_scope == my_scope)
			idx = exprs.nodes[idx]->next_statement = statement();
		i++;
		
		--current_scope;

		if (type_is(Token::Type::Else)) {
			i++;

			if (!type_is(Token::Type::Open_Brace)) return 0;
			i++;

			my_scope = ++current_scope;

			if (!type_is(Token::Type::Close_Brace))
				x.else_statement_idx = idx = statement();
			while (!type_is(Token::Type::Close_Brace) && current_scope == my_scope)
				idx = exprs.nodes[idx]->next_statement = statement();
			i++;

			--current_scope;
		}

		ast_return;
	}

	size_t string_litteral() noexcept {
		Expressions::Litteral x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (!type_is(Token::Type::String)) return 0;
		x.token = tokens[i++];

		ast_return;
	};

	size_t number_litteral() noexcept {
		Expressions::Litteral x;
		x.scope = current_scope;
		x.depth = current_depth++;

		if (!type_is(Token::Type::Number)) return 0;
		x.token = tokens[i++];

		ast_return;
	};

	size_t litteral() noexcept {

		if (type_is(Token::Type::Proc))       return function_definition();
		if (type_is(Token::Type::String))     return string_litteral();
		if (type_is(Token::Type::Number))     return number_litteral();

		return 0;
	};



	size_t assignement() noexcept {
		Expressions::Assignement x;
		x.scope = current_scope;
		x.depth = current_depth++;

		x.identifier = tokens[i++];
		if (!type_is(Token::Type::Colon)) return 0;
		i++;
		if (type_is(Token::Type::Identifier)) {
			x.type_identifier = tokens[i++];
		} else if (!type_is(Token::Type::Equal)) return 0;
		i++;

		auto literral_idx = expression();
		if (!literral_idx) return 0;
		x.value_idx = literral_idx;

		ast_return;
	};
	#define TT Token::Type
	size_t expression() noexcept {
		std::vector<std::initializer_list<TT>> least_to_most_precedent = {
			{ TT::Or, TT::And }, { TT::Eq, TT::Neq }, { TT::Gt, TT::Geq, TT::Lt, TT::Geq }
		};

		std::vector<std::function<size_t(void)>> sub_expr;
		for (size_t it = 0; it < least_to_most_precedent.size(); ++it) {

			auto func = [&, it, ops = least_to_most_precedent[it]] () -> size_t {
				Expressions::Operation_List x;
				x.scope = current_scope;
				x.depth = current_depth++;

				std::function<size_t(void)> f =
					(it + 1 == sub_expr.size()) ? [&] { return factor(); } : sub_expr[it+1];

				x.operand_idx = f();


				size_t idx = x.operand_idx;

				if    (!type_is_any(ops)) return idx;
				else                      x.op = cast_to_binary_op(tokens[i].type);
				
				while ( type_is_any(ops) && i++) {
					idx = exprs.nodes[idx]->next_statement = f();
					if (!idx) return 0;
				}

				ast_return;
			};

			sub_expr.push_back(func);
		}

		return sub_expr.front()();
	}

	size_t unary_operation() noexcept {
		Expressions::Unary_Operation x;
		x.depth = current_depth++;
		x.scope = current_scope;

		if (!is_unary_op(tokens[i].type)) return 0;
		x.op = cast_to_unary_op(tokens[i++].type);

		x.right_idx = factor();
		if (!x.right_idx) return 0;

		ast_return;
	}

	size_t factor() noexcept {
		if (type_is(Token::Type::Identifier) && next_type_is(Token::Type::Open_Paran))
			return function_call();

		if (type_is(Token::Type::Open_Paran)) {
			i++;
			auto idx = expression();
			if (!type_is(Token::Type::Close_Paran)) return 0;
			i++;
			return idx;
		}

		if (is_unary_op(tokens[i].type)) return unary_operation();

		if (type_is(Token::Type::Identifier)) return identifier();
		return litteral();
	}
	#undef TT

	size_t identifier() noexcept {
		Expressions::Identifier x;
		x.scope = current_scope;
		x.depth = current_depth++;

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
		} else /* assume expression */ {
			auto idx = expression();
			if (!type_is(Token::Type::Semicolon)) return 0;
			i++;
			return idx;
		}
	};
};


Expressions parse(const std::vector<Token>& tokens, std::string_view file) noexcept {
	Expressions exprs;
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