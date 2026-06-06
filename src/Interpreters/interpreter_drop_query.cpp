// src/Interpreters/interpreter_drop_query.cpp — DROP interpreter implementation

#include "Interpreters/interpreter_drop_query.h"
#include "Interpreters/ddl_guard.h"
#include "Interpreters/ddl_transaction.h"
#include "Interpreters/interpreter_ddl_utils.h"
#include "Storages/memory_storage.h"
#include "Storages/file_storage.h"
#include "StorageUnits/storage_unit_manager.h"
#include "ReplicaGroups/replica_group_manager.h"
#include "ShardGroups/shard_group_manager.h"
#include "Connectors/connector_manager.h"
#include "Pipelines/pipeline_manager.h"
#include "Streaming/stream_manager.h"
#include "Models/model_manager.h"
#include "FeatureSets/feature_set_manager.h"
#include "Common/exceptions.h"

namespace mnemo::interpreters {

namespace {

auto resolve_storage(Context& context, const std::string& table)
    -> std::shared_ptr<storages::IStorage> {
    return ddl_utils::resolve_storage(context, table);
}

} // namespace

auto InterpreterDropQuery::execute(Context& context, const parsers::QueryAST& query)
    -> core::Block {
    DDLTransaction txn{context};
    const auto db_name = ddl_utils::resolve_current_database(context);
    txn.snapshot_table(db_name, query.drop.table);

    switch (query.drop.kind) {
        case parsers::QueryAST::Drop::Kind::Drop:
            if (query.drop.object_kind == parsers::QueryAST::ObjectKind::View) {
                do_drop_view(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::MaterializedView) {
                do_drop_materialized_view(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::StorageUnit) {
                do_drop_storage_unit(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::ReplicaGroup) {
                do_drop_replica_group(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::ShardGroup) {
                do_drop_shard_group(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Connector) {
                do_drop_connector(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Stream) {
                do_drop_stream(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Topic) {
                do_drop_topic(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::ConsumerGroup) {
                do_drop_consumer_group(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Pipeline) {
                do_drop_pipeline(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Stage) {
                do_drop_stage(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Task) {
                do_drop_task(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Trigger) {
                do_drop_trigger(context, query.drop);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Model) {
                models::ModelManager::instance().drop_model(query.drop.table, query.drop.if_exists);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::ModelTemplate) {
                models::ModelManager::instance().drop_template(query.drop.table, query.drop.if_exists);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::TrainingJob) {
                models::ModelManager::instance().drop_training_job(query.drop.table, query.drop.if_exists);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::TuningJob) {
                models::ModelManager::instance().drop_tuning_job(query.drop.table, query.drop.if_exists);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::FeatureSet) {
                feature_sets::FeatureSetManager::instance().drop_feature_set(query.drop.table, query.drop.if_exists);
            } else if (query.drop.object_kind == parsers::QueryAST::ObjectKind::Dataset) {
                feature_sets::FeatureSetManager::instance().drop_dataset(query.drop.table, query.drop.if_exists);
            } else {
                do_drop(context, query.drop);
            }
            break;
        case parsers::QueryAST::Drop::Kind::Truncate:
            do_truncate(context, query.drop);
            break;
        case parsers::QueryAST::Drop::Kind::Detach:
            do_detach(context, query.drop);
            break;
    }

    txn.commit();
    return ddl_utils::make_ok_block();
}

auto InterpreterDropQuery::do_drop(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto db = ddl_utils::require_database(context, db_name);
    if (!db->table_exists(drop.table)) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown table: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    auto storage = db->table(drop.table);
    if (auto file = std::dynamic_pointer_cast<storages::FileStorage>(storage)) {
        const auto unit_name = file->storage_unit_name();
        if (!unit_name.empty()) {
            storage_units::StorageUnitManager::instance().release_disk(unit_name);
        }
    }
    if (storage) {
        const auto sg_name = storage->shard_group_name();
        if (!sg_name.empty()) {
            shard_groups::ShardGroupManager::instance().release_group(sg_name);
        }
        const auto rg_name = storage->replica_group_name();
        if (!rg_name.empty()) {
            replica_groups::ReplicaGroupManager::instance().release_group(rg_name);
        }
    }

    db->drop_table(drop.table);
    context.unregister_storage(drop.table);
}

auto InterpreterDropQuery::do_drop_view(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto db = ddl_utils::require_catalog(context);
    if (!db->has_view(drop.table)) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown view: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    db->drop_view(drop.table);
}

auto InterpreterDropQuery::do_drop_materialized_view(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto db = ddl_utils::require_catalog(context);
    if (!db->has_materialized_view(drop.table)) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown materialized view: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    db->drop_table(drop.table);
    db->drop_materialized_view(drop.table);
    context.unregister_storage(drop.table);
}

auto InterpreterDropQuery::do_drop_storage_unit(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    (void)context;
    storage_units::StorageUnitManager::instance().drop_unit(drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_replica_group(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    (void)context;
    replica_groups::ReplicaGroupManager::instance().drop_group(drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_shard_group(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    (void)context;
    shard_groups::ShardGroupManager::instance().drop_group(drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_connector(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    (void)context;
    connectors::ConnectorManager::instance().drop_connector(drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_pipeline(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    (void)context;
    pipelines::PipelineManager::instance().drop_pipeline(drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_stage(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    (void)context;
    pipelines::PipelineManager::instance().drop_stage(
        drop.pipeline_name, drop.stage_name, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_task(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    (void)context;
    pipelines::PipelineManager::instance().drop_task(
        drop.pipeline_name, drop.stage_name, drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_trigger(
    Context& context, const parsers::QueryAST::Drop& drop) -> void {
    (void)context;
    if (drop.pipeline_name.empty()) {
        throw common::Exception{
            "DROP TRIGGER requires FROM PIPELINE clause",
            static_cast<int>(common::ErrorCode::SYNTAX_ERROR)};
    }
    pipelines::PipelineManager::instance().drop_trigger(
        drop.pipeline_name, drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_stream(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    (void)context;
    streaming::StreamManager::instance().drop_stream(drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_topic(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    (void)context;
    streaming::StreamManager::instance().drop_topic(drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_drop_consumer_group(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    (void)context;
    streaming::StreamManager::instance().drop_consumer_group(drop.table, drop.if_exists);
}

auto InterpreterDropQuery::do_truncate(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto storage = resolve_storage(context, drop.table);
    if (!storage) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown table: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    if (auto mem = std::dynamic_pointer_cast<storages::MemoryStorage>(storage)) {
        mem->truncate();
        return;
    }

    storage->alter([](storages::IStorage& s) {
        if (auto* mem = dynamic_cast<storages::MemoryStorage*>(&s)) {
            mem->truncate();
        }
    });
}

auto InterpreterDropQuery::do_detach(Context& context, const parsers::QueryAST::Drop& drop)
    -> void {
    const auto db_name = ddl_utils::resolve_current_database(context);
    DDLGuard guard{db_name, drop.table};

    auto db = ddl_utils::require_database(context, db_name);
    if (!db->table_exists(drop.table)) {
        if (drop.if_exists) {
            return;
        }
        throw common::Exception{
            "Unknown table: " + drop.table,
            static_cast<int>(common::ErrorCode::UNKNOWN_TABLE)};
    }

    db->detach_table(drop.table);
    context.unregister_storage(drop.table);
}

} // namespace mnemo::interpreters
