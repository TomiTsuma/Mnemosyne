# Mnemosyne API Reference

This document describes the public API of Mnemosyne modules.

## Core API

### Block (`mnemosyne::core::Block`)
- `Block()` — Default constructor
- `Block(size_t n_columns, size_t n_rows)` — Construct with dimensions
- `Block(std::vector<std::string> names, std::vector<std::unique_ptr<Column>> columns)` — From columns
- `is_empty() const -> bool` — Check if empty
- `size() const -> size_t` — Number of rows
- `columns() const -> std::vector<std::string>` — Column names
- `add_column(std::string name, std::unique_ptr<Column> column)` — Add column
- `column(const std::string& name) -> Column*` — Get column by name
- `column_types() const -> std::vector<DataTypePtr>` — Column types

### Field (`mnemosyne::core::Field`)
- `Field()` — Default constructor
- `Field(Int64 value)` — Construct from int64
- `Field(Float64 value)` — Construct from float64
- `Field(String value)` — Construct from string
- `Field(Date value)` — Construct from date
- `type() const -> DataTypeId` — Get type
- `as<DataTypeId>() const -> T` — Get value as type

### Series (`mnemosyne::core::Series<T>`)
- `Series(size_t n)` — Construct with n elements
- `Series(std::initializer_list<T> values)` — From initializer list
- `operator[](size_t index) -> T&` — Access element
- `operator[](size_t index) const -> T` — Access element (const)
- `size() const -> size_t` — Number of elements
- `fill(T value)` — Fill with value
- `data() -> T*` — Get raw pointer
- `data() const -> T* const` — Get raw pointer (const)
- Arithmetic operators (+, -, *, /, +=, -=, *=, /=)

## Data Types API

### DataType (`mnemosyne::datatypes::DataType`)
- `virtual ~DataType() = default`
- `type_name() const -> std::string` — Get type name
- `type_id() const -> DataTypeId` — Get type ID
- `create_column() const -> ColumnPtr` — Create column
- `create_field() const -> Field` — Create field
- `can_convert_to(DataTypeId other) const -> bool` — Check conversion

### DataTypeRegistry (`mnemo::datatypes::TypeFactory`)
- `static auto& instance() -> TypeFactory&` — Get singleton
- `register_type(DataTypePtr type)` — Register type
- `get(std::string_view name) -> DataTypePtr` — Get by name
- `get(DataTypeId id) -> DataTypePtr` — Get by ID
- `names() const -> std::vector<std::string>` — List names
- `has(std::string_view name) const -> bool` — Check exists
- `create_field(DataTypeId id) -> Field` — Create field

## Columns API

### Column (`mnemosyne::columns::Column`)
- `virtual ~Column() = default`
- `name() const -> std::string` — Get name
- `size() const -> size_t` — Get size
- `get_value(size_t index) -> Field` — Get value at index
- `insert_value(Field value)` — Insert value
- `swap_rows(size_t i, size_t j)` — Swap two rows
- `clear()` — Clear all values

### ColumnVector<T> (`mnemosyne::columns::ColumnVector<T>`)
- `static auto create() -> ColumnPtr` — Create column
- `get<T>(size_t index) -> T` — Get typed value
- `insert(T value)` — Insert typed value

### ColumnString (`mnemosyne::columns::ColumnString`)
- `static auto create() -> ColumnPtr` — Create column
- `get(size_t index) -> std::string` — Get string
- `insert(std::string value)` — Insert string

## Functions API

### Function (`mnemosyne::functions::Function`)
- `static auto create(...) -> FunctionPtr` — Create function
- `info() const -> FunctionInfo` — Get function info
- `execute(Block& block) -> Block` — Execute function
- `name() const -> std::string` — Get name
- `is_aggregate() const -> bool` — Check if aggregate
- `arg_count() const -> size_t` — Get argument count

### FunctionFactory (`mnemosyne::functions::FunctionFactory`)
- `static auto& instance() -> FunctionFactory&` — Get singleton
- `register_function(FunctionPtr func)` — Register function
- `get(std::string_view name) -> FunctionPtr` — Get by name
- `names() const -> std::vector<std::string>` — List names
- `has(std::string_view name) const -> bool` — Check exists

## Parser API

### Lexer (`mnemosyne::parsers::Lexer`)
- `Lexer(std::string_view text)` — Construct
- `next() -> Token` — Get next token
- `peek() -> Token` — Peek next token
- `is_eof() const -> bool` — Check EOF

### QueryParser (`mnemosyne::parsers::QueryParser`)
- `QueryParser(Lexer lexer)` — Construct
- `parse() -> std::variant<std::shared_ptr<ASTNode>, std::string>` — Parse query
- `get_last_error() const -> std::string` — Get error

### AST (`mnemosyne::parsers::ASTNode`)
- `virtual ~ASTNode() = default`
- `type() const -> ASTNodeType` — Get AST node type
- `to_string() const -> std::string` — Convert to string
- `accept(Visitor& visitor)` — Accept visitor

## Analyzer API

### Analyzer (`mnemosyne::analyzer::Analyzer`)
- `Analyzer(Context& context)` — Construct
- `analyze(std::shared_ptr<ASTNode> ast) -> std::variant<std::shared_ptr<QueryTree>, std::string>` — Analyze AST
- `get_passes() const -> std::vector<std::shared_ptr<AnalyzerPass>>` — Get passes

