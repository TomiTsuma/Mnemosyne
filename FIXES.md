````markdown
# Mnemosyne Build Errors - TODO List

## 🚨 Priority 1: Fix C++20 / Standard Library Configuration

### Issue: `std::span` not recognized
**Symptoms**
- `error C2039: 'span': is not a member of 'std'`
- `error C2061: syntax error: identifier 'span'`
- Errors originating from:
  - `src/Core/column.h`
  - `src/DataTypes/data_type.h`
  - `src/Disks/disk.h`
  - `src/Storages/file_storage.cpp`

**TODO**
- [ ] Verify project is compiling with C++20 (`/std:c++20`)
- [ ] Ensure all files using `std::span` include:
  ```cpp
  #include <span>
````

* [ ] Verify CMake contains:

  ```cmake
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  ```
* [ ] Verify Visual Studio project is using C++20 language standard
* [ ] Rebuild after fixing `std::span` issues before investigating downstream errors

---

## 🚨 Priority 2: Fix Missing `<mutex>` Support

### Issue: `std::mutex` not recognized

**Symptoms**

* `error C2039: 'mutex': is not a member of 'std'`
* `error C3646: 'mutex_' unknown override specifier`
* `error C2065: 'mutex_' undeclared identifier`
* `std::lock_guard` template deduction failures

**Files**

* `src/Loggers/logger.h`
* `src/Loggers/logger.cpp`

**TODO**

* [ ] Add:

  ```cpp
  #include <mutex>
  ```
* [ ] Verify logger class contains:

  ```cpp
  std::mutex mutex_;
  ```
* [ ] Recompile logger module after fixing include issues

---

# Core Interface Mismatches

## Issue: IColumn interface missing methods

### Symptoms

* `get_at()` not found
* `get_data_type()` not found

**Files**

* `src/Core/column.h`
* `memory_storage.cpp`
* `dictionary_storage.cpp`
* `file_storage.cpp`

**TODO**

* [ ] Verify `IColumn` interface definition
* [ ] Add or restore:

  ```cpp
  virtual Field get_at(size_t index) const = 0;
  virtual IDataTypePtr get_data_type() const = 0;
  ```
* [ ] Ensure all concrete column classes implement these methods

---

## Issue: IDataType interface missing methods

### Symptoms

* `sizeof_impl is not a member of IDataType`

**Files**

* `src/DataTypes/data_type.h`
* `memory_storage.cpp`

**TODO**

* [ ] Verify intended datatype API
* [ ] Add missing method or replace call with current equivalent
* [ ] Update all datatype implementations

---

# Storage Layer Refactor Issues

## Issue: Undefined `columns`

### Symptoms

* `columns undeclared identifier`
* `columns is not a class or namespace name`

**Files**

* `memory_storage.cpp`
* `dictionary_storage.cpp`
* `file_storage.cpp`

**TODO**

* [ ] Determine whether:

  * `columns`
  * `columns_`
  * `get_columns()`
    was intended
* [ ] Fix all incorrect references
* [ ] Verify storage ownership model

---

## Issue: Missing storage metadata members

### Symptoms

* `column_names_ undeclared`
* `column_types_ undeclared`
* `empty_ undeclared`

**Files**

* `dictionary_storage.cpp`
* `file_storage.cpp`

**TODO**

* [ ] Verify storage class definitions
* [ ] Restore missing member variables
* [ ] Remove obsolete references if design changed

---

## Issue: Missing storage methods

### Symptoms

* `add_column not found`
* `set_columns not found`

**Files**

* `dictionary_storage.cpp`
* `file_storage.cpp`

**TODO**

* [ ] Verify storage API
* [ ] Restore methods or replace with new API
* [ ] Update callers accordingly

---

## Issue: Wrong add_column signature

### Symptoms

* `MemoryStorage::add_column function does not take 1 arguments`

**TODO**

* [ ] Check declaration in `memory_storage.h`
* [ ] Check call sites in `memory_storage.cpp`
* [ ] Align signatures

---

# Dictionary Storage Problems

## Issue: std::visit failures

### Symptoms

* `std::visit no matching overloaded function found`

**Files**

* `dictionary_storage.cpp`

**TODO**

* [ ] Verify variant types
* [ ] Verify visitor lambda return types
* [ ] Ensure variants are valid after fixing earlier type errors

---

## Issue: Field type missing

### Symptoms

* `Field undeclared identifier`
* `syntax error: identifier 'Field'`

**Files**

* `dictionary_storage.cpp`
* `file_storage.cpp`

**TODO**

* [ ] Locate actual Field definition
* [ ] Add correct include
* [ ] Verify namespace:

  ```cpp
  mnemo::core::Field
  ```
* [ ] Update all references

---

## Issue: DictionaryStorage method signature mismatch

### Symptoms

* `put(std::string)` overload not found
* `get()` redefinition
* `optional<Field>` errors

**TODO**

* [ ] Compare implementation against header
* [ ] Synchronize method signatures
* [ ] Verify return types

---

# File Storage Problems

## Issue: Disk API mismatch

### Symptoms

* `IDisk::write function does not take 2 arguments`
* `std::span` usage in write call

**Files**

* `file_storage.cpp`
* `disk.h`

**TODO**

* [ ] Verify current disk write API
* [ ] Update FileStorage calls
* [ ] Rework buffer handling

---

## Issue: LocalFileDisk factory missing

### Symptoms

* `LocalFileDisk is not a type`
* `create() not found`

**TODO**

* [ ] Verify LocalFileDisk implementation exists
* [ ] Verify namespace
* [ ] Restore factory method or update caller

---

## Issue: Invalid shared_ptr assignment

### Symptoms

* Cannot assign:

  ```cpp
  shared_ptr<FileStorage>
  ```

  to:

  ```cpp
  shared_ptr<IDisk>
  ```

**TODO**

* [ ] Inspect line 97 in `file_storage.cpp`
* [ ] Verify intended object type
* [ ] Correct factory return type

---

# Initialization Problems

## Issue: Variables used before initialization

### Symptoms

* `cloned cannot be used before initialized`
* `col cannot be used before initialized`
* `val cannot be used before initialized`

**Files**

* `memory_storage.cpp`
* `dictionary_storage.cpp`
* `file_storage.cpp`

**TODO**

* [ ] Trace variable declarations
* [ ] Fix construction order
* [ ] Resolve upstream type errors first

---

# Syntax Errors Likely Caused by Earlier Failures

### Symptoms

* `type '<error type>' unexpected`
* `type 'double' unexpected`
* `syntax error ':'`
* `missing ';' before '{'`
* `built-in subscript operator expects a single expression`

**TODO**

* [ ] Ignore temporarily
* [ ] Rebuild after fixing:

  * C++20 configuration
  * std::span issues
  * missing interfaces
  * missing members
* [ ] Re-evaluate remaining errors

---

# Build Order Recommendation

### Phase 1

* [ ] Fix C++20 configuration
* [ ] Fix `<span>`
* [ ] Fix `<mutex>`

### Phase 2

* [ ] Fix `IColumn`
* [ ] Fix `IDataType`
* [ ] Fix `Field`

### Phase 3

* [ ] Fix Storage APIs
* [ ] Fix member variables
* [ ] Fix add_column/set_columns

### Phase 4

* [ ] Fix DictionaryStorage
* [ ] Fix FileStorage
* [ ] Fix Disk APIs

### Phase 5

* [ ] Resolve remaining syntax errors
* [ ] Full rebuild
* [ ] Run unit tests

```

From this log alone, roughly **70–80% of the downstream errors appear to be cascading from three root causes**:

1. `std::span` / C++20 not configured correctly.
2. Missing or changed interfaces (`IColumn`, `IDataType`, `Field`).
3. Storage classes (`FileStorage`, `DictionaryStorage`, `MemoryStorage`) being out of sync with their headers.

Append the next build log and I’ll continue expanding this TODO list while deduplicating repeated errors.
```
