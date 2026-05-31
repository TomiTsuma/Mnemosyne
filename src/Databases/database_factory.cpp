// src/Databases/database_factory.cpp — Database factory implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "database_factory.h"
#include "database_memory.h"
#include "Common/exceptions.h"

namespace mnesso::databases {

auto& DatabaseFactory::instance() {
    static DatabaseFactory inst;
    return inst;
}

void DatabaseFactory::register_engine(std::string name,
                                      std::function<std::shared_ptr<IDatabase>()> creator) {
    registry_[std::move(name)] = std::move(creator);
}

auto DatabaseFactory::get_engine(std::string_view name) -> std::shared_ptr<IDatabase> {
    auto it = registry_.find(std::string{name});
    if (it == registry_.end()) {
        throw common::Exception{
            "DatabaseFactory: unknown database engine: " + std::string{name},
            static_cast<int>(common::ErrorCode::UNKNOWN_DATABASE)};
    }
    return it->second();
}

auto DatabaseFactory::names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(registry_.size());
    for (auto& [name, _] : registry_) {
        names.push_back(name);
    }
    return names;
}

bool DatabaseFactory::has(std::string_view name) const {
    return registry_.find(std::string{name}) != registry_.end();
}

// Register builtin engines
namespace {
    struct RegisterBuiltinEngines {
        RegisterBuiltinEngines() {
            auto& factory = DatabaseFactory::instance();
            factory.register_engine(BuiltinEngines::MEMORY,
                []() {
                    return databases::DatabaseMemory::create("default", "");
                });
            factory.register_engine(BuiltinEngines::FILE,
                []() {
                    return databases::DatabaseMemory::create("file", "");
                });
        }
    };
    static RegisterBuiltinEngines init;
}

} // namespace mnesso::databases
