// src/Parsers/ast.h — AST node types for Mnemosyne SQL
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <unordered_map>
#include <cstdint>

namespace mnemo::parsers {

// ── Forward declarations ──
class IASTVisitor;
class ASTUseDatabase;
class ASTSelectQuery;
class ASTCreateTable;
class ASTInsertQuery;
class ASTExpr;
class ASTLiteral;
class ASTColumnRef;
class ASTFunction;
class QueryAST;
class ASTBinaryOp;
class ASTUnaryOp;
class ASTAlias;
class ASTSubQuery;
class QueryAST;
class ASTFromClause;
class ColumnDef;

// ── ASTExpr — base for all expression nodes ──
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(const IASTVisitor& visitor) = 0;
    [[nodiscard]] virtual auto to_string() const -> std::string = 0;
};

class IASTVisitor {
public:
    virtual ~IASTVisitor() = default;
    virtual void visit(ASTUseDatabase& n)     = 0;
    virtual void visit(ASTSelectQuery& n)     = 0;
    virtual void visit(ASTCreateTable& n)     = 0;
    virtual void visit(ASTInsertQuery& n)     = 0;
    virtual void visit(ASTExpr& n)            = 0;
    virtual void visit(ASTLiteral& n)         = 0;
    virtual void visit(ASTColumnRef& n)       = 0;
    virtual void visit(ASTFunction& n)        = 0;
    virtual void visit(ASTBinaryOp& n)        = 0;
    virtual void visit(ASTUnaryOp& n)         = 0;
    virtual void visit(ASTAlias& n)           = 0;
    virtual void visit(ASTSubQuery& n)        = 0;
    virtual void visit(QueryAST& n)           = 0;
};

// ── ASTUseDatabase ──
class ASTUseDatabase final : public ASTNode {
public:
    std::string database_name;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTUseDatabase"; }
};

// ── ASTSelectQuery ──
class ASTSelectQuery final : public ASTNode {
public:
    std::vector<std::pair<std::shared_ptr<ASTExpr>, std::optional<std::string>>> select_list;
    std::shared_ptr<ASTFromClause>                from;
    std::shared_ptr<ASTExpr>                      where;
    std::vector<std::shared_ptr<ASTExpr>>         group_by;
    std::shared_ptr<ASTExpr>                      having;
    std::vector<std::pair<std::shared_ptr<ASTExpr>, bool>> order_by;
    std::pair<size_t, std::optional<size_t>>      limit;
    std::optional<std::shared_ptr<ASTSelectQuery>>  left;
    std::optional<std::shared_ptr<ASTSelectQuery>>  right;
    std::string                                   join_type;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTSelectQuery"; }
};

// ── ASTDDLQuery — shared fields for DDL statements ──
struct ASTDDLQuery {
    std::string database;
    std::string table;
    bool if_not_exists = false;
    bool if_exists     = false;
};

// ── ASTCreateTable / ASTCreateQuery ──
class ASTCreateTable final : public ASTNode, public ASTDDLQuery {
public:
    struct ColumnDef {
        std::string name;
        std::string type_name;
        bool        nullable = false;
        std::optional<std::string> comment;
    };
    std::vector<ColumnDef> columns;
    std::string engine = "Memory";
    std::string order_by;
    std::string primary_key;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTCreateTable"; }
};

// ── ASTDropQuery ──
class ASTDropQuery final : public ASTNode, public ASTDDLQuery {
public:
    enum class Kind { Drop, Detach, Truncate };
    Kind kind = Kind::Drop;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTDropQuery"; }
};

// ── ASTAlterQuery ──
class ASTAlterQuery final : public ASTNode, public ASTDDLQuery {
public:
    struct AlterCommand {
        enum class Type { ADD_COLUMN, DROP_COLUMN, MODIFY_COLUMN };
        Type type = Type::ADD_COLUMN;
        std::string column_name;
        std::string column_type;
    };
    std::vector<AlterCommand> commands;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTAlterQuery"; }
};

// ── ASTInsertQuery ──
class ASTInsertQuery final : public ASTNode {
public:
    std::string database;
    std::string table;
    std::vector<std::string> columns;
    std::shared_ptr<ASTSelectQuery> data_query;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTInsertQuery"; }
};

// ── ASTExpr — base for expressions ──
class ASTExpr : public ASTNode {
public:
    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] virtual std::string to_string() const { return "ASTExpr"; }
};

