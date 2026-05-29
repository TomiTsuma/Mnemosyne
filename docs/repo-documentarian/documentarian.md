# ClickHouse Developer Documentation

Comprehensive guide to ClickHouse's developer documentation, testing, and build infrastructure.

## Documentation Sources

### Primary Documentation

| Location | Content |
|------|-------|
| `docs/` | Developer documentation source (markdown files) |
| `docs/en/` | English documentation |
| `docs/ru/` | Russian documentation |
| `docs/zh/` | Chinese documentation |
| `docs/fr/` | French documentation |
| `docs/pt-br/` | Portuguese documentation |
| `docs/eu/` | Basque documentation |
| `README.md` | Project overview |
| `CONTRIBUTING.md` | Contribution guidelines |
| `SECURITY.md` | Security policy |
| `CHANGELOG.md` | Release history (~540k lines) |
| `LICENSE` | Apache 2.0 license text |
| `AI_POLICY.md` | AI usage policy |

### In-Source Documentation

| Location | Content |
|------|-------|
| `src/` header files | Doxygen-style comments |
| `programs/*.cpp` | Program-specific documentation |
| `CMakeLists.txt` | Build configuration |
| `cmake/*.cmake` | CMake helper scripts |

## Build System

### CMake Configuration

```bash
# Configure build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Install
cmake --install build
```

### Key CMake Files

| File | Purpose |
|------|-------|
| `CMakeLists.txt` | Root build configuration |
| `cmake/toolchain/` | Cross-compilation toolchains |
| `cmake/linux/` | Linux-specific configs |
| `cmake/darwin/` | macOS-specific configs |
| `cmake/freebsd/` | FreeBSD-specific configs |
| `cmake/sunos/` | Solaris-specific configs |
| `cmake/sanitize.cmake` | Sanitizer configurations |
| `cmake/warnings.cmake` | Compiler warning flags |
| `cmake/version.cmake` | Version generation |
| `cmake/git.cmake` | Git info integration |
| `cmake/ccache.cmake` | Compiler cache support |
| `cmake/cpu_features.cmake` | CPU feature detection |
| `cmake/unwind.cmake` | Stack unwinding support |
| `cmake/xray_instrumentation.cmake` | XRay profiling |
| `cmake/profile_optimization.cmake` | PGO support |
| `cmake/dbms_glob_sources.cmake` | Source file collection |

### Supported Platforms

| Platform | Architecture | Notes |
|------|--|-----|
| Linux | x86_64 | Primary target |
| Linux | aarch64 | Supported |
| Linux | s390x | Supported |
| Linux | RISC-V | Supported |
| macOS | x86_64 | Development target |
| macOS | aarch64 | Supported |
| FreeBSD | x86_64 | Supported |
| Solaris | sparcv9 | Supported |

### Build Types

| Type | CMake Flag | Use |
|------|---|-----|
| Debug | `-DCMAKE_BUILD_TYPE=Debug` | Development |
| Release | `-DCMAKE_BUILD_TYPE=Release` | Production |
| RelWithDebInfo | `-DCMAKE_BUILD_TYPE=RelWithDebInfo` | Profiling |
| MinSizeRel | `-DCMAKE_BUILD_TYPE=MinSizeRel` | Embedded |

### Sanitizers

| Sanitizer | CMake Flag | Purpose |
|------|--|-----|
| AddressSanitizer | `-DENABLE_SANITIZER=address` | Memory bugs |
| UBSan | `-DENABLE_SANITIZER=undefined` | Undefined behavior |
| ThreadSanitizer | `-DENABLE_SANITIZER=thread` | Data races |
| LeakSanitizer | `-DENABLE_SANITIZER=leak` | Memory leaks |
| MemorySanitizer | `-DENABLE_SANITIZER=memory` | Uninitialized reads |

## Testing Infrastructure

### Integration Tests (`tests/`)

| Component | Description |
|------|--|
| `tests/` | Main test directory |
| `tests/integration/` | pytest-based integration tests |
| `tests/clickhouse-test` | Python test runner |
| `tests/pytest.ini` | pytest configuration |
| `tests/conftest.py` | pytest fixtures |
| `tests/helpers/` | Test helper utilities |
| `tests/compose/` | Docker compose for test env |

#### Running Integration Tests

```bash
# Run all integration tests
python3 tests/clickhouse-test --integration-tests-dir tests/integration

# Run specific test
python3 tests/clickhouse-test test_name

# Run with Docker
docker-compose -f tests/compose/docker-compose.yml up
```

