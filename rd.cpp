#include <algorithm>
#include <cassert>
#include <cstring>
#include "ebnf.h"

#define PREFIX "_parse_"

using namespace std;

set<Symbol*> first_of_production(Symbol *nterm, const Branch &body);
void print_symbol(bool rich, Symbol *s, FILE *fp);

#include "preamble.inc"

#if 0
static bool check_arity(Instance *inst, bool input)
{
	size_t narg = inst->args ? (input ? inst->args->in.size() : inst->args->out.size()) : 0;
	Symbol *sym = inst->sym;
	if (sym->core)
		sym = sym->core; // special case for guarded symbols
	size_t nparam = input ? sym->params.in.size() : sym->params.out.size();
	if (narg == nparam)
		return true;
	fprintf(stderr, "error: ");
	print_symbol(false, sym, stderr);
	fprintf(stderr, " expects %lu %s arguments, got %lu\n",
		nparam, input ? "input" : "output", narg);
	return false;
}

static bool check_arity_all()
{
	bool ret = true;
	for_each_nterm([&](Symbol *nterm) {
		for (const Branch &branch: nterm->branches) {
			for (Instance *inst: branch) {
				if (inst->sym->kind == Symbol::NTERM && !check_arity(inst, true))
					ret = false;
				if (!check_arity(inst, false))
					ret = false;
			}
		}
	});
	return ret;
}
#endif

static void print_param_list(const vector<Param> &list)
{
	for (const Param &p: list)
		printf("%s %s, ", p.type.c_str(), p.name.c_str());
}

static void print_params(Symbol *nterm)
{
	print_param_list(nterm->params.in);
}

static void print_arg_list(const vector<string> &list)
{
	for (const string &arg: list)
		printf("%s, ", arg.c_str());
}

static void print_args(const ArgSpec &args)
{
	print_arg_list(args.in);
}

static void emit_proc_param_list(Symbol *nterm)
{
	putchar('(');
	print_params(nterm);
	printf("const set &_t, const set &_f)");
}

static void emit_proc_header(Symbol *s)
{
	const char *rettype = s->params.out.empty() ? "void" : s->params.out[0].type.c_str();
	printf("static %s " PREFIX "%s", rettype, s->name.c_str());
	emit_proc_param_list(s);
}

static string term_sv(Symbol *term)
{
	return term->params.out.empty() ? "" : "tokval."+term->params.out[0].name;
}

//static map<const Branch*, vector<Param>> branch_locals;

static void emit_action(Instance *inst)
{
	Symbol *s = inst->sym;
	if (s->action.empty()) {
		if (inst->args) {
			const vector<string> &out_args = inst->args->out;
			size_t n_out = out_args.size();
			if (n_out) {
				if (n_out > 1) {
					fprintf(stderr, "error: more than one output argument passed to @%s", s->name.c_str());
				}
				printf("%s = ", out_args[0].c_str());
			}
		}
		printf("%s(", s->name.c_str());
		if (inst->args) {
			const vector<string> &in_args = inst->args->in;
			size_t n = in_args.size();
			for (size_t i=0; i<n; i++) {
				printf("%s", in_args[i].c_str());
				if (i < n-1)
					printf(", ");
			}
		}
		printf(");\n");
	} else {
		printf("%s\n", s->action.c_str());
	}
}

static void emit_proc(Symbol *nterm, int level);

static void print_set(const set<Symbol*> f)
{
	putchar('{');
	auto it = f.begin();
	while (it != f.end()) {
		Symbol *term = *it;
		printf("%s", term->name.c_str());
		if (next(it) != f.end())
			putchar(',');
		it++;
	}
	putchar('}');
}

static void print_tf(const Branch &branch, Branch::const_iterator it)
{
	set<Symbol*> nt;
	Symbol *s = (*it)->core_sym();
	bool thru;
	if (s->kind == Symbol::NTERM || s->weak) {
		auto it2 = next(it);
		while (it2 != branch.end()) {
			Symbol *s2 = (*it2)->sym;
			nt.insert(s2->first.begin(), s2->first.end());
			if (!s2->nullable)
				break;
			it2++;
		}
		thru = it2 == branch.end();
	} else {
		thru = false;
	}
	printf("set");
	print_set(nt);
	if (thru) {
		printf("|_t, _f");
	} else {
		printf(", _t|_f");
	}
}

