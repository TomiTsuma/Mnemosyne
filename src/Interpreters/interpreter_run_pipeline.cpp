// src/Interpreters/interpreter_run_pipeline.cpp

#include "Interpreters/interpreter_run_pipeline.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Interpreters/pipeline_executor.h"
#include "Pipelines/pipeline_manager.h"
#include "Columns/column_string.h"

namespace mnemo::interpreters {

auto InterpreterRunPipeline::execute_run(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    const auto& name = query.pipeline_control.pipeline_name;
    const auto run = PipelineExecutor::run(context, name, "manual");

    auto run_id_col = std::make_shared<columns::ColumnString>();
    auto status_col = std::make_shared<columns::ColumnString>();
    auto duration_col = std::make_shared<columns::ColumnString>();
    run_id_col->insert(core::Field(run.run_id));
    status_col->insert(core::Field(pipelines::run_status_name(run.status)));
    duration_col->insert(core::Field(std::to_string(run.duration_ms)));

    core::Block block;
    block.add_column("run_id", run_id_col);
    block.add_column("status", status_col);
    block.add_column("duration_ms", duration_col);
    return block;
}

auto InterpreterRunPipeline::execute_pause(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    pipelines::PipelineManager::instance().pause_pipeline(
        query.pipeline_control.pipeline_name);
    return ddl_utils::make_ok_block();
}

auto InterpreterRunPipeline::execute_resume(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    pipelines::PipelineManager::instance().resume_pipeline(
        query.pipeline_control.pipeline_name);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
