// src/Interpreters/interpreter_model_control.h — MODEL layer control verbs
// Mnemosyne: A column-oriented analytical DBMS
//
// Handles RUN TRAINING_JOB / RUN TUNING_JOB / DEPLOY / PREDICT / EVALUATE /
// COMPARE / GENERATE. These bypass the planner and are dispatched directly by
// the HTTP handler, mirroring RUN PIPELINE.

#pragma once

#include "Core/block.h"
#include "Interpreters/context.h"
#include "Parsers/ast.h"

namespace mnemo::interpreters {

class InterpreterModelControl {
public:
    static auto execute(Context& context, const parsers::QueryAST& query) -> core::Block;

private:
    static auto run_training_job(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto run_tuning_job(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto deploy(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto predict(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto evaluate(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto compare(Context& context, const parsers::QueryAST& query) -> core::Block;
    static auto generate(Context& context, const parsers::QueryAST& query) -> core::Block;
};

} // namespace mnemo::interpreters
