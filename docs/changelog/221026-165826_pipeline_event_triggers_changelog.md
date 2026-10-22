# Pipeline event triggers

**Date:** 2026-10-22
**Branch:** `feature/pipeline-event-triggers`
**PRD:** `docs/PRDS/PIPELINE_LAYER_.md` §10 (TRIGGER), §15 (Event-Driven execution) — Phase 2 "Event Triggers".

## Summary

Before this change a pipeline `TRIGGER` could only be `MANUAL` (run via `RUN PIPELINE`) or
`SCHEDULE` (a cron expression checked each scheduler tick). This adds the three event-driven trigger
types from the PRD, so a pipeline can run in reaction to data instead of a clock:

- `ON INSERT INTO <table>` — rows were inserted into a table.
- `ON STREAM <stream>` — an event was appended to a stream (directly, or via a `PUBLISH` to a bound
  topic).
- `WEBHOOK [SECRET '<token>']` — an HTTP `POST` reached `/webhooks/<pipeline>/<trigger>`.

Mnemo has no `UPDATE` or `DELETE`, so insert is the only table event.

## Grammar

`CREATE TRIGGER <name> ON PIPELINE <pipeline> [ <event> ]` where `<event>` is at most one of
`SCHEDULE '<cron>'`, `ON INSERT INTO <table>`, `ON STREAM <stream>`, or `WEBHOOK [SECRET '<token>']`.
With no event clause the trigger is manual. A leftover or mistyped keyword after the clause is now a
syntax error — previously it was ignored, so a typo silently produced a manual trigger.

## Design

- **`src/Pipelines/pipeline_event_bus.{h,cpp}` (new)** — a singleton `PipelineEventBus` that turns
  events into queued `TriggerRef`s and hands them to the `PipelineScheduler`.
  - Queued triggers are a `std::set`, so a burst of events before the scheduler drains the queue
    yields **one** run per trigger, not one per event.
  - A thread-local `RunScope` (RAII), held by `PipelineExecutor::run` for the length of a run,
    makes events raised on that thread no-ops. This prevents a pipeline that writes the table it
    watches from re-triggering itself, and prevents two pipelines from triggering each other in a
    loop. The trade-off — one pipeline's output does not start another through a trigger — is
    documented.
- **`PipelineScheduler`** now waits on a `std::condition_variable` instead of sleeping. Cron is
  still evaluated once per tick; the event bus's wake callback makes the worker drain the event
  queue immediately. `dispatch_events` drops a queued trigger whose pipeline was dropped, is paused,
  or is already running — the same policy as a missed schedule. Both cron and event runs go through
  the existing run callback (wired in `src/Server/server.cpp`), which holds the execution lock.
- **Event sources:**
  - `interpreter_insert_query.cpp` calls `table_inserted("db.table")` after a table write and
    `stream_appended(stream)` after a stream append.
  - `interpreter_stream_control.cpp` calls `stream_appended` for every stream bound to a published
    topic.
- **Webhook route** (`src/Server/http_handler.cpp`): `POST /webhooks/<pipeline>/<trigger>` validates
  the path, looks up the trigger, checks it is a `WEBHOOK` trigger, and (when a secret is set)
  compares the `X-Webhook-Token` header to the secret in **constant time**. Responses: `202` queued,
  `401` missing/wrong token, `404` unknown webhook, `405` non-`POST`, `409` pipeline paused. The run
  is asynchronous and the request body is not passed to the tasks. New HTTP status lines (202/401/
  405/409) were added to `http_server.cpp`.
- **Catalog** (`pipeline_catalog.{h,cpp}`): `TriggerType` gains `TableInsert`, `StreamEvent`,
  `Webhook`; `TriggerEntry` gains `target` (watched `database.table` or stream) and `secret`
  (webhook only, never shown). Name/parse mappings and validation updated. Triggers remain
  in-memory, like schedule triggers — no on-disk format changed.
- **Interpreter** (`interpreter_create_query.cpp`): a `TableInsert`/`StreamEvent` trigger is
  refused at creation if the table or stream does not exist, so a typo cannot create a trigger that
  can never fire. Table names are qualified to `database.table` via the new
  `ddl_utils::qualify_table_name`, so the trigger keeps watching the same table across a later
  `USE`.

## Tests

- `tests/test_parsers.h`: `CREATE TRIGGER` grammar — manual, schedule, table-insert (qualified and
  unqualified), stream, webhook (with and without secret), and the rejection of malformed clauses.

## Fix folded in

While wiring the stream event source, `StreamManager::streams_bound_to_topic` had been declared and
defined twice (the method already existed from the streaming layer). The duplicate caused a
compile error (`C2535`/`C2084`); the older unsorted copy was removed in favour of the sorted,
documented one.

## Files

- New: `src/Pipelines/pipeline_event_bus.{h,cpp}`.
- Changed: `src/Parsers/parser.cpp`, `src/Parsers/ast.h`,
  `src/Interpreters/interpreter_create_query.cpp`, `src/Interpreters/interpreter_ddl_utils.h`,
  `src/Interpreters/interpreter_insert_query.cpp`,
  `src/Interpreters/interpreter_stream_control.cpp`, `src/Interpreters/pipeline_executor.cpp`,
  `src/Pipelines/pipeline_catalog.{h,cpp}`, `src/Pipelines/pipeline_manager.{h,cpp}`,
  `src/Pipelines/pipeline_scheduler.{h,cpp}`, `src/Streaming/stream_manager.{h,cpp}`,
  `src/Server/http_handler.{h,cpp}`, `src/Server/http_server.cpp`.
- Docs: `docs/mnemo_sql_documentation/entities/pipelines.md`,
  `docs/mnemo_sql_documentation/grammar/{full-grammar,statement-catalog,keywords}.md`.