// ── ASTLiteral ──
class ASTLiteral : public ASTExpr {
public:
    std::variant<int64_t, double, std::string, bool, std::monostate> value;
    bool is_null() const { return std::holds_alternative<std::monostate>(value); }

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTColumnRef ──
class ASTColumnRef : public ASTExpr {
public:
    std::string table;
    std::string column;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTFunction ──
class ASTFunction : public ASTExpr {
public:
    struct WindowSpec {
        std::vector<std::shared_ptr<ASTExpr>> partition_by;
        std::vector<std::pair<std::string, bool>> order_by;
    };

    std::string    name;
    std::vector<std::shared_ptr<ASTExpr>> args;
    bool            is_aggregate = false;
    std::vector<std::shared_ptr<ASTExpr>> distinct_args;
    std::optional<WindowSpec> window;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTBinaryOp ──
class ASTBinaryOp : public ASTExpr {
public:
    enum class Op { Add, Sub, Mul, Div, Eq, Ne, Gt, Lt, Ge, Le, And, Or, Like, NotLike, In };
    Op       op;
    std::shared_ptr<ASTExpr> left;
    std::shared_ptr<ASTExpr> right;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTUnaryOp ──
class ASTUnaryOp : public ASTExpr {
public:
    enum class Op { Neg, Not, BitwiseNot, Plus };
    Op   op;
    std::shared_ptr<ASTExpr> operand;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTAlias — expression with alias ──
class ASTAlias final : public ASTExpr {
public:
    std::shared_ptr<ASTExpr> expression;
    std::string              alias;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTSubQuery — nested query ──
class ASTSubQuery final : public ASTNode {
public:
    std::shared_ptr<ASTSelectQuery> query;
    std::string alias;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

// ── ASTSubQueryExpr — subquery used as an expression (WHERE IN, scalar) ──
class ASTSubQueryExpr final : public ASTExpr {
public:
    std::shared_ptr<QueryAST> query;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] std::string to_string() const override;
};

inline std::string ASTSubQueryExpr::to_string() const {
    return query ? "(subquery)" : "(null subquery)";
}

// ── ASTFromClause ──
class ASTFromClause final : public ASTNode {
public:
    struct TableRef {
        std::string table;
        std::string database;
        std::string alias;
        bool        is_global;
    };
    std::vector<TableRef> tables;

    void accept(const IASTVisitor& visitor) override {}
    [[nodiscard]] auto to_string() const -> std::string override { return "ASTFromClause"; }
};

// ── QueryAST — top-level parsed query (inherits ASTNode for polymorphism) ──
class QueryAST final : public ASTNode {
public:
    enum class QueryType {
        SELECT, INSERT, CREATE, DROP, ALTER, SHOW, DESCRIBE, EXPLAIN, USE, REFRESH,
        REGISTER, DRAIN, REMOVE, TEST, DISCOVER, RUN, PAUSE, RESUME, PUBLISH, SUBSCRIBE,
        // MODEL layer control verbs
        DEPLOY, PREDICT, EVALUATE, COMPARE, GENERATE
    };

    enum class ObjectKind {
        Table, View, MaterializedView, StorageUnit, Node, Cluster, ReplicaGroup, ShardGroup,
        Connector, Pipeline, Stage, Task, Trigger, Stream, Topic, ConsumerGroup,
        // MODEL layer entities
        Model, ModelVersion, TrainingJob, TuningJob, ModelTemplate, FeatureSet, Dataset,
        ModelEndpoint
    };
    QueryType query_type = QueryType::SELECT;

    void accept(const IASTVisitor& visitor) override { }
    [[nodiscard]] auto to_string() const -> std::string override {
        return "QueryAST";
    }

    struct Select {
        struct JoinClause {
            std::string join_type = "INNER";
            std::string table;
            std::string alias;
            std::shared_ptr<ASTExpr> on;
        };

        std::vector<std::shared_ptr<ASTExpr>> columns;
        std::string table;
        std::string table_alias;
        bool from_connector = false;
        std::string connector_name;
        std::string connector_resource;
        std::vector<JoinClause> joins;
        std::shared_ptr<ASTExpr> where;
        std::vector<std::shared_ptr<ASTExpr>> group_by;
        std::shared_ptr<ASTExpr> having;
        std::vector<std::pair<std::string, bool>> order_by;
        std::pair<size_t, size_t> limit = {0, 0};
    } select;

