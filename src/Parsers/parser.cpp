// src/Parsers/parser.cpp — SQL parser for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Parsers/parser.h"
#include "Parsers/parser_query.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace mnemo::parsers {

namespace {
auto upper_str(std::string s) -> std::string {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(::toupper(c)); });
    return s;
}
} // namespace

// ── Parser ──

Parser::Parser(Lexer lexer) : lexer_{lexer}, current_{lexer_.next()} {}

auto Parser::parse() -> std::unique_ptr<QueryAST> {
    std::fprintf(stderr, "Parser::parse start\n");
    lexer_.reset();
    current_ = lexer_.next();
    std::fprintf(stderr, "Parser::parse first token='%s'\n", current_.value.c_str());
    return parse_query();
}

auto Parser::parse_query() -> std::unique_ptr<QueryAST> {
    auto ast = std::make_unique<QueryAST>();

    if (current_.type == TokenType::KeywordSelect) {
        ast->query_type = QueryAST::QueryType::SELECT;
        parse_select(ast);
    } else if (current_.type == TokenType::KeywordInsert) {
        ast->query_type = QueryAST::QueryType::INSERT;
        parse_insert(ast);
    } else if (current_.type == TokenType::KeywordCreate) {
        ast->query_type = QueryAST::QueryType::CREATE;
        parse_create(ast);
    } else if (current_.type == TokenType::KeywordDrop ||
               current_.type == TokenType::KeywordTruncate ||
               current_.type == TokenType::KeywordDetach) {
        ast->query_type = QueryAST::QueryType::DROP;
        parse_drop(ast);
    } else if (current_.type == TokenType::KeywordAlter) {
        ast->query_type = QueryAST::QueryType::ALTER;
        parse_alter(ast);
    } else if (current_.type == TokenType::KeywordShow) {
        ast->query_type = QueryAST::QueryType::SHOW;
        parse_show(ast);
    } else if (current_.type == TokenType::KeywordDescribe || current_.type == TokenType::KeywordDesc) {
        ast->query_type = QueryAST::QueryType::DESCRIBE;
        parse_describe(ast);
    } else if (current_.type == TokenType::KeywordExplain) {
        ast->query_type = QueryAST::QueryType::EXPLAIN;
        parse_explain(ast);
    } else if (current_.type == TokenType::KeywordUse) {
        ast->query_type = QueryAST::QueryType::USE;
        parse_use(ast);
    } else if (current_.type == TokenType::KeywordRefresh) {
        ast->query_type = QueryAST::QueryType::REFRESH;
        parse_refresh(ast);
    } else if (current_.type == TokenType::KeywordRegister) {
        ast->query_type = QueryAST::QueryType::REGISTER;
        parse_register(ast);
    } else if (current_.type == TokenType::KeywordDrain) {
        ast->query_type = QueryAST::QueryType::DRAIN;
        parse_drain(ast);
    } else if (current_.type == TokenType::KeywordRemove) {
        ast->query_type = QueryAST::QueryType::REMOVE;
        parse_remove(ast);
    } else if (current_.type == TokenType::KeywordTest) {
        ast->query_type = QueryAST::QueryType::TEST;
        parse_test(ast);
    } else if (current_.type == TokenType::KeywordDiscover) {
        ast->query_type = QueryAST::QueryType::DISCOVER;
        parse_discover(ast);
    } else if (current_.type == TokenType::KeywordRun) {
        ast->query_type = QueryAST::QueryType::RUN;
        parse_run(ast);
    } else if (current_.type == TokenType::KeywordPause) {
        ast->query_type = QueryAST::QueryType::PAUSE;
        parse_pause(ast);
    } else if (current_.type == TokenType::KeywordResume) {
        ast->query_type = QueryAST::QueryType::RESUME;
        parse_resume(ast);
    } else if (current_.type == TokenType::KeywordPublish) {
        ast->query_type = QueryAST::QueryType::PUBLISH;
        parse_publish(ast);
    } else if (current_.type == TokenType::KeywordSubscribe) {
        ast->query_type = QueryAST::QueryType::SUBSCRIBE;
        parse_subscribe(ast);
    } else if (current_.type == TokenType::KeywordDeploy) {
        ast->query_type = QueryAST::QueryType::DEPLOY;
        parse_deploy(ast);
    } else if (current_.type == TokenType::KeywordPredict) {
        ast->query_type = QueryAST::QueryType::PREDICT;
        parse_predict(ast);
    } else if (current_.type == TokenType::KeywordEvaluate) {
        ast->query_type = QueryAST::QueryType::EVALUATE;
        parse_evaluate(ast);
    } else if (current_.type == TokenType::KeywordCompare) {
        ast->query_type = QueryAST::QueryType::COMPARE;
        parse_compare(ast);
    } else if (current_.type == TokenType::KeywordGenerate) {
        ast->query_type = QueryAST::QueryType::GENERATE;
        parse_generate(ast);
    } else {
        std::string msg = "Parser: unexpected token '" + current_.value + "'";
        int code = static_cast<int>(common::ErrorCode::SYNTAX_ERROR);
        throw common::Exception{std::move(msg), code};
    }

    return ast;
}

