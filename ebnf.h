#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct Symbol;

typedef std::vector<Symbol*> Branch;

struct Symbol {
	enum SymbolKind { TERM, NTERM } kind;
	std::string name;
	std::vector<Branch> branches;
	std::unique_ptr<std::vector<Branch>> branches_core;
	std::set<Symbol*> first, follow;
	Symbol *up; // for inline symbols
	Symbol *core; // TERM
	int id = -1;
	bool nullable = false;
	bool defined = false;
	int opening_sym();
	const char *svtype();
	Symbol(SymbolKind kind, const std::string &name);
};

extern std::map<std::string, Symbol*> term_dict, nterm_dict;
extern Symbol *top;
extern Symbol *empty;

template <typename F>
void for_each_term(F f) {
	for (auto &pair: term_dict)
		f(pair.second);
}

template <typename F>
void for_each_nterm(F f) {
	for (auto &pair: nterm_dict)
		f(pair.second);
}

void for_each_reachable_nterm(std::function<void(Symbol*)>);