## Planner API

### Planner (`mnemosyne::planner::Planner`)
- `Planner(Context& context)` — Construct
- `plan(std::shared_ptr<QueryTree> tree) -> std::shared_ptr<ExecutionPlan>` — Plan query
- `estimate_cost(std::shared_ptr<ExecutionPlan> plan) -> double` — Estimate cost

## Interpreter API

### Interpreter (`mnemosyne::interpreters::Interpreter`)
- `virtual ~Interpreter() = default`
- `execute() -> ExecutionResult` — Execute plan
- `plan() const -> ExecutionPlanPtr` — Get plan
- `context() const -> Context` — Get context

### InterpreterFactory (`mnemosyne::interpreters::InterpreterFactory`)
- `static auto create_select(ExecutionPlanPtr plan, Context& ctx) -> InterpreterPtr` — Create SELECT
- `static auto create_insert(ExecutionPlanPtr plan, Context& ctx) -> InterpreterPtr` — Create INSERT
- `static auto create_update(ExecutionPlanPtr plan, Context& ctx) -> InterpreterPtr` — Create UPDATE
- `static auto create_delete(ExecutionPlanPtr plan, Context& ctx) -> InterpreterPtr` — Create DELETE

## Storage API

### IStorage (`mnemosyne::storages::IStorage`)
- `virtual ~IStorage() = default`
- `virtual name() const -> std::string` — Get name
- `virtual engine() const -> std::string` — Get engine
- `virtual path() const -> std::string` — Get path
- `virtual columns() const -> std::vector<std::string>` — Get columns
- `virtual column_types() const -> std::unordered_map<std::string, DataTypePtr>` — Get column types
- `virtual read(std::vector<std::string> column_names, size_t max_block_size) -> core::Block` — Read
- `virtual write(core::Block& block) -> bool` — Write
- `virtual empty() const -> bool` — Check empty
- `virtual row_count() const -> size_t` — Get row count
- `virtual byte_count() const -> size_t` — Get byte count
- `virtual alter(std::function<void(IStorage&)> modify) -> bool` — Alter
- `virtual get_setting(std::string_view name) -> std::optional<SettingValueType>` — Get setting
- `virtual lock() -> bool` — Lock
- `virtual unlock() -> void` — Unlock
- `virtual flush() -> bool` — Flush

### StorageFactory (`mnemosyne::storages::StorageFactory`)
- `static auto& instance() -> StorageFactory&` — Get singleton
- `register_engine(std::string name, std::function<std::shared_ptr<IStorage>()> creator)` — Register
- `get_engine(std::string_view name) -> std::shared_ptr<IStorage>` — Get by name
- `names() const -> std::vector<std::string>` — List names
- `has(std::string_view name) const -> bool` — Check exists

## Server API

### HTTPServer (`mnemosyne::server::HTTPServer`)
- `HTTPServer(Context& context, std::string host, uint16_t port)` — Construct
- `start() -> bool` — Start server
- `stop() -> bool` — Stop server
- `is_running() const -> bool` — Check status
- `port() const -> uint16_t` — Get port
- `shutdown() -> void` — Shutdown

### HTTPHandler (`mnemosyne::server::HTTPHandler`)
- `HTTPHandler(Context& context)` — Construct
- `handle(std::string method, std::string path, std::string body) -> HTTPResponse` — Handle request
- `handle_query(std::string query, std::string format) -> HTTPResponse` — Execute query

## Logger API

### Logger (`mnemosyne::loggers::Logger`)
- `static auto& get_instance() -> Logger&` — Get singleton
- `trace(std::string msg, std::string component)` — Log trace
- `debug(std::string msg, std::string component)` — Log debug
- `info(std::string msg, std::string component)` — Log info
- `warn(std::string msg, std::string component)` — Log warning
- `error(std::string msg, std::string component)` — Log error
- `fatal(std::string msg, std::string component)` — Log fatal
- `set_level(LogLevel level)` — Set level
- `level() const -> LogLevel` — Get level
- `add_target(std::shared_ptr<LogTarget> target)` — Add target
- `remove_target(std::string name)` — Remove target
- `targets() const -> std::vector<std::string>` — List targets
- `clear_targets()` — Clear targets
- `flush()` — Flush

## Common API

### Settings (`mnemosyne::common::Settings`)
- `static auto defaults() -> Settings` — Get defaults
- `get(std::string name) -> std::optional<SettingValueType>` — Get setting
- `set(std::string name, SettingValueType value)` — Set setting
- `set_int(std::string name, Int64 value)` — Set int
- `set_double(std::string name, double value)` — Set double
- `set_bool(std::string name, bool value)` — Set bool
- `set_string(std::string name, std::string value)` — Set string
- `load_from_file(std::string path) -> bool` — Load file
- `save_to_file(std::string path) -> bool` — Save file
- `names() const -> std::vector<std::string>` — List names

### ThreadPool (`mnemosyne::common::ThreadPool`)
- `explicit ThreadPool(size_t n_threads = 0)` — Construct
- `submit(std::function<void()> task) -> std::future<void>` — Submit task
- `shutdown() -> void` — Shutdown
- `size() const -> size_t` — Get size
- `active_count() const -> size_t` — Get active count
- `queue_size() const -> size_t` — Get queue size
