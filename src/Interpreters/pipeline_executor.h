// src/Interpreters/pipeline_executor.h — In-process pipeline DAG executor

#pragma once

#include "Interpreters/context.h"
#include "Pipelines/pipeline_catalog.h"
#include <string>
#include <string_view>

namespace mnemo::interpreters {

class PipelineExecutor {
public:
    static auto run(Context& context, std::string_view pipeline_name,
                    std::string_view trigger_name) -> pipelines::PipelineRunEntry;
};

} // namespace mnemo::interpreters
