#include <cassert>
#include <cstdio>
#include "ebnf.h"
#include "tokens.h"

using namespace std;

extern char *yytext;
extern int yyleng;
extern int yylineno;
extern int tokval;
extern char qtext[];
extern int qlen;

extern "C" int yylex();

static int sym;

static void getsym()
{
	sym = yylex();
}

static void error()
{
	fprintf(stderr, "%d: error: syntax error (sym=%d)\n", yylineno, sym);
	exit(1);
}

int closing_sym(int opening);

static void parse_branches(Symbol *lhs);

static void synth_nterm_name(int kind, char *name)
{
	static int id;
	char c;
	switch (kind) {
	case '(': c = 's'; break;
	case '[': c = 'o'; break;
	case '{': c = 'm'; break;
	default: assert(0);
	}
	sprintf(name, "_%c%d", c, id);
	id++;
}

static void parse_branch(Branch &branch, Symbol *lhs, int branch_id)
{
	for (;;) {
		Symbol *newsym;
		switch (sym) {
#if 0
		case '\n':
			getsym();
			if (sym != '\t') error();
			getsym();
			newsym = nullptr;
			break;
#endif
		case '(':
		case '[':
		case '{':
			{
				char synth_name[16];
				int opening = sym;
				synth_nterm_name(sym, synth_name);
				string nterm_name(synth_name);
				newsym = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
				getsym();
				parse_branches(newsym);
				newsym->up = lhs;
				if (sym == closing_sym(opening)) getsym();
				else error();
				newsym->branches_core = make_unique<vector<Branch>>(newsym->branches);
				switch (opening) {
				case '{':
					for (auto &c: newsym->branches)
						c.emplace_back(newsym);
				       	newsym->branches.emplace_back();
					break;
				case '[':
				       	newsym->branches.emplace_back();
					break;
				case '(':
					break;
				default:
					assert(0);
				}
			}
			break;
		case CHAR:
			{
				char synth_name[4];
				sprintf(synth_name, "'%c'", tokval);
				string term_name(synth_name);
				newsym = term_dict[term_name];
				if (!newsym) {
					newsym = term_dict[term_name] = new Symbol(Symbol::TERM, term_name);
				}
				getsym();
			}
			break;
		case QUOTE:
			{
				string nterm_name(qtext, qlen);
				Symbol *nterm = nterm_dict[nterm_name];
				if (!nterm) {
					nterm = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
				}
				newsym = nterm;
				getsym();
			}
			break;
		case IDENT:
			{
				string term_name(yytext);
				newsym = term_dict[term_name];
				if (!newsym) {
					newsym = term_dict[term_name] = new Symbol(Symbol::TERM, term_name);
				}
				getsym();
			}
			break;
		default:
			// S ::= ... | <empty> | ...
			if (branch.size() == 1 && branch[0] == empty)
				branch.clear();
			return;
		}
		branch.emplace_back(newsym);
	}
}

static void parse_branches(Symbol *lhs)
{
	vector<Branch> &branches = lhs->branches;
	for (;;) {
		int branch_id = branches.size();
		branches.emplace_back();
		parse_branch(branches.back(), lhs, branch_id);
		if (sym != '|')
			break;
		getsym();
	}
}

static void parse_rule()
{
	string nterm_name;
	if (sym != QUOTE) error();
	nterm_name = string(qtext, qlen);
	getsym();

	Symbol *nterm = nterm_dict[nterm_name];
	if (nterm) {
		if (nterm->defined) {
			fprintf(stderr, "%d: error: redefinition of <%s>\n",
				yylineno, nterm_name.c_str());
			exit(1);
		} else {
			nterm->defined = true;
		}
	} else {
		nterm = nterm_dict[nterm_name] = new Symbol(Symbol::NTERM, nterm_name);
	}

	if (!top) top = nterm;

	if (sym != IS) error();
	getsym();

	parse_branches(nterm);

	if (sym != '\n') error();
	getsym();
}

void parse()
{
	getsym();
	while (sym) {
		parse_rule();
		while (sym == '\n') {
			getsym(); // skip blank links following declaration
		}
	}
}
