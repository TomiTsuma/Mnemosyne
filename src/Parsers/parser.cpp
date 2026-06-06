// src/Parsers/parser.cpp — SQL parser for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Parsers/parser.h"
#include "Parsers/parser_query.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace mnemo::parsers {

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
        auto [table, alias] = parse_table_ref();
        select.table = std::move(table);
        select.table_alias = std::move(alias);

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
    if (current_.type != TokenType::KeywordTable) {
        throw common::Exception{
            "Parser: expected TABLE or NODE after ALTER",
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
    if (current_.type == TokenType::KeywordNode) {
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
        } else if (current_.type == TokenType::KeywordPartition) {
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
    if (consume_storage_unit_keyword()) {
        describe.object_kind = QueryAST::ObjectKind::StorageUnit;
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
        current_.type == TokenType::KeywordCluster) {
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
    return parse_table_name();
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

void Parser::consume() {
    current_ = lexer_.next();
}

bool Parser::is_current(TokenType type) const {
    return current_.type == type;
}

// ── QueryParser — concrete SQL query parser ──

auto QueryParser::parse() -> std::unique_ptr<QueryAST> {
    return Parser::parse();  // calls base's parse_query() → std::unique_ptr<QueryAST>
}

} // namespace mnemo::parsers
