// src/Analyzer/analyzer_passes.h — Analysis and transformation passes
// Mnemosyne: A column-oriented analytical DBMS

#pragma once

#include "ast.h"
#include <memory>
#include <vector>
#include <string>

namespace mnesso::analyzer {

// ── Pass — a transformation that modifies the AST ──
class Pass {
public:
    virtual ~Pass() = default;
    virtual auto name() const -> std::string = 0;
    virtual void execute(std::shared_ptr<parsers::ASTNode>& ast) = 0;
};

// ── Column resolution pass ──
class ColumnResolverPass final : public Pass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "ColumnResolver"; }
    void execute(std::shared_ptr<parsers::ASTNode>& ast) override;
};

// ── Function resolution pass ──
class FunctionResolverPass final : public Pass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "FunctionResolver"; }
    void execute(std::shared_ptr<parsers::ASTNode>& ast) override;
};

// ── Type inference pass ──
class TypeInferencePass final : public Pass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "TypeInference"; }
    void execute(std::shared_ptr<parsers::ASTNode>& ast) override;
};

// ── Subquery flattening pass ──
class SubqueryFlatteningPass final : public Pass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "SubqueryFlattening"; }
    void execute(std::shared_ptr<parsers::ASTNode>& ast) override;
};

// ── Constant folding pass ──
class ConstantFoldingPass final : public Pass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "ConstantFolding"; }
    void execute(std::shared_ptr<parsers::ASTNode>& ast) override;
};

// ── Column pruning pass ──
class ColumnPruningPass final : public Pass {
public:
    [[nodiscard]] auto name() const -> std::string override { return "ColumnPruning"; }
    void execute(std::shared_ptr<parsers::ASTNode>& ast) override;
};

// ── Run all passes in order ──
void run_all_passes(std::shared_ptr<parsers::ASTNode>& ast);

} // namespace mnesso::analyzer
