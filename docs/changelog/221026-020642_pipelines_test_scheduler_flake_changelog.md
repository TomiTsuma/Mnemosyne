# Fix a time-of-minute flake in the scheduler smoke test

**Branch:** `feature/export-to-connector`
**Date:** 2026-10-22

## Problem

`test_scheduler_smoke` in `scripts/test_pipelines_api.py` created a schedule trigger whose cron
expression names the *current* minute (`<minute> <hour> * * *`), then waited up to 12 seconds
for a run. The scheduler ticks every 5 seconds, so a trigger created in the last seconds of a
minute could expire before any tick saw it, and nothing ever fired: `scheduler fired pipeline
run` failed, roughly 10% of the time, depending only on the wall clock.

It showed up as an unexplained 73/74 in a regression sweep that had passed 74/74 minutes
earlier. It reproduced deterministically by creating the trigger at 02:03:59.6: no run within
12 seconds.

## Fix

If fewer than 10 seconds remain in the current minute, the test waits for the next minute
before building the cron expression. Calling `test_scheduler_smoke` directly at 02:05:59.6, the
same instant that failed before, now passes.

No server code changed; this was a defect in the test, not in the scheduler.
