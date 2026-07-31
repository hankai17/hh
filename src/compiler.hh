#pragma once
#include "syntax.hh"
#include "fsa_anno.hh"

#include <unordered_map>

extern std::unordered_map<DefineStmt*, FsaAnno> compiled;

void print_assoc(const FsaAnno &anno);
void print_fsa(const Fsa &fsa);
void compile(DefineStmt *);
bool compile_export(DefineStmt *stmt);
void generate_cxx(Module *mod);
void generate_graphviz(Module *mod);
