// src/Analyzer/query_tree.h — IQueryTreeNode representation after analysis
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <optional>
#include "Parsers/ast.h"

namespace mnemo::analyzer {

// ── IQueryTreeNode — intermediate representation after analysis ──
// This is the output of the analyzer and input to the planner.
class IQueryTreeNode {
public:
    virtual ~IQueryTreeNode() = default;
    virtual auto node_type() const -> std::string = 0;
};

// ── Node types in the query tree ──
class SelectNode final : public IQueryTreeNode {
public:
    struct ColumnExpr {
        std::shared_ptr<IQueryTreeNode> expression;
        std::string                      alias;
        datatypes::DataTypePtr             result_type;
    };

    std::vector<ColumnExpr> columns;
    std::shared_ptr<IQueryTreeNode> from;
    std::shared_ptr<IQueryTreeNode> where;
    std::vector<std::shared_ptr<IQueryTreeNode>> group_by;
    std::shared_ptr<IQueryTreeNode> having;
    std::vector<std::pair<std::shared_ptr<IQueryTreeNode>, bool>> order_by;
    std::pair<size_t, std::optional<size_t>> limit;

    [[nodiscard]] auto node_type() const -> std::string override { return "Select"; }
};

class TableNode final : public IQueryTreeNode {
public:
    std::string database;
    std::string table;
    std::vector<std::string> columns; // Column names for CREATE TABLE

    [[nodiscard]] auto node_type() const -> std::string override { return "Table"; }
};

class DDLNode final : public IQueryTreeNode {
public:
    enum class Kind {
        CreateDatabase,
        CreateTable,
        Insert,
        Drop,
        Truncate,
        Detach,
        Alter,
        ShowDatabases,
        ShowTables,
        ShowViews,
        ShowMaterializedViews,
        ShowStorageUnits,
        ShowStorageUsage,
        ShowNodes,
        ShowNodeMetrics,
        ShowNodeCapabilities,
        ShowNodePartitions,
        ShowNodeReplicas,
        ShowClusters,
        ShowReplicaGroups,
        ShowReplicationStatus,
        ShowShardGroups,
        ShowShards,
        ShowShardStatus,
        ShowConnectors,
        ShowConnectorCapabilities,
        ShowConnectorStatus,
        ShowPipelines,
        ShowStages,
        ShowTasks,
        ShowTriggers,
        ShowPipelineRuns,
        ShowPipelineMetrics,
        ShowStreams,
        ShowTopics,
        ShowConsumerGroups,
        ShowStreamMetrics,
        ShowModels,
        ShowModelVersions,
        ShowModelEndpoints,
        ShowModelMetrics,
        ShowModelDrift,
        ShowFeatureSets,
        ShowDatasets,
        ShowTrainingJobs,
        ShowTuningJobs,
        ShowModelTemplates,
        Describe,
        Explain,
        Use,
        Refresh,
        RunPipeline,
        PausePipeline,
        ResumePipeline,
    };

    Kind kind = Kind::CreateTable;
    std::string database;
    std::string table;
    bool if_not_exists = false;
    bool if_exists     = false;
    std::string engine = "Memory";
    std::vector<parsers::ColumnDef> column_defs;
    std::vector<std::string> insert_columns;
    std::vector<std::vector<std::string>> insert_values;
    std::vector<parsers::ASTAlterQuery::AlterCommand> alter_commands;
    std::string use_database;
    parsers::QueryAST::ObjectKind describe_kind = parsers::QueryAST::ObjectKind::Table;
    std::string refresh_name;
    std::string show_node_name;
    std::string pipeline_name;
    std::string stage_name;
    std::string stream_name;
    std::string model_name;          // MODEL layer SHOW/DESCRIBE filter
    uint32_t    model_version = 0;   // DESCRIBE MODEL VERSION m:vN

    [[nodiscard]] auto node_type() const -> std::string override { return "DDL"; }
};

class JoinNode final : public IQueryTreeNode {
public:
    std::string join_type; // INNER, LEFT, RIGHT, FULL, CROSS
    std::shared_ptr<IQueryTreeNode> left;
    std::shared_ptr<IQueryTreeNode> right;
    std::shared_ptr<IQueryTreeNode> condition;

    [[nodiscard]] auto node_type() const -> std::string override { return "Join"; }
};

class AggregateNode final : public IQueryTreeNode {
public:
    struct AggregateExpr {
        std::shared_ptr<IQueryTreeNode> function;
        std::string                      alias;
        bool                             is_distinct;
    };
    std::vector<AggregateExpr> aggregates;
    std::vector<std::shared_ptr<IQueryTreeNode>> group_by;
    std::shared_ptr<IQueryTreeNode> child;

    [[nodiscard]] auto node_type() const -> std::string override { return "Aggregate"; }
};

class FilterNode final : public IQueryTreeNode {
public:
    std::shared_ptr<IQueryTreeNode> condition;
    std::shared_ptr<IQueryTreeNode> child;

    [[nodiscard]] auto node_type() const -> std::string override { return "Filter"; }
};

class SortNode final : public IQueryTreeNode {
public:
    struct SortKey {
        std::shared_ptr<IQueryTreeNode> expression;
        bool   ascending;
        bool   null_first;
    };
    std::vector<SortKey> keys;
    std::shared_ptr<IQueryTreeNode> child;

    [[nodiscard]] auto node_type() const -> std::string override { return "Sort"; }
};

class LimitNode final : public IQueryTreeNode {
public:
    size_t count;
    std::optional<size_t> offset;
    std::shared_ptr<IQueryTreeNode> child;

    [[nodiscard]] auto node_type() const -> std::string override { return "Limit"; }
};

} // namespace mnemo::analyzer
