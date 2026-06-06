// src/Interpreters/pipeline_executor.cpp

#include "Interpreters/pipeline_executor.h"
#include "Interpreters/query_executor.h"
#include "Interpreters/interpreter_create_query.h"
#include "Interpreters/interpreter_insert_query.h"
#include "Interpreters/interpreter_drop_query.h"
#include "Interpreters/interpreter_select_query.h"
#include "Analyzer/analyzer.h"
#include "Planner/planner.h"
#include "Parsers/lexer.h"
#include "Parsers/parser_query.h"
#include "Pipelines/pipeline_manager.h"
#include "Common/exceptions.h"
#include <chrono>

namespace mnemo::interpreters {

namespace {

auto execute_sql(Context& context, const std::string& sql) -> void {
    parsers::Lexer lexer{sql};
    parsers::QueryParser parser{std::move(lexer)};
    auto ast = parser.parse();
    if (!ast) {
        throw common::Exception{
            "Pipeline SQL task parse error",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }

    analyzer::Analyzer analyzer{context};
    auto owned = std::shared_ptr<parsers::QueryAST>{std::move(ast)};
    auto result = analyzer.analyze(owned);
    if (analyzer.has_errors(result)) {
        std::string errors;
        for (const auto& e : result.errors) {
            errors += e + "; ";
        }
        throw common::Exception{
            "Pipeline SQL task analysis error: " + errors,
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }

    if (auto* query_ast = dynamic_cast<parsers::QueryAST*>(result.analyzed_ast.get())) {
        switch (query_ast->query_type) {
            case parsers::QueryAST::QueryType::SELECT:
                (void)InterpreterSelectQuery::execute(context, *query_ast);
                return;
            case parsers::QueryAST::QueryType::INSERT:
                (void)InterpreterInsertQuery::execute(context, *query_ast);
                return;
            case parsers::QueryAST::QueryType::CREATE:
                (void)InterpreterCreateQuery::execute(context, *query_ast);
                return;
            case parsers::QueryAST::QueryType::DROP:
                (void)InterpreterDropQuery::execute(context, *query_ast);
                return;
            default:
                break;
        }
    }

    auto query_tree = analyzer.buildQueryTree(result);
    planner::Planner planner{context};
    auto plan = planner.plan(query_tree);
    auto executor = InterpreterFactory::create(plan, context);
    const auto query_result = executor->execute();
    if (!query_result.error.empty()) {
        throw common::Exception{
            "Pipeline SQL task failed: " + query_result.error,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
}

auto execute_builtin(const pipelines::TaskEntry& task) -> void {
    if (task.body == "fail") {
        throw common::Exception{
            "Built-in task forced failure: " + task.name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
}

} // namespace

auto PipelineExecutor::run(Context& context, std::string_view pipeline_name,
                           std::string_view trigger_name) -> pipelines::PipelineRunEntry {
    auto& mgr = pipelines::PipelineManager::instance();
    const auto run_start = std::chrono::steady_clock::now();
    auto& run = mgr.begin_run(pipeline_name, trigger_name);
    const std::string run_id = run.run_id;

    pipelines::RunStatus final_status = pipelines::RunStatus::Succeeded;
    try {
        const auto tasks = mgr.topological_task_order(pipeline_name);
        for (const auto& task : tasks) {
            pipelines::TaskRunRecord record;
            record.task_name = task.name;
            record.stage_name = task.stage_name;
            record.status = pipelines::TaskStatus::Running;
            record.started_at = std::chrono::system_clock::now();
            const auto task_start = std::chrono::steady_clock::now();

            uint32_t attempts = 0;
            const uint32_t max_attempts = task.max_retries + 1;
            bool task_ok = false;
            std::string task_error;
            while (attempts < max_attempts) {
                ++attempts;
                try {
                    switch (task.type) {
                        case pipelines::TaskType::Sql:
                            execute_sql(context, task.body);
                            break;
                        case pipelines::TaskType::BuiltIn:
                            execute_builtin(task);
                            break;
                    }
                    task_ok = true;
                    break;
                } catch (const common::Exception& ex) {
                    task_error = ex.what();
                }
            }

            const auto task_end = std::chrono::steady_clock::now();
            record.ended_at = std::chrono::system_clock::now();
            record.duration_ms = std::chrono::duration<double, std::milli>(task_end - task_start)
                                     .count();
            if (task_ok) {
                record.status = pipelines::TaskStatus::Succeeded;
            } else {
                record.status = pipelines::TaskStatus::Failed;
                record.error = task_error;
                final_status = pipelines::RunStatus::Failed;
                mgr.record_task_run(pipeline_name, run_id, record);
                break;
            }
            mgr.record_task_run(pipeline_name, run_id, record);
        }
    } catch (const common::Exception& ex) {
        final_status = pipelines::RunStatus::Failed;
        pipelines::TaskRunRecord record;
        record.task_name = "(pipeline)";
        record.status = pipelines::TaskStatus::Failed;
        record.error = ex.what();
        record.ended_at = std::chrono::system_clock::now();
        mgr.record_task_run(pipeline_name, run_id, record);
    }

    const auto run_end = std::chrono::steady_clock::now();
    const double duration_ms =
        std::chrono::duration<double, std::milli>(run_end - run_start).count();
    mgr.finish_run(pipeline_name, run_id, final_status, duration_ms);

    const auto* pipeline = mgr.get_pipeline(pipeline_name);
    if (!pipeline || pipeline->runs.empty()) {
        throw common::Exception{
            "Pipeline run missing after execution",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }
    return pipeline->runs.back();
}

} // namespace mnemo::interpreters
