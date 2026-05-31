# Mnemosyne DBMS — Startup Guide

## Prerequisites

### Compiler
- **MSVC** (Visual Studio 2022) — C++23 support via `/std:c++latest`
- **GCC 14+** (MinGW-w64 on Windows, or native on WSL/Linux)
- **Clang 18+** (via LLVM or WSL)

C++23 is required. The codebase uses:
- `std::span` (C++20)
- `std::format` / `std::source_location` (C++20/23)
- `std::expected` (C++23)
- `std::ranges` algorithms
- `#pragma once`

### Build System
- **CMake 3.28+** — for C++23 standard enforcement and modern `add_subdirectory` behavior

### Dependencies
- **None** — Mnemosyne is a zero-dependency project. All libraries are written in-house.
- Windows Sockets (winsock2/ws2tcpip) are used on Windows for the HTTP server.

---

## Quick Start (Windows / MSVC)

### 1. Install prerequisites

```powershell
# Install Visual Studio 2022 (Community is fine)
# During install, check: "C++ desktop development" workload

# Install CMake (or use winget)
winget install Kitware.CMake

# Verify
cmake --version   # should be >= 3.28
```

### 2. Configure

```powershell
cd C:\Users\tsuma.thomas\Documents\Mnemosyne

# Create build directory
mkdir build
cd build

# Configure with MSVC (auto-detects your installed VS version)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Or explicitly specify generator:
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
```

### 3. Build

```powershell
# Build via CMake
cmake --build . --config Release

# Or build just the server:
cmake --build . --config Release --target mnemosyne_server
```

The binary will be at: `build\bin\Release\mnemosyne_server.exe`

### 4. Run

```powershell
cd ..\bin\Release
.\mnemosyne_server.exe
```

You'll see:
```
========================================
  Mnemosyne DBMS v0.1.0
  Column-oriented analytical database
========================================
[Server] Starting on port 8123 (HTTP)
[Server] Use http://localhost:8123 to connect
```

### 5. Open the UI

Open your browser to: **http://localhost:8123**

You now have a full web-based query interface with:
- SQL query editor (with autocomplete)
- Database & table browser
- Schema viewer
- Query history
- Settings panel

---

## Quick Start (WSL2 / Linux)

### 1. Install prerequisites

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential cmake git

# Verify C++23 compiler
g++ --version   # 14+ required
# or
clang++ --version  # 18+ required
```

### 2. Configure & Build

```bash
cd ~/Documents/Mnemosyne
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### 3. Run

```bash
./bin/Release/mnemosyne_server
```

### 4. Open the UI

Open **http://localhost:8123** in your browser.

---

## Quick Start (MinGW-w64 on Windows)

### 1. Install prerequisites

```powershell
# Install via MSYS2 (https://www.msys2.org/)
# In MSYS2 MinGW64 shell:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
```

### 2. Configure & Build

```bash
cd /c/Users/tsuma.thomas/Documents/Mnemosyne
mkdir -p build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### 3. Run

```bash
./bin/Release/mnemosyne_server.exe
```

---

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_TESTS` | `ON` | Build unit test suite |
| `ENABLE_BENCHMARK` | `ON` | Build benchmark targets |
| `ENABLE_SIMD` | `ON` | Enable SIMD optimizations (AVX2/AVX-512) |
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug`, `Release`, `RelWithDebInfo` |

Example:
```powershell
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF -DENABLE_BENCHMARK=OFF
```

---

## Making Queries

### Via the Web UI (recommended)

1. Open **http://localhost:8123**
2. Type SQL in the query editor: `SELECT 1 + 1 AS result`
3. Click **▶ Run** or press **Ctrl+Enter**
4. Results appear in the table view below

### Via curl

```bash
# Simple query
curl "http://localhost:8123/query?query=SELECT+1"

# With format
curl "http://localhost:8123/query?query=SELECT+1&format=JSON"

# POST query
curl -X POST "http://localhost:8123/query" -d "SELECT 1"
```

### Available HTTP Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/ping` | Health check |
| GET | `/query?query=...` | Execute SQL (URL-encoded) |
| GET | `/databases` | List all databases |
| GET | `/tables/{db}` | List tables in a database |
| GET | `/schema/{db}/{table}` | Get table schema |
| GET | `/settings` | View server settings |
| GET | `/metrics` | Server metrics |
| POST | `/query` | Execute SQL (body) |

