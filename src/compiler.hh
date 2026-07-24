#pragma once
#include "syntax.hh"
#include "fsa_anno.hh"

void compile(DefineStmt *);
void export_statement(DefineStmt *);
void generate_header(Module *mod);
void generate_body(Module *mod);
