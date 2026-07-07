// src/Interpreters/interpreter_create_query.cpp — CREATE interpreter implementation

#include "Interpreters/interpreter_create_query.h"
#include "Interpreters/ddl_guard.h"
#include "Interpreters/ddl_transaction.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Interpreters/interpreter_select_query.h"
#include "Databases/database_manager.h"
#include "Databases/database_memory.h"
#include "Databases/view_catalog.h"
#include "Storages/memory_storage.h"
#include "Storages/file_storage.h"
#include "Storages/storage_factory.h"
#include "StorageUnits/storage_unit_catalog.h"
#include "StorageUnits/storage_unit_manager.h"
#include "Nodes/node_catalog.h"
#include "Nodes/node_manager.h"
#include "ReplicaGroups/replica_group_catalog.h"
#include "ReplicaGroups/replica_group_manager.h"
#include "ShardGroups/shard_group_catalog.h"
#include "ShardGroups/shard_group_manager.h"
#include "Connectors/connector_catalog.h"
#include "Connectors/connector_manager.h"
#include "Pipelines/pipeline_catalog.h"
#include "Pipelines/pipeline_manager.h"
#include "Streaming/stream_manager.h"
#include "Models/model_manager.h"
#include "Models/model_catalog.h"
#include "FeatureSets/feature_set_manager.h"
#include "FeatureSets/feature_set_catalog.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

