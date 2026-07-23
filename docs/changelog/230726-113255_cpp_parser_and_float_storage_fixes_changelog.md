---
description: "Two C++ bug fixes discovered during ClickHouse-to-Mnemosyne benchmark migration: unary minus in parser and float truncation in ColumnVector."
sidebar_label: "Parser & Float Storage Fixes"
sidebar_position: 1
slug: /changelog/230726-cpp-parser-float-fixes
title: "C++ Parser Unary Minus and Float Storage Fixes"
doc_type: changelog
---

# C++ Parser Unary Minus and Float Storage Fixes {#cpp-parser-float-fixes}

**Date:** 2026-07-23

## Summary {#summary}

Two pre-existing bugs were discovered and fixed while running a ClickHouse-to-Mnemosyne data migration benchmark. Both bugs silently corrupted data: the parser failed on negative number literals, and the column storage truncated floating-point values to integers.

---

## Bug 1: `parse_value_row` rejected negative number literals {#bug-1-unary-minus}

### File {#bug-1-file}

`src/Parsers/parser.cpp` — function `parse_value_row`

### Root cause {#bug-1-root-cause}

The `INSERT INTO ... VALUES (...)` row parser consumed exactly one token per column value. Negative number literals such as `-1.286389` tokenize as two tokens: `TokenType::Minus` followed by a `FloatLiteral`. The parser consumed only the `Minus` token as the value and then found a `FloatLiteral` where it expected `)` or `,`, throwing `Parser: expected )`.

### Fix {#bug-1-fix}

Added a `read_one()` lambda inside `parse_value_row` that detects `TokenType::Minus` and concatenates its value with the following token before returning the combined string.

```cpp
auto read_one = [&]() -> std::string {
    if (current_.type == TokenType::Minus) {
        consume(); // consume -
        std::string val = "-" + current_.value;
        consume();
        return val;
    }
    std::string val = current_.value;
    consume();
    return val;
};
```

### Impact {#bug-1-impact}

Any `INSERT` statement with negative numeric literals (coordinates, signed amounts, temperatures, etc.) would fail with `Exception: Parser: expected )`. Affected tables: `dim_geo` (negative lat/lng), `fact_bank_transaction`, `fact_gl_entry`, `fact_social_account_daily_metrics`.

---

## Bug 2: `ColumnVector<T>` truncated `Float64` values to integers {#bug-2-float-truncation}

### File {#bug-2-file}

`src/Columns/column_vector.h` — methods `insert`, `insert_range`, `set`

### Root cause {#bug-2-root-cause}

`insert()` called `Field::as_int64()` first regardless of the column type `T`. For a `Field` holding a `double` (e.g. `3.14159`), `Field::as_int64()` in `src/Core/field.cpp` at lines 57–58 performs `static_cast<int64_t>(*v)`, which truncates the fractional part and returns a non-null optional. The truncated integer was then `static_cast<T>`-ed and pushed to the `double` column, storing `3` instead of `3.14159`.

### Fix {#bug-2-fix}

For columns where `std::is_floating_point_v<T>` is `true`, check `as_float64()` first. Fall back to `as_int64()` only if the float accessor returns nullopt (e.g. for integer-valued `Field`s). The same guard was applied to `insert_range` (now delegates to `insert`) and `set`.

```cpp
auto insert(const Field& value) -> size_t override {
    if (value.type() == core::FieldType::Null) {
        data_.push_back(T{});
    } else if constexpr (std::is_floating_point_v<T>) {
        auto fv = value.as_float64();
        if (fv) {
            data_.push_back(static_cast<T>(*fv));
        } else {
            auto v = value.as_int64();
            data_.push_back(v ? static_cast<T>(*v) : T{});
        }
    } else {
        auto v = value.as_int64();
        if (v) {
            data_.push_back(static_cast<T>(*v));
        } else {
            auto fv = value.as_float64();
            data_.push_back(fv ? static_cast<T>(*fv) : T{});
        }
    }
    size_ = data_.size();
    return size_ - 1;
}
```

### Impact {#bug-2-impact}

All `Float32` and `Float64` columns silently stored the integer part of every inserted value. Any query involving `SUM`, `AVG`, or exact float comparisons on these columns would return incorrect results. Affected tables: all fact tables with decimal/float columns (amounts, rates, scores).

---

## Benchmark validation {#benchmark-validation}

Both fixes were validated by a full migration from ClickHouse (59 tables, ~8 000 rows) and a 19-query benchmark:

- **19/19 COUNT(\*) queries matched** between ClickHouse and Mnemosyne after fixes.
- Before Bug 1 fix: 4 tables failed to insert (0 rows loaded).
- Before Bug 2 fix: all numeric aggregates on float columns were wrong.

The benchmark tooling lives in `clickhouse_mnemo_benchmark/` (gitignored).
