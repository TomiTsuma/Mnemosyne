// tests/test_all.cpp — Catch2 test suite
// Mnemosyne: A column-oriented analytical DBMS

#define CATCH_CONFIG_MAIN
#include "catch2/catch_all.hpp"

#include "test_data_types.h"
#include "test_columns.h"
#include "test_functions.h"
#include "test_parsers.h"
#include "test_analyzer.h"
#include "test_planner.h"
#include "test_interpreter.h"
#include "test_processors.h"
#include "test_storages.h"
#include "test_disks.h"
#include "test_io.h"
#include "test_loggers.h"
#include "test_server.h"
#include "test_backups.h"
#include "test_coordination.h"
#include "test_common.h"
#include "test_aggregate_functions.h"
