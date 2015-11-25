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

// (a, b, c, ...)
static vector<string> parse_arg_list(void)
{
	vector<string> list;
	if (sym == IDENT) {
		list.emplace_back(yytext);
		getsym();
		goto finish;
	}
	if (sym == QUOTE) {
		list.emplace_back(qtext);
		getsym();
		goto finish;
	}
	if (sym == '(') getsym();
	else error();
	if (sym == ')') {
		getsym();
		goto finish;
	}
	for (;;) {
		if (sym == IDENT) {
			list.emplace_back(yytext);
			getsym();
		} else if (sym == QUOTE) {
			list.emplace_back(qtext);
			getsym();
		} else {
			error();
		}
		if (sym == ')') {
			getsym();
			goto finish;
		}
		if (sym == ',') getsym();
		else error();
	}
finish:
	return list;
}

static Param parse_param()
{
	if (sym != QUOTE) error();
	string type(qtext, qlen);
	getsym();
	string name;
	if (sym == IDENT) {
		name = yytext;
		getsym();
	}
	return Param(type, name);
}

static vector<Param> parse_param_list()
{
	vector<Param> list;
	if (sym == QUOTE) {
		list.emplace_back(parse_param());
		goto finish;
	}
	if (sym == '(') getsym();
	else error();
	if (sym == ')') {
		getsym();
		goto finish;
	}
	for (;;) {
		list.emplace_back(parse_param());
		if (sym == ')') {
			getsym();
			goto finish;
		}
		if (sym == ',') getsym();
		else error();
	}
finish:
	return list;
}

static ParamSpec parse_params() {
	ParamSpec params;
	if (sym == IN) {
		getsym();
		params.in = parse_param_list();
	}
	if (sym == OUT) {
		getsym();
		params.out = parse_param_list();
	}
	return params;
}

static ArgSpec parse_args() {
	ArgSpec args;
	if (sym == IN) {
		getsym();
		args.in = parse_arg_list();
	}
	if (sym == OUT) {
		getsym();
		args.out = parse_arg_list();
	}
	return args;
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
						c.emplace_back(new Instance(newsym));
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
				if (lhs && sym == '?') {
					getsym();
					// expect IDENT: name of semantic predicate function
					if (sym == IDENT) {
						string guarded_term_name(term_name + "::" + yytext);
						Symbol *guarded_term = term_dict[guarded_term_name];
						if (!guarded_term) {
							guarded_term = term_dict[guarded_term_name] =
								new Symbol(Symbol::TERM, term_name);
						}
						guarded_term->sp = yytext;
						guarded_term->core = newsym;
						newsym = guarded_term;
						getsym();
					} else {
						error();
					}
				}
			}
			break;
		case NAMED_ACTION:
			{
				string name(yytext+1); // +1 for leading '@'
				getsym();
				newsym = action_dict[name];
				if (!newsym)
					newsym = action_dict[name] = new Symbol(Symbol::ACTION, name);
				newsym->nullable = true;
			}
			break;
		case INLINE_ACTION:
			{
				newsym = new Symbol(Symbol::ACTION, "");
				newsym->action = qtext;
				getsym();
				newsym->nullable = true;
			}
			break;
		default:
			// S ::= ... | <empty> | ...
			if (branch.size() == 1 && branch[0]->sym == empty)
				branch.clear();
			return;
		}
		Instance *newinst;
		string atact;
		if (sym == ATTACHED_ACTION) {
			atact = qtext;
			getsym();
		}
		ArgSpec args = parse_args();
		if (!args.empty())
			newinst = new Instance(newsym, std::move(args));
		else
			newinst = new Instance(newsym);
		newinst->attached_action = atact;
		branch.emplace_back(newinst);
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

#ifdef ENABLE_WEAK
	// TODO fix broken code
	if (nterm_name == "WEAK") {
		Branch weak_symbols;
		parse_branch(weak_symbols, nullptr, 0);
		for (Symbol *s: weak_symbols)
			s->weak = true;
	} else {
#endif
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
#ifdef ENABLE_WEAK
	}
#endif

	nterm->params = parse_params();

	if (sym != IS) error();
	getsym();

	parse_branches(nterm);

	if (sym != '\n') error();
	getsym();
}

static void parse_decl()
{
	Symbol *s;
	if (sym == IDENT) {
		string name(yytext);
		getsym();
		s = term_dict[name];
		if (!s)
			s = term_dict[name] = new Symbol(Symbol::TERM, name);
	} else if (sym == NAMED_ACTION) {
		string name(yytext+1);
		getsym();
		s = action_dict[name];
		if (!s)
			s = action_dict[name] = new Symbol(Symbol::ACTION, name);
	} else {
		error();
	}
	s->params = parse_params();
	if (sym != '\n') error();
	getsym();
}

void parse()
{
	getsym();
	while (sym) {
		if (sym == QUOTE) parse_rule();
		else parse_decl();
		while (sym == '\n')
			getsym(); // skip blank links following declaration
	}
}