### Performance Tests

| Location | Description |
|------|--|
| `tests/performance/` | Performance test suite |
| `tests/per_drafts/` | Experimental performance tests |
| `benchmark/` | Benchmark runner |
| `benchmark/` | Benchmark configuration |

#### Running Benchmarks

```bash
# Run benchmark
clickhouse benchmark < queries.sql

# Run performance tests
python3 tests/clickhouse-test --performance-tests-dir tests/performance
```

### Fuzzing

| Location | Description |
|------|--|
| `tests/fuzz/` | Fuzzing configurations |
| `ci/nightly_fuzzers.yml` | Nightly fuzzing CI |

### Lexer Tests

| Location | Description |
|------|--|
| `tests/lexer/` | SQL lexer unit tests |

### Jepsen Tests

| Location | Description |
|------|--|
| `tests/jepsen.clickhouse` | Jepsen consistency tests |
| `ci/nightly_jepsen.yml` | Jepsen CI workflow |

## CI System

### CI Architecture

```
GitHub Actions (pull_request.yml, master.yml, etc.)
  └── praktika framework (Python)
      ├── job definitions
      ├── workflow configurations
      ├── docker image management
      └── infrastructure provisioning
```

### CI Components

| Component | Location | Purpose |
|------|--|-----|
| CI Framework | `ci/praktika/` | Custom CI framework |
| Workflows | `ci/workflows/` | Workflow definitions |
| Jobs | `ci/jobs/` | Individual job configs |
| Settings | `ci/settings/` | Workflow configuration |
| Docker | `ci/docker/` | CI Docker images |
| Infra | `ci/infra/` | Infrastructure management |
| Tests | `ci/tests/` | CI test infrastructure |

### CI Workflows

| Workflow | Trigger | Purpose |
|------|--|-----|
| `pull_request.yml` | PR open/sync | Build and test PR |
| `master.yml` | Push to master | Nightly build |
| `nightly_coverage.yml` | Scheduled | Code coverage |
| `nightly_fuzzers.yml` | Scheduled | Fuzzing |
| `nightly_jepsen.yml` | Scheduled | Jepsen tests |
| `nightly_keeper.yml` | Scheduled | Keeper tests |
| `nightly_keeper_faults.yml` | Scheduled | Keeper fault injection |
| `nightly_statistics.yml` | Scheduled | Performance stats |
| `release_branches.yml` | Scheduled | Release builds |
| `optimize_clickhouse.yml` | Scheduled | Optimization |
| `optimize_toolchain.yml` | Scheduled | Toolchain optimization |
| `custom_build_praktika.yml` | Manual | Custom builds |
| `vectorsearchstress.yml` | Scheduled | Vector search stress |
| `auto_releases.yml` | Scheduled | Auto-release |
| `cherry_pick.yml` | Manual | Cherry-pick |
| `backport_branches.yml` | Manual | Backport |
| `create_release.yml` | Manual | Create release |

### CI Configuration

```bash
# Local CI environment
cp ci/local.env.example ci/local.env
# Edit ci/local.env with your settings
```

## Packaging

### Package Formats

| Format | Files | Purpose |
|------|--|-----|
| RPM | `packages/*.yaml`, `packages/*.repo` | Red Hat/CentOS/Fedora |
| DEB | `packages/*.yaml` | Debian/Ubuntu |
| Docker | `docker/` | Container images |
| Service | `*.service` | systemd units |
| Init | `*.init` | SysV init scripts |

### Package Components

| Component | RPM Name | DEB Name |
|------|--|-----|
| Server | `clickhouse-server` | `clickhouse-server` |
| Client | `clickhouse-client` | `clickhouse-client` |
| Common | `clickhouse-common-static` | `clickhouse-common-static` |
| Keeper | `clickhouse-keeper` | `clickhouse-keeper` |
| Dev | `clickhouse-common-static-dbg` | `clickhouse-common-static-dbg` |
| Keeper Dev | `clickhouse-keeper-dbg` | `clickhouse-keeper-dbg` |

## Contributing

### Getting Started

1. Fork the repository
2. Clone your fork
3. Initialize submodules: `git submodule update --init --recursive`
4. Build: `cmake -B build && cmake --build build`
5. Run tests: `python3 tests/clickhouse-test`

### Code Style

