#pragma once
#include "syntax.hh"
#include <set>
#include <map>

struct Module {
    bool processed = false;
    LocationFile locfile;
    std::string filename;
    Stmt *toplevel;
    std::map<std::string, DefineStmt*> defined;
    std::vector<Module*> unqualified_import;
    std::map<std::string, Module*> qualified_import;
    std::map<std::string, std::string> named_action;
};

long load(const std::string &filename);
Module *load_module(long &n_errors, const std::string &filename);
void unload_all();