    struct Insert {
        std::string table;
        std::vector<std::string> columns;
        std::vector<std::vector<std::string>> values;
    } insert;

    struct Create : ASTDDLQuery {
        enum class Kind {
            Database, Table, View, MaterializedView, StorageUnit, Node, Cluster, ReplicaGroup,
            ShardGroup, Connector, Pipeline, Stage, Task, Trigger, Stream, Topic, ConsumerGroup,
            // MODEL layer entities
            Model, TrainingJob, TuningJob, ModelTemplate, FeatureSet, Dataset
        };
        Kind kind = Kind::Table;

        std::string database_name;
        std::string table_name;
        std::string view_name;
        std::vector<ColumnDef> columns;
        std::string engine = "Memory";
        Select select_definition;
        std::string storage_unit_name;
        std::string storage_unit_type;
        std::unordered_map<std::string, std::string> storage_properties;
        std::string node_name;
        std::string node_type;
        std::string node_role;
        std::string cluster_name;
        std::string replica_group_name;
        uint32_t replica_count = 0;
        std::string consistency_mode;
        std::string replica_strategy;
        std::string placement_policy;
        std::string shard_group_name;
        uint32_t shard_count = 0;
        std::string shard_key;
        std::string shard_type;
        std::string connector_name;
        std::string connector_type;
        std::string auth_method;
        std::unordered_map<std::string, std::string> connector_properties;
        std::string pipeline_name;
        std::string stage_name;
        std::string task_name;
        std::string trigger_name;
        std::string pipeline_owner;
        uint32_t stage_order = 0;
        std::string task_type;
        std::string task_body;
        std::vector<std::string> task_depends_on;
        std::string trigger_schedule;
        std::string stream_name;
        std::string topic_name;
        std::string consumer_group_name;
        uint32_t partition_count = 1;
        uint32_t retention_days = 7;
        bool retention_forever = true;

        // ── MODEL layer ──
        std::string model_name;
        std::string model_type;          // CLASSIFICATION / REGRESSION / ...
        std::string feature_set_name;
        std::string dataset_name;
        std::string training_job_name;
        std::string tuning_job_name;
        std::string model_template_name;
        std::string entity_key;
        std::vector<std::string> features;
        std::string target;
        std::string framework;
        std::string algorithm;
        std::string entrypoint;
        std::string objective;
        std::string strategy;
        uint32_t trials = 0;
        std::string ref_model;            // MODEL <name> clause
        std::string ref_feature_set;      // FEATURE_SET <name> clause
        std::string ref_dataset;          // DATASET <name> clause
        std::string ref_training_job;     // TRAINING_JOB <name> clause
        std::string source_table;         // FROM <table> (feature set / dataset)
        std::unordered_map<std::string, std::string> hyperparams;
        std::unordered_map<std::string, std::string> search_space;
    } create;

    struct Drop : ASTDDLQuery {
        enum class Kind { Drop, Detach, Truncate };
        Kind kind = Kind::Drop;
        ObjectKind object_kind = ObjectKind::Table;
        std::string pipeline_name;
        std::string stage_name;
    } drop;

    struct Alter : ASTDDLQuery {
        enum class Target { Table, Node, ReplicaGroup, ShardGroup, Connector, Pipeline, Stream };
        Target target = Target::Table;
        std::vector<ASTAlterQuery::AlterCommand> commands;
        struct NodeSet {
            std::string property;
            std::string value;
        };
        std::vector<NodeSet> node_sets;
        std::vector<NodeSet> replica_group_sets;
        std::vector<NodeSet> shard_group_sets;
        std::vector<NodeSet> connector_sets;
        std::vector<NodeSet> pipeline_sets;
        std::vector<NodeSet> stream_sets;
    } alter;

    struct RegisterNode {
        std::string node_name;
        std::string host;
        uint16_t port = 0;
    } register_node;

    struct DrainNode {
        std::string node_name;
    } drain_node;

    struct RemoveNode {
        std::string node_name;
        bool if_exists = false;
    } remove_node;