static void gen_input_bare(const Branch &branch, Branch::const_iterator it, int level)
{
	Instance *inst = *it;
	Symbol *s = inst->core_sym();
	if (s->up) {
		emit_proc(s, level);
	} else {
		printf(PREFIX "%s", s->name.c_str());
	}
	putchar('(');
	if (inst->args)
		print_args(*inst->args);
	print_tf(branch, it);
	putchar(')');
}

static void gen_input(const Branch &branch, Branch::const_iterator it, int level)
{
	Instance *inst = *it;
	if (inst->attached_action.empty()) {
		gen_input_bare(branch, it, level);
		putchar(';');
	} else {
		for (char c: inst->attached_action) {
			if (c == '$')
				gen_input_bare(branch, it, level);
			else
				putchar(c);
		}
	}
	putchar('\n');
}

static void emit_proc(Symbol *nterm, int level)
{
	bool nocheck = false;
	auto indent = [&]() {
		for (int i=0; i<level; i++)
			putchar('\t');
	};
	auto gen_branch = [&](Symbol *nterm, const Branch &branch, const char *ctl) {
		set<Symbol*> f = first_of_production(nterm, branch);
		if (ctl) {
			indent();
			if (strcmp(ctl, "else")) {
				printf("%s (", ctl);
				auto it = f.begin();
				while (it != f.end()) {
					Symbol *term = *it;
					printf("sym == %s", term->name.c_str());
					if (term->core)
						printf(" && %s(%s)", term->sp.c_str(), term_sv(term->core).c_str());
					if (next(it) != f.end())
						printf(" || ");
					it++;
				}
				printf(") {\n");
			} else {
				printf("else {\n");
			}
			nocheck = true;
			level++;
		}
#if 0
		// declare locals
		if (branch_locals.count(&branch)) {
			for (const Param &p: branch_locals[&branch]) {
				indent();
				printf("%s %s;\n", p.type.c_str(), p.name.c_str());
			}
		}
#endif
		// emit code for each symbol in sequence
		for (auto it = branch.begin(); it != branch.end(); it++) {
			Instance *inst = *it;
			Symbol *s = inst->core_sym();
			indent();
			if (s->kind == Symbol::ACTION) {
				emit_action(inst);
			} else {
				if (s->kind == Symbol::TERM && !s->svtype()) {
					const char *outarg = inst->outarg();
					string &&sv(term_sv(s));
					if (outarg)
						printf("%s %s", s->svtype(), outarg);
					if (nocheck) {
						// don't emit check
						if (outarg) {
							printf("= %s;\n", sv.c_str());
							indent();
						}
						printf("getsym();\n");
					} else {
						// emit check
						if (outarg) {
							printf("{};\n");
							indent();
						}
						printf("if (check(%s, ", s->name.c_str());
						print_tf(branch, it);
						printf(")) {\n");
						level++;
						indent();
						if (outarg) {
							printf("%s = %s;\n", outarg, sv.c_str());
							indent();
						}
						printf("getsym();\n");
						level--;
						indent();
						printf("}\n");
					}
				} else /* NTERM or TERM with svtype */ {
					const char *outarg = inst->outarg();
					if (outarg)
						printf("auto %s = ", outarg);
					gen_input(branch, it, level);
				}
				nocheck = false;
			}
		}
		if (ctl) {
			level--;
			indent();
			printf("}\n");
		}
	};
	if (level) {
		printf("[&]");
		emit_proc_param_list(nterm);
	} else {
		putchar('\n');
		indent();
		emit_proc_header(nterm);
	}
	printf(" {\n");
	level++;
	indent();
	printf("try {\n");
	level++;
	if (nterm->opening_sym() == '{') {
		indent();
		printf("for (;;) {\n");
		level++;
		const vector<Branch> &branches = *nterm->branches_core;
		size_t n = branches.size();
		for (size_t i=0; i<n; i++)
			gen_branch(nterm, branches[i], i == 0 ? "if" : "else if");
		indent();
		printf("else break;\n");
		level--;
		indent();
		printf("}\n");
	} else {
		const vector<Branch> &branches = nterm->branches;
		size_t n = branches.size();
		if (n == 1) {
			gen_branch(nterm, branches[0], nullptr);
		} else {
			for (size_t i=0; i<n; i++)
				gen_branch(nterm, branches[i], i == 0 ? "if" : i < n-1 ? "else if" : "else");
		}
	}
	level--;
	indent();
	printf("} catch (SyntaxError &_e) {\n");
	level++;
	indent();
	printf("if (!_t.get(sym)) throw _e;\n");
	const char *svtype = nterm->svtype();
	if (svtype) {
		indent();
		printf("return %s();\n", svtype);
	}
	level--;
	indent();
	printf("}\n");
	level--;
	indent();
	putchar('}');
	putchar(level ? ' ' : '\n');
}

