#include <stdio.h>

#include "File.hpp"
#include "Tokenizer.hpp"
#include "AST.hpp"
#include "Interpreter.hpp"

int main(int argc, char** argv) noexcept {
	auto t1 = seconds();
	defer {
		auto t2 = seconds();
		println("%f seconds elapsed.", t2 - t1);
	};

	if (argc < 2) {
		printf("Please input a file to compîle.\n");
		return 0;
	}

	auto path = argv[1];

	printf("Reading at %s\n", path);

	auto file = read_whole_text(path);

	printf("%s\n", file.c_str());

	auto tokens = tokenize(file);

	size_t i = 0;
	for (auto& x : tokens) {
		printf("%zu, %s ", i++, token_type_to_string(x.type).c_str());
		printf("[%zu; %zu] %.*s\n", x.line, x.col, (int)x.lexeme.size, &file[x.lexeme.i]);
	}

	auto exprs = parse(tokens, file);
	printf("Parsed\n");

	for (auto& x : exprs.nodes) if (x.kind && x->depth == 0)
		printf("%s;\n", x->string(file, exprs).c_str());

	AST_Interpreter ast_interpreter;
	ast_interpreter.push_builtin();
	ast_interpreter.variables.reserve(100000);

	for (size_t i = 1; i < exprs.nodes.size(); ++i) if (exprs.nodes[i]->depth == 0)
		ast_interpreter.print_value(ast_interpreter.interpret(exprs.nodes, i, file));

	return 0;
}