- **Language**: C++23
- **Formatter**: `.clang-format`
- **Linter**: `.clang-tidy`
- **Clangd**: `.clangd` (language server config)
- **YAML**: `.yamllint`

### PR Process

1. Create feature branch
2. Write tests
3. Implement changes
4. Run CI
5. Submit PR with description

### Documentation Guidelines

- Update `CHANGELOG.md` for user-facing changes
- Add inline comments for complex logic
- Update `docs/` for new features
- Add test coverage for new functionality

## Key Source Files for Documentation

### Core Infrastructure

| File | Purpose |
|------|--|
| `CMakeLists.txt` | Root build config |
| `cmake/CMakeLists.txt` | CMake utilities |
| `cmake/tools.cmake` | Toolchain detection |
| `cmake/target.cmake` | Target config |
| `cmake/version.cmake` | Version generation |
| `cmake/git.cmake` | Git info |
| `base/base/defines.h` | Platform macros |
| `base/base/throwError.cpp` | Error handling |

### Build Artifacts

| Artifact | Location |
|------|--|
| `clickhouse` binary | `build/programs/` |
| Library | `build/base/` |
| Headers | `build/include/` |
| Config | `build/etc/clickhouse-server/` |

### Testing Infrastructure

| File | Purpose |
|------|--|
| `tests/clickhouse-test` | Main test runner |
| `tests/conftest.py` | Test fixtures |
| `tests/pytest.ini` | pytest config |
| `ci/praktika/*.py` | CI framework |
| `ci/workflows/*.py` | Workflow definitions |
| `ci/workflows/*.yml` | GitHub Actions |

## Common Development Tasks

### Adding a New Function

1. Create `src/Functions/FunctionX.cpp` and `FunctionX.h`
2. Implement `executeImpl()` method
3. Register in `FunctionFactory.cpp`
4. Add tests in `tests/`
5. Update `CHANGELOG.md`

### Adding a New Storage Engine

1. Create `src/Storages/StorageX.cpp` and `StorageX.h`
2. Implement `IMergeTreeDataPart` interface
3. Register with `StorageFactory`
4. Add distributed support if needed
5. Add tests
6. Update `CHANGELOG.md`

### Adding a New Data Type

1. Create `src/DataTypes/DataTypeX.cpp` and `DataTypeX.h`
2. Implement serialization/deserialization
3. Add column implementation in `src/Columns/`
4. Add function support
5. Add tests
6. Update `CHANGELOG.md`

### Adding a New Compression Codec

1. Create `src/Compression/CodecX.cpp` and `CodecX.h`
2. Implement `ICompressionCodec` interface
3. Register with `CompressionCodecFactory`
4. Add tests
5. Update `CHANGELOG.md`

## Build Artifacts

### Output Files

| File | Description |
|------|--|
| `clickhouse` | Main binary (symlinked) |
| `clickhouse-client` | Client binary |
| `clickhouse-server` | Server binary |
| `clickhouse-keeper` | Keeper binary |
| `clickhouse-compressor` | Compressor utility |
| `clickhouse-format` | Format utility |
| `clickhouse-local` | Local mode |
| `clickhouse-obfuscator` | Data obfuscator |
| `clickhouse-extract-from-config` | Config extractor |

### Configuration Files

| File | Purpose |
|------|--|
| `config.xml` | Server configuration |
| `users.xml` | User configuration |
| `metrika.xml` | External config |
| `prewiew.xml` | Preview config |

## Quick Reference

### Useful Commands

```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run client
./build/programs/clickhouse client

# Run server
./build/programs/clickhouse server

# Run keeper
./build/programs/clickhouse keeper

# Run local mode
./build/programs/clickhouse local --query "SELECT 1"

# Format code
clang-format -i src/**/*.cpp src/**/*.h

# Run tests
python3 tests/clickhouse-test
```

### Key Headers

| Header | Purpose |
|------|--|
| `Core/Block.h` | Block (data unit) |
| `Columns/Column.h` | Column interface |
| `DataTypes/IDataType.h` | Type interface |
| `Functions/IFunction.h` | Function interface |
| `Interpreters/Context.h` | Query context |
| `Processors/IProcessor.h` | Processor interface |
| `Storages/IStorage.h` | Storage interface |
| `IO/ReadBuffer.h` | I/O interface |
| `Disks/IDisk.h` | Disk interface |
| `Common/BaseSettings.h` | Settings system |
