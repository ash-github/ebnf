#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <getopt.h>
#include "ebnf.h"

using namespace std;

// symbol information

struct Symbol;

map<string, Symbol*> term_dict, nterm_dict;
Symbol *top;
Symbol *empty;

int Symbol::opening_sym()
{
	if (name[0] != '_')
		return 0;
	switch (name[1]) {
	case 's': return '(';
	case 'o': return '[';
	case 'm': return '{';
	}
	assert(0);
}
Symbol::Symbol(SymbolKind kind, const string &name):
	kind(kind), name(name) {}

void for_each_reachable_nterm(std::function<void(Symbol*)> f)
{
	struct Visitor {
		set<Symbol*> vis;
		std::function<void(Symbol*)> f;
		void visit(Symbol *nterm) {
			assert(nterm->kind == Symbol::NTERM);
			if (vis.count(nterm)) {
				return;
			}
			vis.insert(nterm);
			f(nterm);
			for (auto &branch: nterm->branches) {
				for (Symbol *s: branch) {
					if (s->kind == Symbol::NTERM) {
						visit(s);
					}
				}
			}
		}
		Visitor(std::function<void(Symbol*)> f): f(f) {}
	} visitor(f);
	visitor.visit(top);
}

void print_symbol(Symbol *s, FILE *fp);

void print_production(Symbol *nterm, const Branch &branch, FILE *fp)
{
	print_symbol(nterm, fp);
	fputs(" ->", fp);
	for (Symbol *s: branch) {
		fputc(' ', fp);
		print_symbol(s, fp);
	}
	fputc('\n', fp);
}

static void define_empty()
{
	string name("empty");
	empty = nterm_dict[name] = new Symbol(Symbol::NTERM, name);
	empty->branches.emplace_back();
	empty->nullable = true;
	empty->defined = true;
}

static void undef_empty()
{
	nterm_dict.erase("empty");
}
	
static void check_undefined()
{
	bool err = false;
	for_each_nterm([&](Symbol *nterm) {
		if (nterm->branches.empty()) {
			fprintf(stderr, "error: <%s> is undefined\n", nterm->name.c_str());
			err = true;
		}
	});
	if (err)
		exit(1);
}

void print_branches(const vector<Branch> &branches, FILE *fp);
void print_branch(const Branch &branch, FILE *fp);

int closing_sym(int opening)
{
	switch (opening) {
	case '(': return ')';
	case '[': return ']';
	case '{': return '}';
	}
	assert(0);
}

void print_symbol(Symbol *s, FILE *fp)
{
	switch (s->kind) {
	case Symbol::TERM:
		fputs(s->name.c_str(), fp);
		break;
	case Symbol::NTERM:
		{
			int opening = s->opening_sym();
			if (opening) {
				fprintf(fp, "%c ", opening);
				print_branches(*s->branches_core, fp);
				fprintf(fp, " %c", closing_sym(opening));
			} else {
				fprintf(fp, "<%s>", s->name.c_str());
			}
		}
		break;
	default:
		assert(0);
	}
}

void print_branch(const Branch &branch, FILE *fp)
{
	bool sep = false;
	for (Symbol *s: branch) {
		if (sep) fputc(' ', fp);
		sep = true;
		print_symbol(s, fp);
	}
}

void print_branches(const vector<Branch> &branches, FILE *fp)
{
	size_t n = branches.size();
	for (size_t i=0; i<n; i++) {
		print_branch(branches[i], fp);
		if (i < n-1)
			fputs(" | ", fp);
	}
}

void print_rules(FILE *fp)
{
	for_each_reachable_nterm([=](Symbol *nterm) {
		if (!nterm->opening_sym()) {
			fprintf(fp, "<%s>", nterm->name.c_str());
			fputs(" ::= ", fp);
			print_branches(nterm->branches, fp);
			fputc('\n', fp);
		}
	});
}

void print_term_set(const set<Symbol*> &s)
{
	putchar('{');
	for (Symbol *term: s)
		printf(" %s", term->name.c_str());
	printf(" }");
}

void print_first_follow()
{
	for_each_nterm([](Symbol *nterm) {
		print_symbol(nterm, stdout);
		putchar('\n');
		printf("FIRST: ");
		print_term_set(nterm->first);
		putchar('\n');
		printf("FOLLOW: ");
		print_term_set(nterm->follow);
		putchar('\n');
	});
}

static void list_symbols()
{
	printf("terminals: %lu\n", term_dict.size());
	for_each_term([](Symbol *term) {
		puts(term->name.c_str());
	});
	printf("nterminals: %lu\n", nterm_dict.size());
	for_each_nterm([](Symbol *nterm) {
		print_symbol(nterm, stdout);
		putchar('\n');
	});
}

#if 0
int term_id = 1; // 0 for EOF
static void number_terms()
{
	for_each_term([&](Symbol *term) {
		term->id = term_id++;
	});
}
#endif

void parse();
bool check_grammar();
void compute_first_follow();

extern FILE *yyin;

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s (-c|-f|-g|-l|-p|-P) <file>\n", progname);
	exit(2);
}

int main(int argc, char **argv)
{
	enum {
		PRINT_RULES,
		LIST_SYMBOLS,
		CHECK_GRAMMAR,
		PRINT_FIRST_FOLLOW,
	} action;
	int opt;
	while ((opt = getopt(argc, argv, "cflp")) != -1) {
		switch (opt) {
		case 'c':
			action = CHECK_GRAMMAR;
			break;
		case 'f':
			action = PRINT_FIRST_FOLLOW;
			break;
		case 'l':
			action = LIST_SYMBOLS;
			break;
		case 'p':
			action = PRINT_RULES;
			break;
		default:
			usage(argv[0]);
		}
	}
	if (optind >= argc)
		usage(argv[0]);
	char *fpath = argv[optind];
	if (!(yyin = fopen(fpath, "r"))) {
		perror(fpath);
		exit(1);
	}
	define_empty();
	parse();
	undef_empty();
	if (!top) {
		fputs("error: grammar is empty\n", stderr);
		exit(1);
	}
	check_undefined();
	int ret = 0;
	switch (action) {
	case PRINT_RULES:
		print_rules(stdout);
		break;
	case LIST_SYMBOLS:
		list_symbols();
		break;
	case CHECK_GRAMMAR:
		compute_first_follow();
		check_grammar();
		break;
	case PRINT_FIRST_FOLLOW:
		compute_first_follow();
		print_first_follow();
		break;
	default:
		assert(0);
	}
	return ret;
}