---

## Troubleshooting

### "CMake 3.28 required" error
```powershell
winget install Kitware.CMake
# or download from https://cmake.org/download/
```

### "C++23 not supported" error (MSVC)
Make sure Visual Studio 2022 is up to date. C++23 support was improved in VS 2022 17.8+.
If needed, enable C++23 explicitly:
```powershell
cmake .. -DCMAKE_CXX_FLAGS="/std:c++latest"
```

### "C++23 not supported" error (GCC/Clang)
You need a newer compiler:
```bash
# GCC 14+
sudo apt install gcc-14 g++-14
cmake .. -DCMAKE_CXX_COMPILER=g++-14

# Clang 18+
sudo apt install clang-18
cmake .. -DCMAKE_CXX_COMPILER=clang++-18
```

### "Port 8123 already in use"
Another process is using the port. Either:
- Kill the process: `netstat -ano | findstr :8123` → `taskkill /PID <pid> /F`
- Or edit `programs/server/main.cpp` to change the port

### "Winsock not initialized" (Windows)
Ensure you're building with the correct Windows SDK. The HTTP server uses the standard Windows Sockets API which is included with Visual Studio.

### Web UI not loading
The `public/index.html` is served as a static file by the HTTP server. If it doesn't appear:
- Check that the file exists at `C:\Users\tsuma.thomas\Documents\Mnemosyne\public\index.html`
- The server serves from the `public/` directory relative to the working directory

---

## Project Structure

```
Mnemosyne/
├── CMakeLists.txt          ← Top-level build file
├── STARTUP.md              ← This file
├── src/                    ← Source code (14 layers)
│   ├── Core/               ← Block, Column, Field, Series
│   ├── Columns/            ← ColumnVector, ColumnString, ColumnArray
│   ├── DataTypes/          ← UInt64, String, Date, Factory
│   ├── Parsers/            ← Lexer, Parser, AST
│   ├── Analyzer/           ← AST validation & rewriting
│   ├── Planner/            ← Execution plan DAG
│   ├── Functions/          ← Built-in functions (arithmetic, comparison)
│   ├── AggregateFunctions/ ← SUM, COUNT, AVG, MIN, MAX
│   ├── Interpreters/       ← Context, QueryExecutor, BlockInterpreter
│   ├── Processors/         ← Pipeline processors (Source, Filter, Project, etc.)
│   ├── Storages/           ← MemoryStorage, FileStorage, Table
│   ├── Databases/          ← Database, DatabaseManager
│   ├── Disks/              ← LocalDisk, S3Disk
│   ├── IO/                 ← Codecs (Binary, CSV, Parquet, LZ4, ZSTD)
│   ├── Server/             ← HTTPServer, HTTPHandler, TCPServer
│   ├── Coordination/       ← Cluster coordination
│   ├── Backups/            ← Backup/restore
│   ├── Loggers/            ← Logging framework
│   └── Common/             ← Settings, Exceptions, ThreadPool
├── programs/
│   ├── server/             ← mnemosyne_server executable
│   └── client/             ← CLI client
├── tests/                  ← Unit tests
├── benchmark/              ← Benchmarks
└── public/                 ← Web UI (index.html)
```

---

## Architecture Overview

Mnemosyne follows a layered architecture inspired by ClickHouse:

```
HTTP/TCP Client
    ↓
HTTPHandler / TCPServer
    ↓
Context (per-query state)
    ↓
Parser (Lexer → AST)
    ↓
Analyzer (validation & rewriting)
    ↓
Planner (DAG of PlanNodes)
    ↓
BlockInterpreter (pipeline construction)
    ↓
Processors (Source → Filter → Project → Aggregate → ...)
    ↓
Storage Engine (Memory / File / S3)
```

Each layer is independently testable and replaceable. The columnar storage format enables efficient analytical queries with minimal memory footprint.





Remove-Item -Recurse -Force CMakeCache.txt, CMakeFiles; cmake -G "Visual Studio 16 2019" -A x64 -DCMAKE_CXX_STANDARD=20 ..; cmake --build build --config Release