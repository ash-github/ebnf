#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

struct ArgSpec;
struct Instance;

// TODO unique_ptr<Instance> would be better
typedef std::vector<Instance*> Branch;

struct Param {
	std::string type;
	std::string name;
	Param(const std::string &type, const std::string &name);
};

struct ParamSpec {
	std::vector<Param> out, in;
	bool empty() const;
};

struct Symbol {
	enum SymbolKind { TERM, NTERM, ACTION } kind;
	std::string name;
	std::vector<Branch> branches;
	std::unique_ptr<std::vector<Branch>> branches_core;
	std::set<Symbol*> first, follow;
	ParamSpec params;
	Symbol *up; // for inline symbols
	std::string sp; // TERM: semantic predicate
	Symbol *core; // TERM
	std::string action; // ACTION (inline)
	int id = -1;
	bool nullable = false;
	bool defined = false;
	bool weak = false;
	int opening_sym();
	const char *svtype();
	Symbol(SymbolKind kind, const std::string &name);
};

struct ArgSpec {
	std::vector<std::string> out, in;
	bool empty() const;
};

struct Instance {
	Symbol *sym;
	std::unique_ptr<ArgSpec> args;
	std::string attached_action;
	Instance(Symbol *sym, ArgSpec &&args);
	Instance(Symbol *sym);
	const char *outarg();
	Symbol *core_sym();
};

extern std::map<std::string, Symbol*> term_dict, nterm_dict, action_dict;
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

template <typename F>
void for_each_action(F f) {
	for (auto &pair: action_dict)
		f(pair.second);
}

void for_each_reachable_nterm(std::function<void(Symbol*)>);
