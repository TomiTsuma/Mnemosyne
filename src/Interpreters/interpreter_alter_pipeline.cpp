// src/Interpreters/interpreter_alter_pipeline.cpp

#include "Interpreters/interpreter_alter_pipeline.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Pipelines/pipeline_manager.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterAlterPipeline::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    (void)context;
    std::optional<std::string> owner;
    for (const auto& set : query.alter.pipeline_sets) {
        if (set.property == "OWNER" || set.property == "owner") {
            owner = set.value;
        } else {
            throw common::Exception{
                "Unknown ALTER PIPELINE property: " + set.property,
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
    }
    pipelines::PipelineManager::instance().alter_pipeline(query.alter.table, owner);
    return ddl_utils::make_ok_block();
}

} // namespace mnemo::interpreters
