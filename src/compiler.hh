#pragma once
#include "syntax.hh"
#include "fsa_anno.hh"

void compile(DefineStmt *);
void generate_cxx(Module *mod);
void generate_graphviz(Module *mod);
