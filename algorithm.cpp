#include <algorithm>
#include <cassert>
#include <cstdio>
#include "ebnf.h"

using namespace std;

void print_production(bool rich, Symbol *nterm, const Branch &body, FILE *fp);

static bool insert_all(set<Symbol*> &a, const set<Symbol*> &b)
{
	size_t old_size = a.size();
	a.insert(b.begin(), b.end());
	return a.size() > old_size;
}

void compute_first_follow()
{
	for_each_term([](Symbol *term) {
		term->first.insert(term);
	});
	bool changed;
	do {
		changed = false;
		for_each_nterm([&](Symbol *nterm) {
			for (auto &branch: nterm->branches) {
				if (!nterm->nullable) {
					if (all_of(branch.begin(), branch.end(), [](Instance *inst){return inst->sym->nullable;})) {
						nterm->nullable = true;
						changed = true;
					}
				}
				for (Instance *inst: branch) {
					Symbol *s = inst->sym;
					if (insert_all(nterm->first, s->first))
						changed = true;
					if (!s->nullable)
						break;
				}
				for (auto it1 = branch.begin(); it1 != branch.end(); it1++) {
					Symbol *s1 = (*it1)->sym, *s2;
					if (s1->kind != Symbol::NTERM)
						continue;
					auto it2 = next(it1);
					while (it2 != branch.end()) {
						s2 = (*it2)->sym;
						if (insert_all(s1->follow, s2->first))
							changed = true;
						if (!s2->nullable)
							break;
						it2++;
					}
					if (it2 == branch.end()) {
						if (insert_all(s1->follow, nterm->follow))
							changed = true;
					}
				}
			}
		});
	} while (changed);
}

static void number_nterms()
{
	int id = 0;
	for_each_reachable_nterm([&](Symbol *nterm) {
		nterm->id = id++;
	});
}

// true if no left recursion
bool check_left_recursion()
{
	struct {
		set<Symbol*> vis;
		bool ans = true;
		void visit(Symbol *nterm) {
			assert(nterm->kind == Symbol::NTERM);
			if (vis.count(nterm)) {
				fprintf(stderr, "error: <%s> is left-recursive\n", nterm->name.c_str());
				ans = false;
				return;
			}
			vis.insert(nterm);
			for (auto &branch: nterm->branches) {
				auto it = branch.begin();
				while (it != branch.end()) {
					Symbol *s = (*it)->sym;
					if (s->kind == Symbol::NTERM)
						visit(s);
					if (!s->nullable)
						break;
					it++;
				}
			}
		}
	} visitor;
	visitor.visit(top);
	return visitor.ans;
}

set<Symbol*> first_of_production(Symbol *nterm, const Branch &body)
{
	set<Symbol*> f;
	auto it = body.begin();
	while (it != body.end()) {
		Symbol *s = (*it)->sym;
		f.insert(s->first.begin(), s->first.end());
		if (!s->nullable)
			break;
		it++;
	}
	if (it == body.end())
		f.insert(nterm->follow.begin(), nterm->follow.end());
	return f;
}

bool check_grammar()
{
	// FIRST(X->y) = if nullable(y) then FIRST(y) ∪ FOLLOW(X) else FIRST(y)
	/* for each nonterminal X, for each pair of distinct productions
	   X->y1 and X->y2, FIRST(X->y1) ∩ FIRST(X->y2) = ∅ */
	bool ans = true;
	number_nterms();
	// detect unreachable nonterminals
	for_each_nterm([](Symbol *nterm) {
		if (nterm->id < 0)
			fprintf(stderr, "warning: <%s> is useless\n", nterm->name.c_str());
	});
	if (!check_left_recursion())
		ans = false;
	for_each_nterm([&](Symbol *nterm) {
		map<Symbol*, Branch*> m;
		for (auto &branch: nterm->branches) {
			set<Symbol*> f = first_of_production(nterm, branch);
			for (Symbol *s: f) {
				if (m[s]) {
					fprintf(stderr, "conflict on %s:\n", s->name.c_str());
					print_production(false, nterm, *m[s], stderr);
					print_production(false, nterm, branch, stderr);
					ans = false;
				} else {
					m[s] = &branch;
				}
			}
		}
	});
	return ans;
}