#if 0
static void f(vector<Param> &argtype, size_t pos, const Branch &branch, Symbol *lhs)
{
	if (pos == branch.size())
		return;
	assert (pos<branch.size());
	Instance *inst = branch[pos];
	Symbol *sym = inst->sym;
	if (sym->core)
		sym = sym->core;
	size_t mark = argtype.size();
	if (sym->kind == Symbol::NTERM && sym->up && lhs != sym /* prevent infinite recursion */ )
		for (const Branch &c: sym->branches)
			f(argtype, 0, c, sym);
	if (inst->args) {
		size_t n = inst->args->out.size();
		for (size_t i=0; i<n; i++) {
			const string &arg_out = inst->args->out[i];
			auto it = find_if(argtype.rbegin(), argtype.rend(), [&](const Param &p){return p.name == arg_out;});
			if (it == argtype.rend()) {
				/* not found */
				const string &type = sym->params.out[i].type;
				Param p(type, arg_out);
				argtype.emplace_back(p);
				branch_locals[&branch].emplace_back(p);
			}
		}
	}
	f(argtype, pos+1, branch, lhs);
	argtype.erase(argtype.begin()+mark, argtype.end());
}

void compute_locals()
{
	for_each_reachable_nterm([&](Symbol *nterm) {
		if (nterm->up)
			return;
		vector<Param> argtype;
		for (const Param &p: nterm->params.in)
			argtype.emplace_back(p.type, p.name);
		for (const Param &p: nterm->params.out)
			argtype.emplace_back(p.type, p.name);
		for (const Branch &branch: nterm->branches)
			f(argtype, 0, branch, nterm);
	});
}
#endif

void compute_first_follow();
void check_grammar();

bool generate_rd()
{
	compute_first_follow();
	check_grammar();
#if 0
	if (!check_arity_all())
		return false;
#endif
	// emit preamble
	fputs(preamble1, stdout);
	{
		int n_named_terms = 0;
		for_each_term([&](Symbol *term) {
			if (term->name[0] != '\'')
			n_named_terms++;
		});
		printf("typedef bitset<%d> set;\n", 256+n_named_terms);
	}
	putchar('\n');
	fputs(preamble2, stdout);
	putchar('\n');
	// forward declarations of parsing routines
	for_each_term([](Symbol *term) {
		if (term->svtype()) {
			emit_proc_header(term);
			printf(";\n");
		}
	});
	for_each_reachable_nterm([](Symbol *nterm) {
		if (!nterm->up) {
			emit_proc_header(nterm);
			printf(";\n");
		}
	});
	// determine local variables needed in each proc
	//compute_locals();
#if 0
	// WTF is this??
	for_each_term([](Symbol *term) {
		if (term->kind == Symbol::ACTION && term->action.empty()) {
			size_t n = term->params.in.size();
			for (size_t i=0; i<n; i++) {
				const Param &p = term->params.in[i];
				printf("%s &%s", p.type.c_str(), p.name.c_str());
				if (i<n-1) printf(", ");
			}
		}
	});
#endif
	putchar('\n');
#if 1
	const char *rettype = top->svtype();
	if (!rettype)
		rettype = "void";
	printf("%1$s parse()\n"
	       "{\n"
	       "\tgetsym();\n"
	       "\treturn " PREFIX "%2$s(set{0}, set{});\n"
	       "}\n",
	       rettype, top->name.c_str());
#endif
	for_each_term([](Symbol *term) {
		const char *svtype = term->svtype();
		if (svtype) {
			emit_proc_header(term);
			printf(" {\n"
			       "\t%s sv{};\n"
			       "\tif (check(%s, _t, _f)) {\n"
			       "\t\tsv = %s;\n"
			       "\t\tgetsym();\n"
			       "\t}\n"
			       "\treturn sv;\n"
			       "}\n",
			       svtype, term->name.c_str(), term_sv(term).c_str());
		}
	});
	for_each_reachable_nterm([](Symbol *nterm) {
		if (!nterm->up)
			emit_proc(nterm, 0);
	});
	return true;
}
