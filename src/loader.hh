#pragma once
#include "syntax.hh"
#include <set>
#include <unordered_map>

extern FILE *output;
extern Module *main_module;
extern std::map<DefineStmt*, std::vector<Expr*>> used_as_call;
extern std::map<DefineStmt*, std::vector<Expr*>> used_as_collapse;
extern std::map<DefineStmt*, std::vector<Expr*>> used_as_embed;

enum ModuleStatus {
    UNPROCESSED = 0,
    BAD,
    GOOD
};

struct Module {
    ModuleStatus status;
    LocationFile locfile;
    std::string filename;
    Stmt *toplevel;
    std::map<std::string, DefineStmt*> defined;
    std::vector<Module*> unqualified_import;
    std::unordered_map<std::string, Module*> qualified_import;
    std::unordered_map<std::string, ActionStmt*> defined_action;
    std::unordered_map<std::string, PreprocessDefineStmt*> macro;
};

Stmt *resolve(Module &mod, const std::string qualified, const std::string &ident);
long load(const std::string &filename);
Module *load_module(long &n_errors, const std::string &filename);
void unload_all();