    struct Show {
        enum class ShowType {
            DATABASES, TABLES, VIEWS, MATERIALIZED_VIEWS, STORAGE_UNITS, STORAGE_USAGE,
            NODES, NODE_METRICS, NODE_CAPABILITIES, NODE_PARTITIONS, NODE_REPLICAS, CLUSTERS,
            REPLICA_GROUPS, REPLICATION_STATUS,
            SHARD_GROUPS, SHARDS, SHARD_STATUS,
            CONNECTORS, CONNECTOR_CAPABILITIES, CONNECTOR_STATUS,
            PIPELINES, STAGES, TASKS, TRIGGERS, PIPELINE_RUNS, PIPELINE_METRICS,
            STREAMS, TOPICS, CONSUMER_GROUPS, STREAM_METRICS,
            // MODEL layer
            MODELS, MODEL_VERSIONS, MODEL_ENDPOINTS, MODEL_METRICS, MODEL_DRIFT,
            FEATURE_SETS, DATASETS, TRAINING_JOBS, TUNING_JOBS, MODEL_TEMPLATES
        };
        ShowType show_type = ShowType::DATABASES;
        std::string node_name;
        std::string connector_name;
        std::string pipeline_name;
        std::string stream_name;
        std::string model_name;
    } show;

    struct PipelineControl {
        std::string pipeline_name;
    } pipeline_control;

    struct StreamControl {
        std::string stream_name;
        std::string topic_name;
        std::string consumer_group_name;
        uint32_t limit = 0;
        std::vector<std::vector<std::string>> values;
    } stream_control;

    struct TestQuery {
        ObjectKind object_kind = ObjectKind::Connector;
        std::string name;
    } test_query;

    struct Discover {
        std::string connector_name;
    } discover;

    struct Describe {
        ObjectKind object_kind = ObjectKind::Table;
        std::string table_name;
        std::string model_name;
        uint32_t version = 0;            // for DESCRIBE MODEL VERSION m:vN
    } describe;

    // ── MODEL layer control verbs (RUN job / DEPLOY / PREDICT / EVALUATE / ...) ──
    struct ModelControl {
        enum class Action {
            None, RunTrainingJob, RunTuningJob, Deploy, Predict, Evaluate, Compare, Generate,
            Explain
        };
        Action action = Action::None;
        std::string target_name;         // job name being run
        std::string model_name;
        uint32_t version = 0;            // 0 = latest
        std::string endpoint_name;       // DEPLOY ... AS <name>
        std::string predict_mode;        // entity | features | batch
        std::vector<std::pair<std::string, std::string>> kv;  // FOR(...) / WITH(...)
        std::string from_table;          // PREDICT ... FROM <table>
        std::string prompt;              // GENERATE ... PROMPT '...'
        std::vector<std::pair<std::string, uint32_t>> compare_targets; // model:vN list
    } model_control;

    struct Refresh {
        std::string name;
    } refresh;

    struct Explain {
        std::shared_ptr<QueryAST> explain_query;
    } explain;

    struct Use {
        std::string database_name;
    } use;
};

// ── Expression — base for expression AST nodes ──
class Expression {
public:
    enum class OpType {
        AND, OR, NOT,
        Plus, Minus, Star, Slash, Percent,
        Eq, Ne, Lt, Gt, Le, Ge,
        Concat
    };

    OpType op = OpType::AND;
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
    explicit Expression(OpType op = OpType::AND,
                        std::shared_ptr<Expression> l = nullptr,
                        std::shared_ptr<Expression> r = nullptr)
        : op(op), left(std::move(l)), right(std::move(r)) {}
    virtual ~Expression() = default;

    [[nodiscard]]
    virtual std::string to_string() const {
        return "Expression";
    }
};

// ── Literal — a literal value node ──
class Literal : public Expression {
public:
    std::string value;
    explicit Literal(const std::string& val) : Expression(), value(val) {}

    [[nodiscard]]
    std::string to_string() const override;
};

// ── Identifier — a column/table name node ──
class Identifier : public Expression {
public:
    std::string name;
    explicit Identifier(const std::string& n) : Expression(), name(n) {}

    [[nodiscard]]
    std::string to_string() const override;
};

// ── ColumnDef ──
struct ColumnDef {
    std::string name;
    std::string data_type;
};

// ── OrderBy ──
struct OrderBy {
    std::string column;
    enum class Direction { ASC, DESC };
    Direction direction = Direction::ASC;
};

} // namespace mnemo::parsers