auto InterpreterCreateQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    DDLTransaction txn{context};

    switch (query.create.kind) {
        case parsers::QueryAST::Create::Kind::View:
            do_create_view(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::MaterializedView: {
            const auto db_name = ddl_utils::resolve_current_database(context);
            txn.snapshot_table(db_name, query.create.view_name);
            do_create_materialized_view(context, query.create);
            break;
        }
        case parsers::QueryAST::Create::Kind::Table: {
            const auto db_name = ddl_utils::resolve_current_database(context);
            txn.snapshot_table(db_name, query.create.table_name);
            do_create_table(context, query.create);
            break;
        }
        case parsers::QueryAST::Create::Kind::Database:
            do_create_database(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::StorageUnit:
            do_create_storage_unit(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Node:
            do_create_node(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Cluster:
            do_create_cluster(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::ReplicaGroup:
            do_create_replica_group(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::ShardGroup:
            do_create_shard_group(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Connector:
            do_create_connector(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Pipeline:
            do_create_pipeline(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Stage:
            do_create_stage(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Task:
            do_create_task(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Trigger:
            do_create_trigger(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Stream:
            do_create_stream(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Topic:
            do_create_topic(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::ConsumerGroup:
            do_create_consumer_group(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Model:
            do_create_model(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::FeatureSet:
            do_create_feature_set(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::Dataset:
            do_create_dataset(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::TrainingJob:
            do_create_training_job(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::TuningJob:
            do_create_tuning_job(context, query.create);
            break;
        case parsers::QueryAST::Create::Kind::ModelTemplate:
            do_create_model_template(context, query.create);
            break;
        default:
            throw common::Exception{
                "CREATE: expected DATABASE, TABLE, VIEW, MATERIALIZED VIEW, STORAGE_UNIT, "
                "NODE, CLUSTER, REPLICA_GROUP, SHARD_GROUP, CONNECTOR, STREAM, TOPIC, "
                "CONSUMER_GROUP, PIPELINE, STAGE, TASK, TRIGGER, MODEL, FEATURE_SET, DATASET, "
                "TRAINING_JOB, TUNING_JOB, or MODEL_TEMPLATE",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }

    txn.commit();
    return ddl_utils::make_ok_block();
}

auto InterpreterCreateQuery::do_create_database(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    if (context.get_database(create.database_name)) {
        if (create.if_not_exists) {
            return;
        }
        throw common::Exception{
            "Database already exists: " + create.database_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto& db_manager = databases::DatabaseManager::instance();
    std::shared_ptr<databases::IDatabase> db = db_manager.create_database(create.database_name);
    if (!db) {
        db = databases::DatabaseMemory::create(create.database_name);
    }
    context.register_database(create.database_name, db);
}

auto InterpreterCreateQuery::do_create_table(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, create.table_name};

    auto db = ddl_utils::require_database(context, db_name);
    const bool exists = [&] {
        if (auto catalog = std::dynamic_pointer_cast<databases::Database>(db)) {
            return catalog->relation_exists(create.table_name);
        }
        return db->table_exists(create.table_name);
    }();
    if (exists) {
        if (create.if_not_exists) {
            if (auto existing = db->table(create.table_name)) {
                context.register_storage(create.table_name, existing);
            }
            return;
        }
        throw common::Exception{
            "Table already exists: " + create.table_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    const auto engine = create.engine.empty() ? "Memory" : create.engine;
    if (!storages::StorageFactory::instance().has(engine)) {
        throw common::Exception{
            "Unknown storage engine: " + engine,
            static_cast<int>(common::ErrorCode::UNKNOWN_DATABASE)};
    }

    auto columns = ddl_utils::build_column_map(create.columns);
    auto storage = db->create_table(create.table_name, columns, engine);
    if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        mem->set_columns(columns);
    } else if (auto file = std::dynamic_pointer_cast<storages::FileStorage>(storage)) {
        file->set_columns(columns);
    }

    if (!create.storage_unit_name.empty()) {
        auto& mgr = storage_units::StorageUnitManager::instance();
        const auto* unit = mgr.get_unit(create.storage_unit_name);
        if (!unit) {
            throw common::Exception{
                "Unknown storage unit: " + create.storage_unit_name,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        if (engine != "File") {
            throw common::Exception{
                "STORAGE_UNIT clause requires ENGINE=File",
                static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
        }
        auto disk = mgr.acquire_disk(create.storage_unit_name);
        if (auto file = std::dynamic_pointer_cast<storages::FileStorage>(storage)) {
            const auto table_path = db_name + "/" + create.table_name;
            file->set_disk(disk);
            file->set_data_path(table_path);
            file->set_storage_unit_name(create.storage_unit_name);
            disk->create_dir(table_path);
        }
    }

    if (!create.shard_group_name.empty()) {
        auto& sg_mgr = shard_groups::ShardGroupManager::instance();
        if (!sg_mgr.has_group(create.shard_group_name)) {
            throw common::Exception{
                "Unknown shard group: " + create.shard_group_name,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        sg_mgr.acquire_group(create.shard_group_name);
        if (storage) {
            storage->set_shard_group_name(create.shard_group_name);
        }
    }

    if (!create.replica_group_name.empty()) {
        auto& rg_mgr = replica_groups::ReplicaGroupManager::instance();
        if (!rg_mgr.has_group(create.replica_group_name)) {
            throw common::Exception{
                "Unknown replica group: " + create.replica_group_name,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
        rg_mgr.acquire_group(create.replica_group_name);
        if (storage) {
            storage->set_replica_group_name(create.replica_group_name);
        }
    }

    if (storage) {
        context.register_storage(create.table_name, storage);
    }
}

auto InterpreterCreateQuery::do_create_view(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, create.view_name};

    auto db = ddl_utils::require_catalog(context);
    if (db->relation_exists(create.view_name)) {
        if (create.if_not_exists) {
            return;
        }
        throw common::Exception{
            "View already exists: " + create.view_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto deps = databases::extract_dependencies(create.select_definition);
    for (const auto& dep : deps) {
        if (!db->table_exists(dep) && !db->has_view(dep) && !db->has_materialized_view(dep)) {
            throw common::Exception{
                "Unknown table or view in view definition: " + dep,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
    }

    databases::ViewEntry entry;
    entry.name = create.view_name;
    entry.definition = create.select_definition;
    entry.dependencies = std::move(deps);
    db->create_view(std::move(entry));
}

auto InterpreterCreateQuery::do_create_materialized_view(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, create.view_name};

    auto db = ddl_utils::require_catalog(context);
    if (db->relation_exists(create.view_name)) {
        if (create.if_not_exists) {
            if (auto storage = db->table(create.view_name)) {
                context.register_storage(create.view_name, storage);
            }
            return;
        }
        throw common::Exception{
            "Materialized view already exists: " + create.view_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto deps = databases::extract_dependencies(create.select_definition);
    for (const auto& dep : deps) {
        if (!db->table_exists(dep) && !db->has_view(dep) && !db->has_materialized_view(dep)) {
            throw common::Exception{
                "Unknown table or view in materialized view definition: " + dep,
                static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
        }
    }

    parsers::QueryAST select_query;
    select_query.query_type = parsers::QueryAST::QueryType::SELECT;
    select_query.select = create.select_definition;
    auto result = InterpreterSelectQuery::execute(context, select_query);

    auto columns = ddl_utils::block_column_map(result);
    if (columns.empty()) {
        throw common::Exception{
            "Materialized view definition produced no columns",
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    auto storage = db->create_table(create.view_name, columns, "Memory");
    if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        mem->load_block(result);
    } else if (!storage->write(result)) {
        db->drop_table(create.view_name);
        throw common::Exception{
            "Failed to populate materialized view: " + create.view_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    databases::MaterializedViewEntry entry;
    entry.name = create.view_name;
    entry.definition = create.select_definition;
    entry.backing_table = create.view_name;
    entry.dependencies = std::move(deps);
    db->create_materialized_view(std::move(entry));
    context.register_storage(create.view_name, storage);
}

auto InterpreterCreateQuery::do_create_storage_unit(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    auto& mgr = storage_units::StorageUnitManager::instance();
    if (mgr.has_unit(create.storage_unit_name)) {
        if (create.if_not_exists) {
            return;
        }
        throw common::Exception{
            "Storage unit already exists: " + create.storage_unit_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    storage_units::StorageUnitEntry entry;
    entry.name = create.storage_unit_name;
    entry.type = storage_units::parse_storage_unit_type(create.storage_unit_type);
    for (const auto& [key, value] : create.storage_properties) {
        if (key == "PATH") {
            entry.path = value;
        } else if (key == "BUCKET") {
            entry.bucket = value;
        } else if (key == "ENDPOINT") {
            entry.endpoint = value;
        } else if (key == "REGION") {
            entry.region = value;
        } else if (key == "ACCESS_KEY") {
            entry.access_key = value;
        } else if (key == "SECRET_KEY") {
            entry.secret_key = value;
        }
    }
    mgr.create_unit(std::move(entry));
}

auto InterpreterCreateQuery::do_create_node(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    nodes::NodeEntry entry;
    entry.node_name = create.node_name;
    entry.node_id = create.node_name;
    if (!create.node_type.empty()) {
        entry.node_type = nodes::parse_node_type(create.node_type);
    }
    if (!create.node_role.empty()) {
        entry.node_role = nodes::parse_node_role(create.node_role);
    }
    entry.status = nodes::NodeStatus::Registering;
    entry.capabilities = nodes::capabilities_for_type(entry.node_type);
    entry.resources = nodes::detect_local_resources();
    nodes::NodeManager::instance().create_node(std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_cluster(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    nodes::ClusterEntry entry;
    entry.name = create.cluster_name;
    nodes::NodeManager::instance().create_cluster(std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_replica_group(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    replica_groups::ReplicaGroupEntry entry;
    entry.name = create.replica_group_name;
    entry.replication_factor = create.replica_count > 0 ? create.replica_count : 1;
    if (!create.consistency_mode.empty()) {
        entry.consistency_mode = replica_groups::parse_consistency_mode(create.consistency_mode);
    }
    if (!create.replica_strategy.empty()) {
        entry.strategy = replica_groups::parse_replica_strategy(create.replica_strategy);
    }
    if (!create.placement_policy.empty()) {
        entry.placement_policy = replica_groups::parse_placement_policy(create.placement_policy);
    }
    replica_groups::ReplicaGroupManager::instance().create_group(std::move(entry),
                                                                 create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_shard_group(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    shard_groups::ShardGroupEntry entry;
    entry.name = create.shard_group_name;
    entry.shard_count = create.shard_count > 0 ? create.shard_count : 1;
    entry.shard_key = create.shard_key;
    if (!create.shard_type.empty()) {
        entry.strategy = shard_groups::parse_shard_strategy(create.shard_type);
    }
    shard_groups::ShardGroupManager::instance().create_group(std::move(entry),
                                                             create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_connector(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    auto& mgr = connectors::ConnectorManager::instance();
    if (mgr.has_connector(create.connector_name)) {
        if (create.if_not_exists) {
            return;
        }
        throw common::Exception{
            "Connector already exists: " + create.connector_name,
            static_cast<int>(common::ErrorCode::LOGICAL_ERROR)};
    }

    connectors::ConnectorEntry entry;
    entry.name = create.connector_name;
    entry.type = connectors::parse_connector_type(create.connector_type);
    if (!create.auth_method.empty()) {
        entry.auth = connectors::parse_auth_method(create.auth_method);
    }
    entry.properties = create.connector_properties;
    mgr.create_connector(std::move(entry));
}

auto InterpreterCreateQuery::do_create_pipeline(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    pipelines::PipelineEntry entry;
    entry.name = create.pipeline_name;
    entry.owner = create.pipeline_owner;
    pipelines::PipelineManager::instance().create_pipeline(std::move(entry),
                                                             create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_stage(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    pipelines::StageEntry stage;
    stage.name = create.stage_name;
    stage.order = create.stage_order;
    pipelines::PipelineManager::instance().create_stage(
        create.pipeline_name, std::move(stage), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_task(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    pipelines::TaskEntry task;
    task.name = create.task_name;
    task.type = pipelines::parse_task_type(create.task_type);
    task.body = create.task_body;
    task.depends_on = create.task_depends_on;
    pipelines::PipelineManager::instance().create_task(
        create.pipeline_name, create.stage_name, std::move(task), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_trigger(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    pipelines::TriggerEntry trigger;
    trigger.name = create.trigger_name;
    if (!create.trigger_schedule.empty()) {
        trigger.type = pipelines::TriggerType::Schedule;
        trigger.schedule = create.trigger_schedule;
    } else {
        trigger.type = pipelines::TriggerType::Manual;
    }
    pipelines::PipelineManager::instance().create_trigger(
        create.pipeline_name, std::move(trigger), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_stream(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    streaming::StreamEntry entry;
    entry.name = create.stream_name;
    entry.topic_name = create.topic_name;
    entry.retention_days = create.retention_days;
    entry.retention = create.retention_forever
        ? streaming::RetentionPolicy::Forever
        : streaming::RetentionPolicy::Days;
    streaming::StreamManager::instance().create_stream(std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_topic(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    streaming::TopicEntry entry;
    entry.name = create.topic_name;
    entry.partition_count = create.partition_count;
    entry.retention_days = create.retention_days;
    entry.retention = create.retention_forever
        ? streaming::RetentionPolicy::Forever
        : streaming::RetentionPolicy::Days;
    streaming::StreamManager::instance().create_topic(std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_consumer_group(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    streaming::ConsumerGroupEntry entry;
    entry.name = create.consumer_group_name;
    streaming::StreamManager::instance().create_consumer_group(
        std::move(entry), create.if_not_exists);
}

// ── MODEL layer ──

namespace {
auto to_ordered_map(const std::unordered_map<std::string, std::string>& in)
    -> std::map<std::string, std::string> {
    return {in.begin(), in.end()};
}
} // namespace

auto InterpreterCreateQuery::do_create_model(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    models::ModelEntry entry;
    entry.name = create.model_name;
    if (!create.model_type.empty()) {
        models::ModelType t;
        if (!models::parse_model_type(create.model_type, t)) {
            throw common::Exception{
                "CREATE MODEL: unknown TYPE '" + create.model_type + "'",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        entry.type = t;
    }
    entry.owner = create.pipeline_owner;
    models::ModelManager::instance().create_model(std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_feature_set(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    feature_sets::FeatureSetEntry entry;
    entry.name = create.feature_set_name;
    entry.source_table = create.source_table;
    entry.entity_key = create.entity_key;
    entry.features = create.features;
    entry.target = create.target;
    feature_sets::FeatureSetManager::instance().create_feature_set(
        std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_dataset(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    feature_sets::DatasetEntry entry;
    entry.name = create.dataset_name;
    entry.source = create.source_table;
    feature_sets::FeatureSetManager::instance().create_dataset(
        std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_training_job(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    models::TrainingJobEntry entry;
    entry.name = create.training_job_name;
    entry.model = create.ref_model;
    entry.feature_set = create.ref_feature_set;
    entry.dataset = create.ref_dataset;
    entry.algorithm = create.algorithm;
    entry.entrypoint = create.entrypoint;
    entry.objective = create.objective;
    entry.hyperparams = to_ordered_map(create.hyperparams);
    if (!create.framework.empty()) {
        models::Framework f;
        if (!models::parse_framework(create.framework, f)) {
            throw common::Exception{
                "CREATE TRAINING_JOB: unknown FRAMEWORK '" + create.framework + "'",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        entry.framework = f;
    }
    models::ModelManager::instance().create_training_job(std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_tuning_job(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    models::TuningJobEntry entry;
    entry.name = create.tuning_job_name;
    entry.model = create.ref_model;
    entry.training_job = create.ref_training_job;
    entry.objective = create.objective;
    if (create.trials > 0) entry.trials = create.trials;
    entry.search_space = to_ordered_map(create.search_space);
    if (!create.strategy.empty()) {
        models::TuningStrategy s;
        if (!models::parse_tuning_strategy(create.strategy, s)) {
            throw common::Exception{
                "CREATE TUNING_JOB: unknown STRATEGY '" + create.strategy + "'",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        entry.strategy = s;
    }
    models::ModelManager::instance().create_tuning_job(std::move(entry), create.if_not_exists);
}

auto InterpreterCreateQuery::do_create_model_template(
    Context& context, const parsers::QueryAST::Create& create) -> void {
    (void)context;
    models::ModelTemplateEntry entry;
    entry.name = create.model_template_name;
    entry.algorithm = create.algorithm;
    if (!create.framework.empty()) {
        models::Framework f;
        if (!models::parse_framework(create.framework, f)) {
            throw common::Exception{
                "CREATE MODEL_TEMPLATE: unknown FRAMEWORK '" + create.framework + "'",
                static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
        }
        entry.framework = f;
    }
    models::ModelManager::instance().create_template(std::move(entry), create.if_not_exists);
}

} // namespace mnemo::interpreters
