---
description: "Scheduled pipeline runs now take the same lock as request handling, closing a data race on the shared `Context`."
sidebar_label: "Pipeline Scheduler Execution Lock"
sidebar_position: 7
slug: /changelog/221026-pipeline-scheduler-execution-lock
title: "Pipeline scheduler takes the execution lock"
doc_type: changelog
---
# Pipeline scheduler takes the execution lock {#pipeline-scheduler-takes-the-execution-lock}

**Branch:** `fix/pipeline-scheduler-execution-lock`
**Date:** 2026-10-22

## Problem {#problem}

`interpreters::Context` and the catalogs behind it (databases, views, storage units, ...) have
no locking of their own, so the HTTP server serialises every request with one mutex. The
`PipelineScheduler` thread did not take it: a scheduled pipeline run executed its SQL tasks
against the same `Context` while a request could be running, which is a data race. Manual
`RUN PIPELINE` over HTTP was already protected, because the request holds the lock.

This was noted as a pre-existing race in
`221026-015257_alter_storage_unit_views_changelog.md`, which introduced the shared
`common::execution_mutex`. This change closes it.

## Change {#change}

The run callback registered in `Server::start` now holds `common::execution_mutex` for the
duration of `PipelineExecutor::run`. `PipelineManager` has its own mutex, and the node
membership and replica failover services never touch `Context`, so this was the only
unprotected path onto it besides the materialized view scheduler (which already takes the lock).

## Behaviour change {#behaviour-change}

While a scheduled pipeline runs, HTTP requests wait for it, exactly as they already do during a
manual `RUN PIPELINE`. A long scheduled pipeline therefore delays queries for its duration.

## Verification {#verification}

`test_pipelines_api` 74/74 (including `scheduler fired pipeline run`, which waits for a
scheduled run to complete), `test_pipeline_phase2_api` 136/136, `test_alter_api` 133/133,
`test_views_api` 36/36, `test_nodes_api` 40/40.