void Parser::parse_select(std::unique_ptr<QueryAST>& ast) {
    auto& select = ast->select;

    // SELECT <columns> FROM <table>
    consume(); // consume SELECT
    select.columns = parse_expression_list();

    if (current_.type == TokenType::KeywordFrom) {
        consume(); // consume FROM
        if (current_.type == TokenType::KeywordConnector) {
            consume(); // CONNECTOR
            select.from_connector = true;
            select.connector_name = parse_table_name();
            if (current_.type != TokenType::Dot) {
                throw common::Exception{
                    "Parser: expected . after connector name",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume(); // .
            select.connector_resource = parse_table_name();
        } else {
            auto [table, alias] = parse_table_ref();
            select.table = std::move(table);
            select.table_alias = std::move(alias);
        }

        while (current_.type == TokenType::KeywordJoin ||
               current_.type == TokenType::KeywordInner ||
               current_.type == TokenType::KeywordLeft ||
               current_.type == TokenType::KeywordRight) {
            QueryAST::Select::JoinClause join;
            join.join_type = "INNER";
            if (current_.type == TokenType::KeywordLeft) {
                join.join_type = "LEFT";
                consume();
                if (current_.type == TokenType::KeywordOuter) consume();
            } else if (current_.type == TokenType::KeywordRight) {
                join.join_type = "RIGHT";
                consume();
                if (current_.type == TokenType::KeywordOuter) consume();
            } else if (current_.type == TokenType::KeywordInner) {
                consume();
            }
            if (current_.type == TokenType::KeywordJoin) {
                consume();
            }
            auto [jt, ja] = parse_table_ref();
            join.table = std::move(jt);
            join.alias = std::move(ja);
            if (current_.type != TokenType::KeywordOn) {
                throw common::Exception{
                    "Parser: expected ON after JOIN",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume();
            join.on = parse_expression();
            select.joins.push_back(std::move(join));
        }
    }

    if (current_.type == TokenType::KeywordWhere) {
        consume(); // consume WHERE
        select.where = parse_expression();
    }

    if (current_.type == TokenType::KeywordGroup) {
        consume(); // consume GROUP
        if (current_.type == TokenType::KeywordBy) {
            consume(); // consume BY
            select.group_by = parse_expression_list();
        }
    }

    if (current_.type == TokenType::KeywordHaving) {
        consume(); // consume HAVING
        select.having = parse_expression();
    }

    if (current_.type == TokenType::KeywordOrder) {
        consume(); // consume ORDER
        if (current_.type == TokenType::KeywordBy) {
            consume(); // consume BY
            auto orders = parse_order_by_list();
            for (auto& ob : orders) {
                select.order_by.emplace_back(ob.column,
                    ob.direction == OrderBy::Direction::DESC);
            }
        }
    }

    if (current_.type == TokenType::KeywordLimit) {
        consume(); // consume LIMIT
        select.limit = parse_limit();
    }
}

void Parser::parse_insert(std::unique_ptr<QueryAST>& ast) {
    auto& insert = ast->insert;

    consume(); // consume INSERT
    if (current_.type != TokenType::KeywordInto) {
        throw common::Exception{
            "Parser: expected INTO after INSERT",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume INTO
    insert.table = parse_table_name();
    if (current_.type == TokenType::LParen) {
        insert.columns = parse_column_list();
    }
    if (current_.type == TokenType::KeywordValues) {
        consume(); // consume VALUES
        insert.values = parse_value_list();
    }
}

void Parser::parse_create(std::unique_ptr<QueryAST>& ast) {
    auto& create = ast->create;

    consume(); // consume CREATE
    if (current_.type == TokenType::KeywordMaterialized) {
        consume(); // consume MATERIALIZED
        if (current_.type != TokenType::KeywordView) {
            throw common::Exception{
                "Parser: expected VIEW after MATERIALIZED",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume VIEW
        create.kind = QueryAST::Create::Kind::MaterializedView;
        create.if_not_exists = parse_if_not_exists();
        create.view_name = parse_table_name();
        create.table_name = create.view_name;
        create.table = create.view_name;
        if (current_.type != TokenType::KeywordAs) {
            throw common::Exception{
                "Parser: expected AS after materialized view name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume AS
        if (current_.type != TokenType::KeywordSelect) {
            throw common::Exception{
                "Parser: expected SELECT after AS",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        auto select_ast = std::make_unique<QueryAST>();
        select_ast->query_type = QueryAST::QueryType::SELECT;
        parse_select(select_ast);
        create.select_definition = std::move(select_ast->select);
    } else if (current_.type == TokenType::KeywordView) {
        consume(); // consume VIEW
        create.kind = QueryAST::Create::Kind::View;
        create.if_not_exists = parse_if_not_exists();
        create.view_name = parse_table_name();
        create.table_name = create.view_name;
        create.table = create.view_name;
        if (current_.type != TokenType::KeywordAs) {
            throw common::Exception{
                "Parser: expected AS after view name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume AS
        if (current_.type != TokenType::KeywordSelect) {
            throw common::Exception{
                "Parser: expected SELECT after AS",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        auto select_ast = std::make_unique<QueryAST>();
        select_ast->query_type = QueryAST::QueryType::SELECT;
        parse_select(select_ast);
        create.select_definition = std::move(select_ast->select);
    } else if (current_.type == TokenType::KeywordDatabase) {
        consume(); // consume DATABASE
        create.kind = QueryAST::Create::Kind::Database;
        create.if_not_exists = parse_if_not_exists();
        create.database_name = parse_table_name();
    } else if (current_.type == TokenType::KeywordCluster) {
        consume(); // consume CLUSTER
        create.kind = QueryAST::Create::Kind::Cluster;
        create.if_not_exists = parse_if_not_exists();
        create.cluster_name = parse_table_name();
    } else if (consume_node_keyword()) {
        create.kind = QueryAST::Create::Kind::Node;
        create.if_not_exists = parse_if_not_exists();
        create.node_name = parse_table_name();
        parse_node_properties(create);
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "STORAGE_UNIT") {
        consume(); // consume STORAGE_UNIT
        create.kind = QueryAST::Create::Kind::StorageUnit;
        create.if_not_exists = parse_if_not_exists();
        create.storage_unit_name = parse_table_name();
        parse_storage_unit_properties(create);
    } else if (current_.type == TokenType::KeywordStorage) {
        consume(); // consume STORAGE
        if (current_.type != TokenType::KeywordUnit) {
            throw common::Exception{
                "Parser: expected UNIT after STORAGE",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume UNIT
        create.kind = QueryAST::Create::Kind::StorageUnit;
        create.if_not_exists = parse_if_not_exists();
        create.storage_unit_name = parse_table_name();
        parse_storage_unit_properties(create);
    } else if (consume_replica_group_keyword()) {
        create.kind = QueryAST::Create::Kind::ReplicaGroup;
        create.if_not_exists = parse_if_not_exists();
        create.replica_group_name = parse_table_name();
        parse_replica_group_properties(create);
    } else if (consume_shard_group_keyword()) {
        create.kind = QueryAST::Create::Kind::ShardGroup;
        create.if_not_exists = parse_if_not_exists();
        create.shard_group_name = parse_table_name();
        parse_shard_group_properties(create);
    } else if (consume_connector_keyword()) {
        create.kind = QueryAST::Create::Kind::Connector;
        create.if_not_exists = parse_if_not_exists();
        create.connector_name = parse_table_name();
        parse_connector_properties(create);
    } else if (consume_stream_keyword()) {
        create.kind = QueryAST::Create::Kind::Stream;
        create.if_not_exists = parse_if_not_exists();
        create.stream_name = parse_table_name();
        if (current_.type == TokenType::KeywordTopic) {
            consume();
            create.topic_name = parse_table_name();
        }
        parse_retention_clause(create);
    } else if (consume_topic_keyword()) {
        create.kind = QueryAST::Create::Kind::Topic;
        create.if_not_exists = parse_if_not_exists();
        create.topic_name = parse_table_name();
        parse_topic_properties(create);
        parse_retention_clause(create);
    } else if (consume_consumer_group_keyword()) {
        create.kind = QueryAST::Create::Kind::ConsumerGroup;
        create.if_not_exists = parse_if_not_exists();
        create.consumer_group_name = parse_table_name();
    } else if (consume_pipeline_keyword()) {
        create.kind = QueryAST::Create::Kind::Pipeline;
        create.if_not_exists = parse_if_not_exists();
        create.pipeline_name = parse_table_name();
        parse_pipeline_owner(create);
    } else if (consume_stage_keyword()) {
        create.kind = QueryAST::Create::Kind::Stage;
        create.if_not_exists = parse_if_not_exists();
        create.stage_name = parse_table_name();
        if (current_.type != TokenType::KeywordIn) {
            throw common::Exception{
                "Parser: expected IN PIPELINE after stage name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // IN
        if (!consume_pipeline_keyword()) {
            throw common::Exception{
                "Parser: expected PIPELINE after IN",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        create.pipeline_name = parse_table_name();
        parse_stage_order(create);
    } else if (consume_task_keyword()) {
        create.kind = QueryAST::Create::Kind::Task;
        create.if_not_exists = parse_if_not_exists();
        create.task_name = parse_table_name();
        if (current_.type != TokenType::KeywordIn) {
            throw common::Exception{
                "Parser: expected IN STAGE after task name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // IN
        if (!consume_stage_keyword()) {
            throw common::Exception{
                "Parser: expected STAGE after IN",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        create.stage_name = parse_table_name();
        if (current_.type != TokenType::KeywordIn) {
            throw common::Exception{
                "Parser: expected IN PIPELINE after stage name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // IN
        if (!consume_pipeline_keyword()) {
            throw common::Exception{
                "Parser: expected PIPELINE after IN",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        create.pipeline_name = parse_table_name();
        parse_task_properties(create);
    } else if (consume_trigger_keyword()) {
        create.kind = QueryAST::Create::Kind::Trigger;
        create.if_not_exists = parse_if_not_exists();
        create.trigger_name = parse_table_name();
        if (current_.type != TokenType::KeywordOn) {
            throw common::Exception{
                "Parser: expected ON PIPELINE after trigger name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // ON
        if (!consume_pipeline_keyword()) {
            throw common::Exception{
                "Parser: expected PIPELINE after ON",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        create.pipeline_name = parse_table_name();
        parse_trigger_properties(create);
    } else if (current_.type == TokenType::Identifier && current_.value == "MODEL_TEMPLATE") {
        consume();
        create.kind = QueryAST::Create::Kind::ModelTemplate;
        create.if_not_exists = parse_if_not_exists();
        create.model_template_name = parse_table_name();
        parse_model_clauses(create);
    } else if (current_.type == TokenType::Identifier && current_.value == "MODEL") {
        consume();
        create.kind = QueryAST::Create::Kind::Model;
        create.if_not_exists = parse_if_not_exists();
        create.model_name = parse_table_name();
        parse_model_clauses(create);
    } else if (current_.type == TokenType::Identifier && current_.value == "FEATURE_SET") {
        consume();
        create.kind = QueryAST::Create::Kind::FeatureSet;
        create.if_not_exists = parse_if_not_exists();
        create.feature_set_name = parse_table_name();
        parse_feature_set_clauses(create);
    } else if (current_.type == TokenType::Identifier && current_.value == "DATASET") {
        consume();
        create.kind = QueryAST::Create::Kind::Dataset;
        create.if_not_exists = parse_if_not_exists();
        create.dataset_name = parse_table_name();
        parse_model_clauses(create);
    } else if (current_.type == TokenType::Identifier && current_.value == "TRAINING_JOB") {
        consume();
        create.kind = QueryAST::Create::Kind::TrainingJob;
        create.if_not_exists = parse_if_not_exists();
        create.training_job_name = parse_table_name();
        parse_model_clauses(create);
    } else if (current_.type == TokenType::Identifier && current_.value == "TUNING_JOB") {
        consume();
        create.kind = QueryAST::Create::Kind::TuningJob;
        create.if_not_exists = parse_if_not_exists();
        create.tuning_job_name = parse_table_name();
        parse_model_clauses(create);
    } else if (current_.type == TokenType::KeywordTable) {
        consume(); // consume TABLE
        create.kind = QueryAST::Create::Kind::Table;
        create.if_not_exists = parse_if_not_exists();
        create.table_name = parse_table_name();
        create.table = create.table_name;
        create.columns = parse_column_definitions();
        if (current_.type == TokenType::KeywordEngine) {
            consume(); // consume ENGINE
            if (current_.type != TokenType::Eq) {
                throw common::Exception{
                    "Parser: expected = after ENGINE",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume(); // consume =
            create.engine = parse_table_name();
        }
        if (consume_storage_unit_keyword()) {
            create.storage_unit_name = parse_table_name();
        }
        if (consume_shard_group_keyword()) {
            if (!create.shard_group_name.empty()) {
                throw common::Exception{
                    "Parser: duplicate SHARD_GROUP clause",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            create.shard_group_name = parse_table_name();
        }
        if (consume_replica_group_keyword()) {
            if (!create.replica_group_name.empty()) {
                throw common::Exception{
                    "Parser: duplicate REPLICA_GROUP clause",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            create.replica_group_name = parse_table_name();
        }
    }
}

void Parser::parse_drop(std::unique_ptr<QueryAST>& ast) {
    auto& drop = ast->drop;

    if (current_.type == TokenType::KeywordTruncate) {
        drop.kind = QueryAST::Drop::Kind::Truncate;
        consume();
    } else if (current_.type == TokenType::KeywordDetach) {
        drop.kind = QueryAST::Drop::Kind::Detach;
        consume();
    } else {
        drop.kind = QueryAST::Drop::Kind::Drop;
        consume(); // consume DROP
    }

    if (current_.type == TokenType::KeywordMaterialized) {
        consume(); // consume MATERIALIZED
        if (current_.type != TokenType::KeywordView) {
            throw common::Exception{
                "Parser: expected VIEW after MATERIALIZED",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume VIEW
        drop.object_kind = QueryAST::ObjectKind::MaterializedView;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (current_.type == TokenType::KeywordView) {
        consume(); // consume VIEW
        drop.object_kind = QueryAST::ObjectKind::View;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (consume_storage_unit_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::StorageUnit;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (consume_connector_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::Connector;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (consume_stream_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::Stream;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (consume_topic_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::Topic;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (consume_consumer_group_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::ConsumerGroup;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (consume_pipeline_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::Pipeline;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (consume_stage_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::Stage;
        drop.if_exists = parse_if_exists();
        drop.stage_name = parse_table_name();
        if (current_.type != TokenType::KeywordFrom) {
            throw common::Exception{
                "Parser: expected FROM PIPELINE after stage name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // FROM
        if (!consume_pipeline_keyword()) {
            throw common::Exception{
                "Parser: expected PIPELINE after FROM",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        drop.pipeline_name = parse_table_name();
    } else if (consume_task_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::Task;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
        if (current_.type != TokenType::KeywordFrom) {
            throw common::Exception{
                "Parser: expected FROM STAGE after task name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // FROM
        if (!consume_stage_keyword()) {
            throw common::Exception{
                "Parser: expected STAGE after FROM",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        drop.stage_name = parse_table_name();
        if (current_.type != TokenType::KeywordIn) {
            throw common::Exception{
                "Parser: expected IN PIPELINE after stage name",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // IN
        if (!consume_pipeline_keyword()) {
            throw common::Exception{
                "Parser: expected PIPELINE after IN",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        drop.pipeline_name = parse_table_name();
    } else if (consume_trigger_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::Trigger;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
        if (current_.type == TokenType::KeywordFrom) {
            consume(); // FROM
            if (!consume_pipeline_keyword()) {
                throw common::Exception{
                    "Parser: expected PIPELINE after FROM",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            drop.pipeline_name = parse_table_name();
        }
    } else if (consume_replica_group_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::ReplicaGroup;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (consume_shard_group_keyword()) {
        drop.object_kind = QueryAST::ObjectKind::ShardGroup;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (current_.type == TokenType::Identifier && current_.value == "MODEL_TEMPLATE") {
        consume();
        drop.object_kind = QueryAST::ObjectKind::ModelTemplate;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (current_.type == TokenType::Identifier && current_.value == "MODEL") {
        consume();
        drop.object_kind = QueryAST::ObjectKind::Model;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (current_.type == TokenType::Identifier && current_.value == "FEATURE_SET") {
        consume();
        drop.object_kind = QueryAST::ObjectKind::FeatureSet;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (current_.type == TokenType::Identifier && current_.value == "DATASET") {
        consume();
        drop.object_kind = QueryAST::ObjectKind::Dataset;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (current_.type == TokenType::Identifier && current_.value == "TRAINING_JOB") {
        consume();
        drop.object_kind = QueryAST::ObjectKind::TrainingJob;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (current_.type == TokenType::Identifier && current_.value == "TUNING_JOB") {
        consume();
        drop.object_kind = QueryAST::ObjectKind::TuningJob;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    } else if (current_.type == TokenType::KeywordTable) {
        consume(); // consume TABLE
        drop.object_kind = QueryAST::ObjectKind::Table;
        drop.if_exists = parse_if_exists();
        drop.table = parse_table_name();
    }
}

void Parser::parse_alter(std::unique_ptr<QueryAST>& ast) {
    auto& alter = ast->alter;

    consume(); // consume ALTER
    if (consume_stream_keyword()) {
        alter.target = QueryAST::Alter::Target::Stream;
        alter.table = parse_table_name();
        while (current_.type != TokenType::EndOfQuery &&
               current_.type != TokenType::Semicolon) {
            if (current_.type != TokenType::KeywordSet) {
                throw common::Exception{
                    "Parser: expected SET in ALTER STREAM",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume(); // SET
            QueryAST::Alter::NodeSet cmd;
            cmd.property = parse_name_or_keyword();
            if (cmd.property == "RETENTION" || cmd.property == "retention") {
                if (current_.type == TokenType::KeywordForever ||
                    (current_.type == TokenType::Identifier &&
                     current_.value == "FOREVER")) {
                    consume();
                    cmd.value = "FOREVER";
                } else if (current_.type == TokenType::IntegerLiteral) {
                    cmd.value = current_.value;
                    consume();
                    if (current_.type == TokenType::KeywordDays ||
                        (current_.type == TokenType::Identifier &&
                         current_.value == "DAYS")) {
                        consume();
                        cmd.value += " DAYS";
                    }
                } else {
                    cmd.value = parse_table_name();
                    if (current_.type == TokenType::KeywordDays ||
                        (current_.type == TokenType::Identifier &&
                         current_.value == "DAYS")) {
                        consume();
                        cmd.value += " DAYS";
                    }
                }
            } else {
                cmd.value = parse_property_value();
            }
            alter.stream_sets.push_back(std::move(cmd));
            if (current_.type == TokenType::Comma) {
                consume();
            } else {
                break;
            }
        }
        return;
    }
    if (consume_pipeline_keyword()) {
        alter.target = QueryAST::Alter::Target::Pipeline;
        alter.table = parse_table_name();
        while (current_.type != TokenType::EndOfQuery &&
               current_.type != TokenType::Semicolon) {
            if (current_.type != TokenType::KeywordSet) {
                throw common::Exception{
                    "Parser: expected SET in ALTER PIPELINE",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume(); // SET
            QueryAST::Alter::NodeSet cmd;
            cmd.property = parse_name_or_keyword();
            cmd.value = parse_property_value();
            alter.pipeline_sets.push_back(std::move(cmd));
            if (current_.type == TokenType::Comma) {
                consume();
            } else {
                break;
            }
        }
        return;
    }
    if (consume_connector_keyword()) {
        alter.target = QueryAST::Alter::Target::Connector;
        alter.table = parse_table_name();
        while (current_.type != TokenType::EndOfQuery &&
               current_.type != TokenType::Semicolon) {
            if (current_.type != TokenType::KeywordSet) {
                throw common::Exception{
                    "Parser: expected SET in ALTER CONNECTOR",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume(); // SET
            QueryAST::Alter::NodeSet cmd;
            cmd.property = parse_name_or_keyword();
            cmd.value = parse_property_value();
            alter.connector_sets.push_back(std::move(cmd));
            if (current_.type == TokenType::Comma) {
                consume();
            } else {
                break;
            }
        }
        return;
    }
    if (consume_node_keyword()) {
        alter.target = QueryAST::Alter::Target::Node;
        alter.table = parse_table_name();
        while (current_.type != TokenType::EndOfQuery &&
               current_.type != TokenType::Semicolon) {
            if (current_.type != TokenType::KeywordSet) {
                throw common::Exception{
                    "Parser: expected SET in ALTER NODE",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume(); // SET
            QueryAST::Alter::NodeSet cmd;
            cmd.property = parse_name_or_keyword();
            cmd.value = parse_name_or_keyword();
            alter.node_sets.push_back(std::move(cmd));
            if (current_.type == TokenType::Comma) {
                consume();
            } else {
                break;
            }
        }
        return;
    }
    if (consume_replica_group_keyword()) {
        alter.target = QueryAST::Alter::Target::ReplicaGroup;
        alter.table = parse_table_name();
        while (current_.type != TokenType::EndOfQuery &&
               current_.type != TokenType::Semicolon) {
            if (current_.type != TokenType::KeywordSet) {
                throw common::Exception{
                    "Parser: expected SET in ALTER REPLICA_GROUP",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume(); // SET
            QueryAST::Alter::NodeSet cmd;
            cmd.property = parse_name_or_keyword();
            cmd.value = parse_name_or_keyword();
            alter.replica_group_sets.push_back(std::move(cmd));
            if (current_.type == TokenType::Comma) {
                consume();
            } else {
                break;
            }
        }
        return;
    }
    if (consume_shard_group_keyword()) {
        alter.target = QueryAST::Alter::Target::ShardGroup;
        alter.table = parse_table_name();
        while (current_.type != TokenType::EndOfQuery &&
               current_.type != TokenType::Semicolon) {
            if (current_.type != TokenType::KeywordSet) {
                throw common::Exception{
                    "Parser: expected SET in ALTER SHARD_GROUP",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume(); // SET
            QueryAST::Alter::NodeSet cmd;
            cmd.property = parse_name_or_keyword();
            cmd.value = parse_name_or_keyword();
            alter.shard_group_sets.push_back(std::move(cmd));
            if (current_.type == TokenType::Comma) {
                consume();
            } else {
                break;
            }
        }
        return;
    }
    if (current_.type != TokenType::KeywordTable) {
        throw common::Exception{
            "Parser: expected TABLE, NODE, REPLICA_GROUP, or SHARD_GROUP after ALTER",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume TABLE
    alter.target = QueryAST::Alter::Target::Table;
    alter.table = parse_table_name();

    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        ASTAlterQuery::AlterCommand cmd;

        if (current_.type == TokenType::KeywordAdd) {
            consume();
            if (current_.type == TokenType::KeywordColumn) {
                consume();
            }
            cmd.type = ASTAlterQuery::AlterCommand::Type::ADD_COLUMN;
            cmd.column_name = parse_table_name();
            cmd.column_type = parse_table_name();
        } else if (current_.type == TokenType::KeywordDrop) {
            consume();
            if (current_.type == TokenType::KeywordColumn) {
                consume();
            }
            cmd.type = ASTAlterQuery::AlterCommand::Type::DROP_COLUMN;
            cmd.column_name = parse_table_name();
        } else if (current_.type == TokenType::KeywordModify) {
            consume();
            if (current_.type == TokenType::KeywordColumn) {
                consume();
            }
            cmd.type = ASTAlterQuery::AlterCommand::Type::MODIFY_COLUMN;
            cmd.column_name = parse_table_name();
            cmd.column_type = parse_table_name();
        } else {
            throw common::Exception{
                "Parser: unknown ALTER command",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }

        alter.commands.push_back(std::move(cmd));

        if (current_.type == TokenType::Comma) {
            consume();
        } else {
            break;
        }
    }
}

auto Parser::parse_if_not_exists() -> bool {
    if (current_.type == TokenType::KeywordIf) {
        consume();
        if (current_.type != TokenType::KeywordNot) {
            throw common::Exception{
                "Parser: expected NOT after IF",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume();
        if (current_.type != TokenType::KeywordExists) {
            throw common::Exception{
                "Parser: expected EXISTS after IF NOT",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume();
        return true;
    }
    return false;
}

auto Parser::parse_if_exists() -> bool {
    if (current_.type == TokenType::KeywordIf) {
        consume();
        if (current_.type != TokenType::KeywordExists) {
            throw common::Exception{
                "Parser: expected EXISTS after IF",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume();
        return true;
    }
    return false;
}

void Parser::parse_show(std::unique_ptr<QueryAST>& ast) {
    auto& show = ast->show;

    consume(); // consume SHOW
    if (current_.type == TokenType::KeywordPipelineRuns) {
        consume();
        show.show_type = QueryAST::Show::ShowType::PIPELINE_RUNS;
        if (current_.type == TokenType::KeywordFor) {
            consume();
            if (!consume_pipeline_keyword()) {
                throw common::Exception{
                    "Parser: expected PIPELINE after FOR",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            show.pipeline_name = parse_table_name();
        }
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "PIPELINE_METRICS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::PIPELINE_METRICS;
        if (current_.type == TokenType::KeywordFor) {
            consume();
            if (!consume_pipeline_keyword()) {
                throw common::Exception{
                    "Parser: expected PIPELINE after FOR",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            show.pipeline_name = parse_table_name();
        }
    } else if (current_.type == TokenType::KeywordConnector) {
        consume(); // CONNECTOR
        if (current_.type == TokenType::KeywordCapabilities) {
            consume();
            show.show_type = QueryAST::Show::ShowType::CONNECTOR_CAPABILITIES;
            if (current_.type == TokenType::Identifier ||
                current_.type == TokenType::KeywordTable) {
                show.connector_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordStatus) {
            consume();
            show.show_type = QueryAST::Show::ShowType::CONNECTOR_STATUS;
            if (current_.type == TokenType::Identifier ||
                current_.type == TokenType::KeywordTable) {
                show.connector_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordConnectors ||
                   (current_.type == TokenType::Identifier &&
                    current_.value == "CONNECTORS")) {
            consume();
            show.show_type = QueryAST::Show::ShowType::CONNECTORS;
        } else {
            show.show_type = QueryAST::Show::ShowType::CONNECTORS;
        }
    } else if (current_.type == TokenType::KeywordConnectors ||
               (current_.type == TokenType::Identifier &&
                current_.value == "CONNECTORS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::CONNECTORS;
    } else if (current_.type == TokenType::KeywordNode) {
        consume(); // NODE
        if (current_.type == TokenType::KeywordMetrics) {
            consume();
            show.show_type = QueryAST::Show::ShowType::NODE_METRICS;
            if (current_.type == TokenType::Identifier ||
                current_.type == TokenType::KeywordTable) {
                show.node_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordCapabilities) {
            consume();
            show.show_type = QueryAST::Show::ShowType::NODE_CAPABILITIES;
            if (current_.type == TokenType::Identifier ||
                current_.type == TokenType::KeywordTable) {
                show.node_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordReplicas) {
            consume();
            show.show_type = QueryAST::Show::ShowType::NODE_REPLICAS;
            if (current_.type == TokenType::Identifier ||
                current_.type == TokenType::KeywordTable) {
                show.node_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordPartition ||
                   (current_.type == TokenType::Identifier &&
                    current_.value == "PARTITIONS")) {
            consume();
            show.show_type = QueryAST::Show::ShowType::NODE_PARTITIONS;
            if (current_.type == TokenType::Identifier ||
                current_.type == TokenType::KeywordTable) {
                show.node_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordNodes) {
            consume();
            show.show_type = QueryAST::Show::ShowType::NODES;
        } else {
            show.show_type = QueryAST::Show::ShowType::NODES;
        }
    } else if (current_.type == TokenType::KeywordNodes) {
        consume();
        show.show_type = QueryAST::Show::ShowType::NODES;
    } else if (current_.type == TokenType::KeywordClusters) {
        consume();
        show.show_type = QueryAST::Show::ShowType::CLUSTERS;
    } else if (current_.type == TokenType::KeywordShard) {
        consume();
        if (current_.type == TokenType::KeywordStatus) {
            consume();
            show.show_type = QueryAST::Show::ShowType::SHARD_STATUS;
        } else if (current_.type == TokenType::KeywordShardGroups ||
                   (current_.type == TokenType::Identifier &&
                    current_.value == "GROUPS")) {
            consume();
            show.show_type = QueryAST::Show::ShowType::SHARD_GROUPS;
        } else {
            throw common::Exception{
                "Parser: expected GROUPS or STATUS after SHARD",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
    } else if (current_.type == TokenType::KeywordShardGroups ||
               (current_.type == TokenType::Identifier &&
                current_.value == "SHARD_GROUPS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::SHARD_GROUPS;
    } else if (current_.type == TokenType::KeywordShards ||
               (current_.type == TokenType::Identifier &&
                current_.value == "SHARDS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::SHARDS;
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "SHARD_STATUS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::SHARD_STATUS;
    } else if (current_.type == TokenType::KeywordReplicaGroups ||
               (current_.type == TokenType::Identifier &&
                current_.value == "REPLICA_GROUPS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::REPLICA_GROUPS;
    } else if (current_.type == TokenType::KeywordReplication) {
        consume();
        if (current_.type != TokenType::KeywordStatus) {
            throw common::Exception{
                "Parser: expected STATUS after REPLICATION",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume();
        show.show_type = QueryAST::Show::ShowType::REPLICATION_STATUS;
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "REPLICATION_STATUS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::REPLICATION_STATUS;
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "NODES") {
        consume();
        show.show_type = QueryAST::Show::ShowType::NODES;
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "NODE_METRICS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::NODE_METRICS;
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "NODE_CAPABILITIES") {
        consume();
        show.show_type = QueryAST::Show::ShowType::NODE_CAPABILITIES;
    } else if (current_.type == TokenType::Identifier &&
        current_.value == "STORAGE_UNITS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::STORAGE_UNITS;
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "STORAGE_USAGE") {
        consume();
        show.show_type = QueryAST::Show::ShowType::STORAGE_USAGE;
    } else if (current_.type == TokenType::KeywordStorage) {
        consume(); // consume STORAGE
        if (current_.type == TokenType::KeywordUsage) {
            consume(); // consume USAGE
            show.show_type = QueryAST::Show::ShowType::STORAGE_USAGE;
        } else if (current_.type == TokenType::KeywordUnits ||
                   current_.type == TokenType::KeywordUnit) {
            consume(); // consume UNITS or UNIT
            show.show_type = QueryAST::Show::ShowType::STORAGE_UNITS;
        } else {
            throw common::Exception{
                "Parser: expected UNITS or USAGE after STORAGE",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
    } else if (current_.type == TokenType::KeywordMaterialized) {
        consume(); // consume MATERIALIZED
        if (current_.type != TokenType::KeywordViews &&
            current_.type != TokenType::KeywordView) {
            throw common::Exception{
                "Parser: expected VIEWS after MATERIALIZED",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume VIEWS
        show.show_type = QueryAST::Show::ShowType::MATERIALIZED_VIEWS;
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "MATERIALIZED_VIEWS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::MATERIALIZED_VIEWS;
    } else if (current_.type == TokenType::KeywordViews ||
               current_.type == TokenType::KeywordView) {
        consume(); // consume VIEWS
        show.show_type = QueryAST::Show::ShowType::VIEWS;
    } else if (current_.type == TokenType::KeywordPipeline) {
        consume();
        if (current_.type == TokenType::KeywordMetrics) {
            consume();
            show.show_type = QueryAST::Show::ShowType::PIPELINE_METRICS;
            if (current_.type == TokenType::KeywordFor) {
                consume();
                if (!consume_pipeline_keyword()) {
                    throw common::Exception{
                        "Parser: expected PIPELINE after FOR",
                        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
                }
                show.pipeline_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordPipelineRuns ||
                   (current_.type == TokenType::Identifier &&
                    current_.value == "PIPELINE_RUNS")) {
            consume();
            show.show_type = QueryAST::Show::ShowType::PIPELINE_RUNS;
            if (current_.type == TokenType::KeywordFor) {
                consume();
                if (!consume_pipeline_keyword()) {
                    throw common::Exception{
                        "Parser: expected PIPELINE after FOR",
                        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
                }
                show.pipeline_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordPipelines ||
                   (current_.type == TokenType::Identifier &&
                    current_.value == "PIPELINES")) {
            consume();
            show.show_type = QueryAST::Show::ShowType::PIPELINES;
        } else {
            show.show_type = QueryAST::Show::ShowType::PIPELINES;
        }
    } else if (current_.type == TokenType::KeywordPipelines ||
               (current_.type == TokenType::Identifier &&
                current_.value == "PIPELINES")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::PIPELINES;
    } else if (current_.type == TokenType::KeywordStages ||
               (current_.type == TokenType::Identifier &&
                current_.value == "STAGES")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::STAGES;
        if (current_.type == TokenType::KeywordFrom) {
            consume();
            if (!consume_pipeline_keyword()) {
                throw common::Exception{
                    "Parser: expected PIPELINE after FROM",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            show.pipeline_name = parse_table_name();
        }
    } else if (current_.type == TokenType::KeywordTasks ||
               (current_.type == TokenType::Identifier &&
                current_.value == "TASKS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::TASKS;
        if (current_.type == TokenType::KeywordFrom) {
            consume();
            if (!consume_pipeline_keyword()) {
                throw common::Exception{
                    "Parser: expected PIPELINE after FROM",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            show.pipeline_name = parse_table_name();
        }
    } else if (current_.type == TokenType::KeywordTriggers ||
               (current_.type == TokenType::Identifier &&
                current_.value == "TRIGGERS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::TRIGGERS;
        if (current_.type == TokenType::KeywordFrom) {
            consume();
            if (!consume_pipeline_keyword()) {
                throw common::Exception{
                    "Parser: expected PIPELINE after FROM",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            show.pipeline_name = parse_table_name();
        }
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "PIPELINE_RUNS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::PIPELINE_RUNS;
        if (current_.type == TokenType::KeywordFor) {
            consume();
            if (!consume_pipeline_keyword()) {
                throw common::Exception{
                    "Parser: expected PIPELINE after FOR",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            show.pipeline_name = parse_table_name();
        }
    } else if (current_.type == TokenType::Identifier &&
               current_.value == "PIPELINE_METRICS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::PIPELINE_METRICS;
        if (current_.type == TokenType::KeywordFor) {
            consume();
            if (!consume_pipeline_keyword()) {
                throw common::Exception{
                    "Parser: expected PIPELINE after FOR",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            show.pipeline_name = parse_table_name();
        }
    } else if (current_.type == TokenType::KeywordStreamMetrics ||
               (current_.type == TokenType::Identifier &&
                current_.value == "STREAM_METRICS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::STREAM_METRICS;
        if (current_.type == TokenType::KeywordFor) {
            consume();
            if (!consume_stream_keyword()) {
                throw common::Exception{
                    "Parser: expected STREAM after FOR",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            show.stream_name = parse_table_name();
        }
    } else if (current_.type == TokenType::KeywordStream) {
        consume();
        if (current_.type == TokenType::KeywordMetrics) {
            consume();
            show.show_type = QueryAST::Show::ShowType::STREAM_METRICS;
            if (current_.type == TokenType::KeywordFor) {
                consume();
                if (!consume_stream_keyword()) {
                    throw common::Exception{
                        "Parser: expected STREAM after FOR",
                        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
                }
                show.stream_name = parse_table_name();
            }
        } else if (current_.type == TokenType::KeywordStreams ||
                   (current_.type == TokenType::Identifier &&
                    current_.value == "STREAMS")) {
            consume();
            show.show_type = QueryAST::Show::ShowType::STREAMS;
        } else {
            show.show_type = QueryAST::Show::ShowType::STREAMS;
        }
    } else if (current_.type == TokenType::KeywordStreams ||
               (current_.type == TokenType::Identifier &&
                current_.value == "STREAMS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::STREAMS;
    } else if (current_.type == TokenType::KeywordTopic) {
        consume();
        if (current_.type == TokenType::KeywordTopics ||
            (current_.type == TokenType::Identifier &&
             current_.value == "TOPICS")) {
            consume();
        }
        show.show_type = QueryAST::Show::ShowType::TOPICS;
    } else if (current_.type == TokenType::KeywordTopics ||
               (current_.type == TokenType::Identifier &&
                current_.value == "TOPICS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::TOPICS;
    } else if (current_.type == TokenType::KeywordConsumerGroups ||
               (current_.type == TokenType::Identifier &&
                current_.value == "CONSUMER_GROUPS")) {
        consume();
        show.show_type = QueryAST::Show::ShowType::CONSUMER_GROUPS;
    } else if (current_.type == TokenType::Identifier && current_.value == "MODELS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::MODELS;
    } else if (current_.type == TokenType::Identifier && current_.value == "MODEL_TEMPLATES") {
        consume();
        show.show_type = QueryAST::Show::ShowType::MODEL_TEMPLATES;
    } else if (current_.type == TokenType::Identifier && current_.value == "MODEL") {
        consume(); // MODEL
        const std::string sub = upper_str(current_.value);
        if (sub == "VERSIONS") {
            consume();
            show.show_type = QueryAST::Show::ShowType::MODEL_VERSIONS;
            if (current_.type == TokenType::Identifier) show.model_name = parse_table_name();
        } else if (sub == "ENDPOINTS") {
            consume();
            show.show_type = QueryAST::Show::ShowType::MODEL_ENDPOINTS;
            if (current_.type == TokenType::Identifier) show.model_name = parse_table_name();
        } else if (current_.type == TokenType::KeywordMetrics || sub == "METRICS") {
            consume();
            show.show_type = QueryAST::Show::ShowType::MODEL_METRICS;
            if (current_.type == TokenType::Identifier) show.model_name = parse_table_name();
        } else if (sub == "DRIFT") {
            consume();
            show.show_type = QueryAST::Show::ShowType::MODEL_DRIFT;
            if (current_.type == TokenType::Identifier) show.model_name = parse_table_name();
        } else if (sub == "TEMPLATES") {
            consume();
            show.show_type = QueryAST::Show::ShowType::MODEL_TEMPLATES;
        } else {
            throw common::Exception{
                "Parser: expected VERSIONS / ENDPOINTS / METRICS / DRIFT after SHOW MODEL",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
    } else if (current_.type == TokenType::Identifier && current_.value == "FEATURE_SETS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::FEATURE_SETS;
    } else if (current_.type == TokenType::Identifier && current_.value == "DATASETS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::DATASETS;
    } else if (current_.type == TokenType::Identifier && current_.value == "TRAINING_JOBS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::TRAINING_JOBS;
    } else if (current_.type == TokenType::Identifier && current_.value == "TUNING_JOBS") {
        consume();
        show.show_type = QueryAST::Show::ShowType::TUNING_JOBS;
    } else if (current_.type == TokenType::KeywordTable) {
        consume(); // consume TABLES
        show.show_type = QueryAST::Show::ShowType::TABLES;
    } else {
        show.show_type = QueryAST::Show::ShowType::DATABASES;
    }
}

void Parser::parse_describe(std::unique_ptr<QueryAST>& ast) {
    auto& describe = ast->describe;

    consume(); // consume DESCRIBE/DESC
    if (consume_stream_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::Stream;
    } else if (consume_topic_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::Topic;
    } else if (consume_consumer_group_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::ConsumerGroup;
    } else if (consume_pipeline_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::Pipeline;
    } else if (consume_connector_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::Connector;
    } else if (consume_storage_unit_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::StorageUnit;
    } else if (consume_replica_group_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::ReplicaGroup;
    } else if (consume_shard_group_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::ShardGroup;
    } else if (consume_node_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::Node;
    } else if (current_.type == TokenType::KeywordCluster) {
        consume();
        describe.object_kind = QueryAST::ObjectKind::Cluster;
    } else if (current_.type == TokenType::KeywordMaterialized) {
        consume(); // consume MATERIALIZED
        if (current_.type != TokenType::KeywordView) {
            throw common::Exception{
                "Parser: expected VIEW after MATERIALIZED",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume VIEW
        describe.object_kind = QueryAST::ObjectKind::MaterializedView;
    } else if (current_.type == TokenType::KeywordView) {
        consume(); // consume VIEW
        describe.object_kind = QueryAST::ObjectKind::View;
    } else if (current_.type == TokenType::Identifier && current_.value == "MODEL_TEMPLATE") {
        consume();
        describe.object_kind = QueryAST::ObjectKind::ModelTemplate;
    } else if (current_.type == TokenType::Identifier && current_.value == "MODEL") {
        consume();
        if (upper_str(current_.value) == "VERSION") {
            consume();
            describe.object_kind = QueryAST::ObjectKind::ModelVersion;
            parse_model_version_ref(describe.model_name, describe.version);
            describe.table_name = describe.model_name;
            return;
        }
        describe.object_kind = QueryAST::ObjectKind::Model;
    } else if (current_.type == TokenType::Identifier && current_.value == "FEATURE_SET") {
        consume();
        describe.object_kind = QueryAST::ObjectKind::FeatureSet;
    } else if (current_.type == TokenType::Identifier && current_.value == "DATASET") {
        consume();
        describe.object_kind = QueryAST::ObjectKind::Dataset;
    } else if (current_.type == TokenType::Identifier && current_.value == "TRAINING_JOB") {
        consume();
        describe.object_kind = QueryAST::ObjectKind::TrainingJob;
    } else if (current_.type == TokenType::Identifier && current_.value == "TUNING_JOB") {
        consume();
        describe.object_kind = QueryAST::ObjectKind::TuningJob;
    } else {
        describe.object_kind = QueryAST::ObjectKind::Table;
    }
    describe.table_name = parse_table_name();
}

void Parser::parse_refresh(std::unique_ptr<QueryAST>& ast) {
    auto& refresh = ast->refresh;

    consume(); // consume REFRESH
    if (current_.type != TokenType::KeywordMaterialized) {
        throw common::Exception{
            "Parser: expected MATERIALIZED after REFRESH",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume MATERIALIZED
    if (current_.type != TokenType::KeywordView) {
        throw common::Exception{
            "Parser: expected VIEW after MATERIALIZED",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume VIEW
    refresh.name = parse_table_name();
}

void Parser::parse_register(std::unique_ptr<QueryAST>& ast) {
    auto& reg = ast->register_node;
    consume(); // REGISTER
    if (!consume_node_keyword()) {
        throw common::Exception{
            "Parser: expected NODE after REGISTER",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    reg.node_name = parse_table_name();
    if (current_.type != TokenType::KeywordHost) {
        throw common::Exception{
            "Parser: expected HOST after node name",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // HOST
    reg.host = parse_property_value();
    if (current_.type != TokenType::KeywordPort) {
        throw common::Exception{
            "Parser: expected PORT after HOST",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // PORT
    if (current_.type != TokenType::IntegerLiteral) {
        throw common::Exception{
            "Parser: expected port number",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    reg.port = static_cast<uint16_t>(std::stoi(current_.value));
    consume();
}

void Parser::parse_drain(std::unique_ptr<QueryAST>& ast) {
    consume(); // DRAIN
    if (!consume_node_keyword()) {
        throw common::Exception{
            "Parser: expected NODE after DRAIN",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    ast->drain_node.node_name = parse_table_name();
}

void Parser::parse_remove(std::unique_ptr<QueryAST>& ast) {
    consume(); // REMOVE
    if (!consume_node_keyword()) {
        throw common::Exception{
            "Parser: expected NODE after REMOVE",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    ast->remove_node.if_exists = parse_if_exists();
    ast->remove_node.node_name = parse_table_name();
}

void Parser::parse_test(std::unique_ptr<QueryAST>& ast) {
    consume(); // TEST
    if (!consume_connector_keyword()) {
        throw common::Exception{
            "Parser: expected CONNECTOR after TEST",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    ast->test_query.object_kind = QueryAST::ObjectKind::Connector;
    ast->test_query.name = parse_table_name();
}

void Parser::parse_discover(std::unique_ptr<QueryAST>& ast) {
    consume(); // DISCOVER
    if (current_.type != TokenType::KeywordSchema) {
        throw common::Exception{
            "Parser: expected SCHEMA after DISCOVER",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // SCHEMA
    if (current_.type != TokenType::KeywordFrom) {
        throw common::Exception{
            "Parser: expected FROM after DISCOVER SCHEMA",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // FROM
    if (!consume_connector_keyword()) {
        throw common::Exception{
            "Parser: expected CONNECTOR after FROM",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    ast->discover.connector_name = parse_table_name();
}

void Parser::parse_explain(std::unique_ptr<QueryAST>& ast) {
    consume(); // consume EXPLAIN
    // Parse the query to be explained
    ast->explain.explain_query = parse_query();
}

void Parser::parse_use(std::unique_ptr<QueryAST>& ast) {
    auto& use = ast->use;

    consume(); // consume USE

    // Accept the optional DATABASE keyword (USE DATABASE <name>), matching
    // ClickHouse's `ParserUseQuery` which allows both `USE db` and `USE DATABASE db`.
    if (current_.type == TokenType::KeywordDatabase) {
        consume(); // consume DATABASE
    }

    use.database_name = parse_table_name();
}

// ── Expression parsing ──

auto Parser::parse_expression() -> std::shared_ptr<ASTExpr> {
    auto left = parse_comparison();

    while (current_.type == TokenType::KeywordAnd ||
           current_.type == TokenType::KeywordOr) {
        auto op = std::make_shared<ASTBinaryOp>();
        op->op = current_.type == TokenType::KeywordAnd ?
                  ASTBinaryOp::Op::And : ASTBinaryOp::Op::Or;
        consume();
        auto right = parse_comparison();
        op->left = left;
        op->right = right;
        left = op;
    }

    return left;
}

auto Parser::parse_comparison() -> std::shared_ptr<ASTExpr> {
    auto left = parse_term();

    while (current_.type == TokenType::Eq || current_.type == TokenType::Ne ||
           current_.type == TokenType::Gt || current_.type == TokenType::Lt ||
           current_.type == TokenType::Ge || current_.type == TokenType::Le ||
           current_.type == TokenType::KeywordIn) {
        auto op = std::make_shared<ASTBinaryOp>();
        if (current_.type == TokenType::KeywordIn) {
            op->op = ASTBinaryOp::Op::In;
            consume();
            if (current_.type != TokenType::LParen) {
                throw common::Exception{
                    "Parser: expected ( after IN",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume();
            auto subq = std::make_shared<ASTSubQueryExpr>();
            subq->query = parse_subquery();
            if (current_.type != TokenType::RParen) {
                throw common::Exception{
                    "Parser: expected ) after IN subquery",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume();
            op->left = left;
            op->right = subq;
            left = op;
            continue;
        }
        switch (current_.type) {
            case TokenType::Eq: op->op = ASTBinaryOp::Op::Eq; break;
            case TokenType::Ne: op->op = ASTBinaryOp::Op::Ne; break;
            case TokenType::Gt: op->op = ASTBinaryOp::Op::Gt; break;
            case TokenType::Lt: op->op = ASTBinaryOp::Op::Lt; break;
            case TokenType::Ge: op->op = ASTBinaryOp::Op::Ge; break;
            case TokenType::Le: op->op = ASTBinaryOp::Op::Le; break;
            default: break;
        }
        consume();
        auto right = parse_term();
        op->left = left;
        op->right = right;
        left = op;
    }

    return left;
}

auto Parser::parse_term() -> std::shared_ptr<ASTExpr> {
    auto left = parse_factor();

    while (current_.type == TokenType::Plus ||
           current_.type == TokenType::Minus ||
           current_.type == TokenType::Star ||
           current_.type == TokenType::Slash ||
           current_.type == TokenType::Percent) {
        auto op = std::make_shared<ASTBinaryOp>();
        switch (current_.type) {
            case TokenType::Plus:   op->op = ASTBinaryOp::Op::Add;   break;
            case TokenType::Minus:  op->op = ASTBinaryOp::Op::Sub;   break;
            case TokenType::Star:   op->op = ASTBinaryOp::Op::Mul;   break;
            case TokenType::Slash:  op->op = ASTBinaryOp::Op::Div;   break;
            case TokenType::Percent: op->op = ASTBinaryOp::Op::Div;  break;
            default: break;
        }
        consume();
        auto right = parse_factor();
        op->left = left;
        op->right = right;
        left = op;
    }

    if (current_.type == TokenType::KeywordAs) {
        consume();
        if (current_.type != TokenType::Identifier) {
            throw common::Exception{
                "Parser: expected alias name after AS",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        auto alias = std::make_shared<ASTAlias>();
        alias->alias = current_.value;
        consume();
        alias->expression = left;
        return alias;
    }

    return left;
}

auto Parser::parse_factor() -> std::shared_ptr<ASTExpr> {
    if (current_.type == TokenType::LParen) {
        consume(); // consume (
        if (current_.type == TokenType::KeywordSelect) {
            auto subq = std::make_shared<ASTSubQueryExpr>();
            subq->query = parse_subquery();
            if (current_.type != TokenType::RParen) {
                throw common::Exception{
                    "Parser: expected ) after subquery",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume();
            return subq;
        }
        auto expr = parse_expression();
        if (current_.type != TokenType::RParen) {
            throw common::Exception{
                "Parser: expected )",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume(); // consume )
        return expr;
    }

    // Literals
    if (current_.type == TokenType::IntegerLiteral) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = static_cast<int64_t>(std::stoll(current_.value));
        consume();
        return lit;
    }

    if (current_.type == TokenType::FloatLiteral) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = std::stod(current_.value);
        consume();
        return lit;
    }

    if (current_.type == TokenType::StringLiteral) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = current_.value;
        consume();
        return lit;
    }

    if (current_.type == TokenType::KeywordNull) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = std::monostate{};
        consume();
        return lit;
    }

    if (current_.type == TokenType::KeywordTrue) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = true;
        consume();
        return lit;
    }

    if (current_.type == TokenType::KeywordFalse) {
        auto lit = std::make_shared<ASTLiteral>();
        lit->value = false;
        consume();
        return lit;
    }

    // Handle '*' for SELECT *
    if (current_.type == TokenType::Star) {
        auto col = std::make_shared<ASTColumnRef>();
        col->column = "*";
        consume();
        return col;
    }

    // Identifiers and aggregate function keywords
    if (current_.type == TokenType::Identifier ||
        current_.type == TokenType::KeywordSum ||
        current_.type == TokenType::KeywordCount ||
        current_.type == TokenType::KeywordAvg ||
        current_.type == TokenType::KeywordMin ||
        current_.type == TokenType::KeywordMax) {
        std::string name = current_.value;
        consume();
        if (current_.type == TokenType::LParen) {
            consume();
            auto func = std::make_shared<ASTFunction>();
            func->name = name;
            static const std::unordered_set<std::string> aggs = {
                "COUNT", "SUM", "AVG", "MIN", "MAX"};
            std::string upper = name;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            func->is_aggregate = aggs.count(upper) > 0;
            if (current_.type == TokenType::Star) {
                auto star = std::make_shared<ASTColumnRef>();
                star->column = "*";
                func->args.push_back(star);
                consume();
            } else if (current_.type != TokenType::RParen) {
                func->args.push_back(parse_expression());
                while (current_.type == TokenType::Comma) {
                    consume();
                    func->args.push_back(parse_expression());
                }
            }
            if (current_.type != TokenType::RParen) {
                throw common::Exception{
                    "Parser: expected ) after function call",
                    static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
            }
            consume();
            if (current_.type == TokenType::KeywordOver) {
                consume();
                if (current_.type != TokenType::LParen) {
                    throw common::Exception{
                        "Parser: expected ( after OVER",
                        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
                }
                consume();
                ASTFunction::WindowSpec spec;
                parse_window_spec(spec);
                if (current_.type != TokenType::RParen) {
                    throw common::Exception{
                        "Parser: expected ) after OVER clause",
                        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
                }
                consume();
                func->window = std::move(spec);
            }
            return func;
        }
        if (current_.type == TokenType::Dot) {
            consume();
            auto col = std::make_shared<ASTColumnRef>();
            col->table = name;
            col->column = current_.value;
            consume();
            return col;
        }
        auto col = std::make_shared<ASTColumnRef>();
        col->column = name;
        return col;
    }

    std::fprintf(stderr, "parse_factor unexpected token='%s' type=%d\n", current_.value.c_str(), static_cast<int>(current_.type));
    throw common::Exception{
        "Parser: unexpected token '" + current_.value + "'",
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto Parser::parse_expression_list() -> std::vector<std::shared_ptr<ASTExpr>> {
    std::vector<std::shared_ptr<ASTExpr>> exprs;
    exprs.push_back(parse_expression());

    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        exprs.push_back(parse_expression());
    }
    return exprs;
}

auto Parser::parse_table_name() -> std::string {
    if (current_.type != TokenType::Identifier) {
        throw common::Exception{
            "Parser: expected table name",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    std::string name = current_.value;
    consume();
    return name;
}

auto Parser::parse_name_or_keyword() -> std::string {
    if (current_.type == TokenType::IntegerLiteral ||
        current_.type == TokenType::FloatLiteral) {
        std::string name = current_.value;
        consume();
        return name;
    }
    if (current_.type == TokenType::Identifier ||
        current_.type == TokenType::KeywordType ||
        current_.type == TokenType::KeywordRole ||
        current_.type == TokenType::KeywordCompute ||
        current_.type == TokenType::KeywordStorage ||
        current_.type == TokenType::KeywordHybrid ||
        current_.type == TokenType::KeywordGpu ||
        current_.type == TokenType::KeywordWorker ||
        current_.type == TokenType::KeywordCoordinator ||
        current_.type == TokenType::KeywordObserver ||
        current_.type == TokenType::KeywordCluster ||
        current_.type == TokenType::KeywordReplicas ||
        current_.type == TokenType::KeywordShards ||
        current_.type == TokenType::KeywordReplica ||
        current_.type == TokenType::KeywordQuorum ||
        current_.type == TokenType::KeywordSynchronous ||
        current_.type == TokenType::KeywordAsynchronous ||
        current_.type == TokenType::KeywordConsistency ||
        current_.type == TokenType::KeywordPlacement ||
        current_.type == TokenType::KeywordNodeAware ||
        current_.type == TokenType::KeywordSchema ||
        current_.type == TokenType::KeywordAuth ||
        current_.type == TokenType::KeywordHost ||
        current_.type == TokenType::KeywordPort ||
        current_.type == TokenType::KeywordDatabase ||
        current_.type == TokenType::KeywordBucket ||
        current_.type == TokenType::KeywordEndpoint ||
        current_.type == TokenType::KeywordRegion ||
        current_.type == TokenType::KeywordPath ||
        current_.type == TokenType::KeywordOwner ||
        current_.type == TokenType::KeywordBuiltin) {
        std::string name = current_.value;
        consume();
        return name;
    }
    throw common::Exception{
        "Parser: expected name",
        static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
}

auto Parser::parse_table_ref() -> std::pair<std::string, std::string> {
    std::string table = parse_table_name();
    std::string alias;
    if (current_.type == TokenType::Identifier) {
        alias = current_.value;
        consume();
    }
    return {table, alias};
}

auto Parser::parse_subquery() -> std::shared_ptr<QueryAST> {
    auto sub = std::make_unique<QueryAST>();
    sub->query_type = QueryAST::QueryType::SELECT;
    parse_select(sub);
    return sub;
}

void Parser::parse_window_spec(ASTFunction::WindowSpec& spec) {
    if (current_.type == TokenType::KeywordPartition) {
        consume();
        if (current_.type != TokenType::KeywordBy) {
            throw common::Exception{
                "Parser: expected BY after PARTITION",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume();
        spec.partition_by = parse_expression_list();
    }
    if (current_.type == TokenType::KeywordOrder) {
        consume();
        if (current_.type != TokenType::KeywordBy) {
            throw common::Exception{
                "Parser: expected BY after ORDER",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume();
        auto orders = parse_order_by_list();
        for (auto& ob : orders) {
            spec.order_by.emplace_back(ob.column,
                ob.direction == OrderBy::Direction::DESC);
        }
    }
}

auto Parser::parse_column_list() -> std::vector<std::string> {
    if (current_.type != TokenType::LParen) {
        return {};
    }
    consume(); // consume (
    std::vector<std::string> cols;
    cols.push_back(current_.value);
    consume();
    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        cols.push_back(current_.value);
        consume();
    }
    if (current_.type != TokenType::RParen) {
        throw common::Exception{
            "Parser: expected )",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume )
    return cols;
}

auto Parser::parse_column_definitions() -> std::vector<ColumnDef> {
    if (current_.type != TokenType::LParen) {
        throw common::Exception{
            "Parser: expected (",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume (
    std::vector<ColumnDef> defs;
    while (current_.type != TokenType::RParen && current_.type != TokenType::EndOfQuery) {
        ColumnDef def;
        def.name = current_.value;
        consume();
        if (current_.type == TokenType::Identifier) {
            def.data_type = current_.value;
            consume();
        }
        defs.push_back(std::move(def));
        if (current_.type == TokenType::Comma) {
            consume(); // consume ,
        }
    }
    if (current_.type == TokenType::RParen) {
        consume(); // consume )
    }
    return defs;
}

auto Parser::parse_value_list() -> std::vector<std::vector<std::string>> {
    std::vector<std::vector<std::string>> values;
    values.push_back(parse_value_row());

    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        values.push_back(parse_value_row());
    }
    return values;
}

auto Parser::parse_value_row() -> std::vector<std::string> {
    if (current_.type != TokenType::LParen) {
        throw common::Exception{
            "Parser: expected (",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume (
    std::vector<std::string> row;
    row.push_back(current_.value);
    consume();
    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        row.push_back(current_.value);
        consume();
    }
    if (current_.type != TokenType::RParen) {
        throw common::Exception{
            "Parser: expected )",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // consume )
    return row;
}

auto Parser::parse_order_by_list() -> std::vector<OrderBy> {
    std::vector<OrderBy> orders;
    orders.push_back(parse_order_by());

    while (current_.type == TokenType::Comma) {
        consume(); // consume ,
        orders.push_back(parse_order_by());
    }
    return orders;
}

auto Parser::parse_order_by() -> OrderBy {
    OrderBy order;
    order.column = current_.value;
    consume();
    if (current_.type == TokenType::KeywordAsc || current_.type == TokenType::KeywordDesc) {
        order.direction = current_.type == TokenType::KeywordAsc ?
                  OrderBy::Direction::ASC : OrderBy::Direction::DESC;
        consume();
    }
    return order;
}

auto Parser::parse_limit() -> std::pair<size_t, size_t> {
    size_t count = static_cast<size_t>(std::stoll(current_.value));
    consume();
    size_t offset = 0;
    if (current_.type == TokenType::Comma) {
        consume(); // consume ,
        offset = static_cast<size_t>(std::stoll(current_.value));
        consume();
    }
    return {offset, count};
}

auto Parser::consume_node_keyword() -> bool {
    if (current_.type == TokenType::KeywordNode) {
        consume();
        return true;
    }
    return false;
}

void Parser::parse_node_properties(QueryAST::Create& create) {
    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        if (current_.type == TokenType::KeywordType) {
            consume();
            create.node_type = parse_name_or_keyword();
        } else if (current_.type == TokenType::KeywordRole) {
            consume();
            create.node_role = parse_name_or_keyword();
        } else {
            break;
        }
    }
}

auto Parser::consume_storage_unit_keyword() -> bool {
    if (current_.type == TokenType::Identifier && current_.value == "STORAGE_UNIT") {
        consume();
        return true;
    }
    if (current_.type == TokenType::KeywordStorage) {
        consume();
        if (current_.type != TokenType::KeywordUnit) {
            throw common::Exception{
                "Parser: expected UNIT after STORAGE",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume();
        return true;
    }
    return false;
}

auto Parser::parse_property_value() -> std::string {
    if (current_.type == TokenType::StringLiteral) {
        auto value = current_.value;
        consume();
        return value;
    }
    if (current_.type == TokenType::IntegerLiteral ||
        current_.type == TokenType::FloatLiteral) {
        auto value = current_.value;
        consume();
        return value;
    }
    return parse_table_name();
}

auto Parser::consume_replica_group_keyword() -> bool {
    if (current_.type == TokenType::Identifier && current_.value == "REPLICA_GROUP") {
        consume();
        return true;
    }
    if (current_.type == TokenType::KeywordReplicaGroup) {
        consume();
        return true;
    }
    return false;
}

void Parser::parse_replica_group_properties(QueryAST::Create& create) {
    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        if (current_.type == TokenType::KeywordReplicas) {
            consume();
            if (current_.type == TokenType::IntegerLiteral) {
                create.replica_count = static_cast<uint32_t>(std::stoul(current_.value));
                consume();
            } else {
                create.replica_count = static_cast<uint32_t>(std::stoul(parse_table_name()));
            }
        } else if (current_.type == TokenType::KeywordConsistency) {
            consume();
            create.consistency_mode = parse_name_or_keyword();
        } else if (current_.type == TokenType::KeywordType ||
                   (current_.type == TokenType::Identifier && current_.value == "STRATEGY")) {
            if (current_.type == TokenType::KeywordType) {
                consume();
            } else {
                consume();
            }
            create.replica_strategy = parse_name_or_keyword();
        } else if (current_.type == TokenType::KeywordPlacement) {
            consume();
            create.placement_policy = parse_name_or_keyword();
        } else {
            break;
        }
    }
}

auto Parser::consume_shard_group_keyword() -> bool {
    if (current_.type == TokenType::Identifier && current_.value == "SHARD_GROUP") {
        consume();
        return true;
    }
    if (current_.type == TokenType::KeywordShardGroup) {
        consume();
        return true;
    }
    return false;
}

void Parser::parse_shard_group_properties(QueryAST::Create& create) {
    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        if (current_.type == TokenType::KeywordType) {
            consume();
            create.shard_type = parse_name_or_keyword();
        } else if (current_.type == TokenType::KeywordKey ||
                   (current_.type == TokenType::Identifier && current_.value == "KEY")) {
            consume();
            create.shard_key = parse_table_name();
        } else if (current_.type == TokenType::KeywordShards ||
                   (current_.type == TokenType::Identifier && current_.value == "SHARDS")) {
            consume();
            if (current_.type == TokenType::IntegerLiteral) {
                create.shard_count = static_cast<uint32_t>(std::stoul(current_.value));
                consume();
            } else {
                create.shard_count = static_cast<uint32_t>(std::stoul(parse_table_name()));
            }
        } else {
            break;
        }
    }
}

void Parser::parse_run(std::unique_ptr<QueryAST>& ast) {
    consume(); // RUN
    if (current_.type == TokenType::Identifier && current_.value == "TRAINING_JOB") {
        consume();
        ast->model_control.action = QueryAST::ModelControl::Action::RunTrainingJob;
        ast->model_control.target_name = parse_table_name();
        return;
    }
    if (current_.type == TokenType::Identifier && current_.value == "TUNING_JOB") {
        consume();
        ast->model_control.action = QueryAST::ModelControl::Action::RunTuningJob;
        ast->model_control.target_name = parse_table_name();
        return;
    }
    if (!consume_pipeline_keyword()) {
        throw common::Exception{
            "Parser: expected PIPELINE after RUN",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    ast->pipeline_control.pipeline_name = parse_table_name();
}

void Parser::parse_pause(std::unique_ptr<QueryAST>& ast) {
    consume(); // PAUSE
    if (!consume_pipeline_keyword()) {
        throw common::Exception{
            "Parser: expected PIPELINE after PAUSE",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    ast->pipeline_control.pipeline_name = parse_table_name();
}

void Parser::parse_resume(std::unique_ptr<QueryAST>& ast) {
    consume(); // RESUME
    if (!consume_pipeline_keyword()) {
        throw common::Exception{
            "Parser: expected PIPELINE after RESUME",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    ast->pipeline_control.pipeline_name = parse_table_name();
}

auto Parser::consume_pipeline_keyword() -> bool {
    if (current_.type == TokenType::KeywordPipeline) {
        consume();
        return true;
    }
    if (current_.type == TokenType::Identifier && current_.value == "PIPELINE") {
        consume();
        return true;
    }
    return false;
}

auto Parser::consume_stage_keyword() -> bool {
    if (current_.type == TokenType::KeywordStage) {
        consume();
        return true;
    }
    if (current_.type == TokenType::Identifier && current_.value == "STAGE") {
        consume();
        return true;
    }
    return false;
}

auto Parser::consume_task_keyword() -> bool {
    if (current_.type == TokenType::KeywordTask) {
        consume();
        return true;
    }
    if (current_.type == TokenType::Identifier && current_.value == "TASK") {
        consume();
        return true;
    }
    return false;
}

auto Parser::consume_trigger_keyword() -> bool {
    if (current_.type == TokenType::KeywordTrigger) {
        consume();
        return true;
    }
    if (current_.type == TokenType::Identifier && current_.value == "TRIGGER") {
        consume();
        return true;
    }
    return false;
}

void Parser::parse_pipeline_owner(QueryAST::Create& create) {
    if (current_.type == TokenType::KeywordOwner) {
        consume();
        create.pipeline_owner = parse_property_value();
    }
}

void Parser::parse_stage_order(QueryAST::Create& create) {
    if (current_.type == TokenType::KeywordOrder) {
        consume();
        if (current_.type == TokenType::IntegerLiteral) {
            create.stage_order = static_cast<uint32_t>(std::stoul(current_.value));
            consume();
        } else {
            create.stage_order = static_cast<uint32_t>(std::stoul(parse_table_name()));
        }
    }
}

void Parser::parse_task_properties(QueryAST::Create& create) {
    if (current_.type != TokenType::KeywordType) {
        throw common::Exception{
            "Parser: expected TYPE after task pipeline clause",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // TYPE
    create.task_type = parse_name_or_keyword();
    if (current_.type == TokenType::KeywordBody) {
        consume();
        create.task_body = parse_property_value();
    }
    if (current_.type == TokenType::KeywordDepends) {
        consume();
        if (current_.type != TokenType::KeywordOn) {
            throw common::Exception{
                "Parser: expected ON after DEPENDS",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        consume();
        do {
            create.task_depends_on.push_back(parse_table_name());
            if (current_.type == TokenType::Comma) {
                consume();
            } else {
                break;
            }
        } while (true);
    }
}

void Parser::parse_trigger_properties(QueryAST::Create& create) {
    if (current_.type == TokenType::KeywordSchedule) {
        consume();
        create.trigger_schedule = parse_property_value();
    }
}

auto Parser::consume_connector_keyword() -> bool {
    if (current_.type == TokenType::KeywordConnector) {
        consume();
        return true;
    }
    if (current_.type == TokenType::Identifier && current_.value == "CONNECTOR") {
        consume();
        return true;
    }
    return false;
}

void Parser::parse_connector_properties(QueryAST::Create& create) {
    if (current_.type != TokenType::KeywordType) {
        throw common::Exception{
            "Parser: expected TYPE after connector name",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // TYPE
    create.connector_type = parse_table_name();

    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        if (current_.type == TokenType::KeywordAuth) {
            consume();
            create.auth_method = parse_name_or_keyword();
            continue;
        }
        std::string key;
        if (current_.type == TokenType::KeywordPath) {
            key = "PATH";
            consume();
        } else if (current_.type == TokenType::KeywordBucket) {
            key = "BUCKET";
            consume();
        } else if (current_.type == TokenType::KeywordEndpoint) {
            key = "ENDPOINT";
            consume();
        } else if (current_.type == TokenType::KeywordRegion) {
            key = "REGION";
            consume();
        } else if (current_.type == TokenType::KeywordHost) {
            key = "HOST";
            consume();
        } else if (current_.type == TokenType::KeywordPort) {
            key = "PORT";
            consume();
        } else if (current_.type == TokenType::KeywordDatabase) {
            key = "DATABASE";
            consume();
        } else if (current_.type == TokenType::KeywordSchema) {
            key = "SCHEMA";
            consume();
        } else if (current_.type == TokenType::Identifier) {
            key = current_.value;
            consume();
        } else {
            break;
        }
        create.connector_properties[key] = parse_property_value();
    }
}

void Parser::parse_storage_unit_properties(QueryAST::Create& create) {
    if (current_.type != TokenType::KeywordType) {
        throw common::Exception{
            "Parser: expected TYPE after storage unit name",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    consume(); // TYPE
    create.storage_unit_type = parse_table_name();

    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        std::string key;
        if (current_.type == TokenType::KeywordPath) {
            key = "PATH";
            consume();
        } else if (current_.type == TokenType::KeywordBucket) {
            key = "BUCKET";
            consume();
        } else if (current_.type == TokenType::KeywordEndpoint) {
            key = "ENDPOINT";
            consume();
        } else if (current_.type == TokenType::KeywordRegion) {
            key = "REGION";
            consume();
        } else if (current_.type == TokenType::Identifier) {
            key = current_.value;
            consume();
        } else {
            break;
        }
        create.storage_properties[key] = parse_property_value();
    }
}

void Parser::parse_publish(std::unique_ptr<QueryAST>& ast) {
    consume(); // PUBLISH
    ast->stream_control.topic_name = parse_table_name();
    if (current_.type == TokenType::KeywordValues) {
        consume();
        ast->stream_control.values = parse_value_list();
    }
}

void Parser::parse_subscribe(std::unique_ptr<QueryAST>& ast) {
    consume(); // SUBSCRIBE
    ast->stream_control.stream_name = parse_table_name();
    if (consume_consumer_group_keyword()) {
        ast->stream_control.consumer_group_name = parse_table_name();
    }
    if (current_.type == TokenType::KeywordLimit) {
        consume();
        if (current_.type == TokenType::IntegerLiteral) {
            ast->stream_control.limit = static_cast<uint32_t>(std::stoul(current_.value));
            consume();
        } else {
            ast->stream_control.limit = static_cast<uint32_t>(std::stoul(parse_table_name()));
        }
    }
}

auto Parser::consume_stream_keyword() -> bool {
    if (current_.type == TokenType::KeywordStream) {
        consume();
        return true;
    }
    if (current_.type == TokenType::Identifier && current_.value == "STREAM") {
        consume();
        return true;
    }
    return false;
}

auto Parser::consume_topic_keyword() -> bool {
    if (current_.type == TokenType::KeywordTopic) {
        consume();
        return true;
    }
    if (current_.type == TokenType::Identifier && current_.value == "TOPIC") {
        consume();
        return true;
    }
    return false;
}

auto Parser::consume_consumer_group_keyword() -> bool {
    if (current_.type == TokenType::KeywordConsumerGroup) {
        consume();
        return true;
    }
    if (current_.type == TokenType::Identifier && current_.value == "CONSUMER_GROUP") {
        consume();
        return true;
    }
    return false;
}

void Parser::parse_retention_clause(QueryAST::Create& create) {
    if (current_.type != TokenType::KeywordRetain) {
        return;
    }
    consume(); // RETAIN
    if (current_.type == TokenType::KeywordForever ||
        (current_.type == TokenType::Identifier && current_.value == "FOREVER")) {
        consume();
        create.retention_forever = true;
        return;
    }
    if (current_.type == TokenType::IntegerLiteral) {
        create.retention_days = static_cast<uint32_t>(std::stoul(current_.value));
        consume();
    } else {
        create.retention_days = static_cast<uint32_t>(std::stoul(parse_table_name()));
    }
    if (current_.type == TokenType::KeywordDays ||
        (current_.type == TokenType::Identifier && current_.value == "DAYS")) {
        consume();
    }
    create.retention_forever = false;
}

void Parser::parse_topic_properties(QueryAST::Create& create) {
    if (current_.type == TokenType::KeywordPartitions) {
        consume();
        if (current_.type == TokenType::IntegerLiteral) {
            create.partition_count = static_cast<uint32_t>(std::stoul(current_.value));
            consume();
        } else {
            create.partition_count = static_cast<uint32_t>(std::stoul(parse_table_name()));
        }
    }
}

void Parser::consume() {
    current_ = lexer_.next();
}

bool Parser::is_current(TokenType type) const {
    return current_.type == type;
}

// ── MODEL layer parsing ──

void Parser::parse_kv_list(std::unordered_map<std::string, std::string>& out) {
    if (current_.type != TokenType::LParen) return;
    consume(); // (
    while (current_.type != TokenType::RParen &&
           current_.type != TokenType::EndOfQuery) {
        std::string key = current_.value;
        consume();
        if (current_.type == TokenType::Eq) {
            consume();
        } else if (current_.type == TokenType::Colon) {
            consume();
        }
        std::string value = parse_property_value();
        out[key] = value;
        if (current_.type == TokenType::Comma) { consume(); continue; }
        break;
    }
    if (current_.type == TokenType::RParen) consume();
}

void Parser::parse_paren_kv_pairs(std::vector<std::pair<std::string, std::string>>& out) {
    if (current_.type != TokenType::LParen) return;
    consume(); // (
    while (current_.type != TokenType::RParen &&
           current_.type != TokenType::EndOfQuery) {
        std::string key = current_.value;
        consume();
        if (current_.type == TokenType::Eq) consume();
        std::string value = parse_property_value();
        out.emplace_back(key, value);
        if (current_.type == TokenType::Comma) { consume(); continue; }
        break;
    }
    if (current_.type == TokenType::RParen) consume();
}

void Parser::parse_model_clauses(QueryAST::Create& create) {
    auto read_word = [&]() -> std::string {
        std::string v = current_.value;
        consume();
        return v;
    };
    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        const std::string kw = upper_str(current_.value);
        if (kw == "TYPE") {
            consume(); create.model_type = read_word();
        } else if (kw == "FRAMEWORK") {
            consume(); create.framework = read_word();
        } else if (kw == "ALGORITHM") {
            consume(); create.algorithm = read_word();
        } else if (kw == "ENTRYPOINT") {
            consume(); create.entrypoint = parse_property_value();
        } else if (kw == "OBJECTIVE") {
            consume(); create.objective = read_word();
        } else if (kw == "STRATEGY") {
            consume(); create.strategy = read_word();
        } else if (kw == "TRIALS") {
            consume();
            if (current_.type == TokenType::IntegerLiteral) {
                create.trials = static_cast<uint32_t>(std::stoul(current_.value));
                consume();
            } else {
                create.trials = static_cast<uint32_t>(std::stoul(read_word()));
            }
        } else if (kw == "MODEL") {
            consume(); create.ref_model = parse_table_name();
        } else if (kw == "FEATURE_SET") {
            consume(); create.ref_feature_set = parse_table_name();
        } else if (kw == "DATASET") {
            consume(); create.ref_dataset = parse_table_name();
        } else if (kw == "TRAINING_JOB") {
            consume(); create.ref_training_job = parse_table_name();
        } else if (kw == "FROM") {
            consume(); create.source_table = parse_table_name();
        } else if (kw == "HYPERPARAMS") {
            consume(); parse_kv_list(create.hyperparams);
        } else if (kw == "SEARCH_SPACE") {
            consume(); parse_kv_list(create.search_space);
        } else {
            break;
        }
    }
}

void Parser::parse_feature_set_clauses(QueryAST::Create& create) {
    auto read_word = [&]() -> std::string {
        std::string v = current_.value;
        consume();
        return v;
    };
    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        const std::string kw = upper_str(current_.value);
        if (kw == "FROM") {
            consume(); create.source_table = parse_table_name();
        } else if (kw == "ENTITY_KEY") {
            consume();
            auto cols = parse_column_list();
            if (!cols.empty()) create.entity_key = cols.front();
        } else if (kw == "FEATURES") {
            consume();
            create.features = parse_column_list();
        } else if (kw == "TARGET") {
            consume();
            create.target = read_word();
        } else {
            break;
        }
    }
}

void Parser::parse_model_version_ref(std::string& model, uint32_t& version) {
    model = parse_table_name();
    version = 0;
    if (current_.type == TokenType::Colon) {
        consume(); // :
        if (current_.type == TokenType::IntegerLiteral) {
            version = static_cast<uint32_t>(std::stoul(current_.value));
            consume();
        } else {
            // token like "v3"
            std::string v = current_.value;
            consume();
            std::string digits;
            for (char c : v) if (std::isdigit(static_cast<unsigned char>(c))) digits += c;
            if (!digits.empty()) version = static_cast<uint32_t>(std::stoul(digits));
        }
    }
}

void Parser::parse_deploy(std::unique_ptr<QueryAST>& ast) {
    auto& mc = ast->model_control;
    consume(); // DEPLOY
    if (upper_str(current_.value) == "MODEL") consume();
    mc.action = QueryAST::ModelControl::Action::Deploy;
    parse_model_version_ref(mc.model_name, mc.version);
    if (current_.type == TokenType::KeywordAs) {
        consume();
        mc.endpoint_name = parse_table_name();
    }
}

void Parser::parse_predict(std::unique_ptr<QueryAST>& ast) {
    auto& mc = ast->model_control;
    consume(); // PREDICT
    if (upper_str(current_.value) == "MODEL") consume();
    mc.action = QueryAST::ModelControl::Action::Predict;
    parse_model_version_ref(mc.model_name, mc.version);
    if (current_.type == TokenType::KeywordFor) {
        consume();
        parse_paren_kv_pairs(mc.kv);
        mc.predict_mode = "entity";
    } else if (current_.type == TokenType::KeywordWith) {
        consume();
        parse_paren_kv_pairs(mc.kv);
        mc.predict_mode = "features";
    } else if (current_.type == TokenType::KeywordFrom) {
        consume();
        mc.from_table = parse_table_name();
        mc.predict_mode = "batch";
    }
}

void Parser::parse_evaluate(std::unique_ptr<QueryAST>& ast) {
    auto& mc = ast->model_control;
    consume(); // EVALUATE
    if (upper_str(current_.value) == "MODEL") consume();
    mc.action = QueryAST::ModelControl::Action::Evaluate;
    parse_model_version_ref(mc.model_name, mc.version);
}

void Parser::parse_compare(std::unique_ptr<QueryAST>& ast) {
    auto& mc = ast->model_control;
    consume(); // COMPARE
    const std::string kw = upper_str(current_.value);
    if (kw == "MODELS" || kw == "MODEL") consume();
    mc.action = QueryAST::ModelControl::Action::Compare;
    while (current_.type != TokenType::EndOfQuery &&
           current_.type != TokenType::Semicolon) {
        std::string model;
        uint32_t version = 0;
        parse_model_version_ref(model, version);
        mc.compare_targets.emplace_back(model, version);
        if (current_.type == TokenType::Comma) { consume(); continue; }
        break;
    }
}

void Parser::parse_generate(std::unique_ptr<QueryAST>& ast) {
    auto& mc = ast->model_control;
    consume(); // GENERATE
    mc.action = QueryAST::ModelControl::Action::Generate;
    if (upper_str(current_.value) == "USING") consume();
    if (upper_str(current_.value) == "MODEL") consume();
    mc.model_name = parse_table_name();
    if (upper_str(current_.value) == "PROMPT") {
        consume();
        mc.prompt = parse_property_value();
    }
}

// ── QueryParser — concrete SQL query parser ──

auto QueryParser::parse() -> std::unique_ptr<QueryAST> {
    return Parser::parse();  // calls base's parse_query() → std::unique_ptr<QueryAST>
}

} // namespace mnemo::parsers
