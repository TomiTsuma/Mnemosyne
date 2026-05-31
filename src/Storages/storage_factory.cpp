// src/Storages/storage_factory.cpp — Storage factory implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "Storages/storage_factory.h"
#include "Storages/memory_storage.h"
#include "Storages/file_storage.h"
#include "Storages/dictionary_storage.h"
#include "Common/exceptions.h"

namespace mnesso::storages {

auto StorageFactory::instance() -> StorageFactory& {
    static StorageFactory inst;
    return inst;
}

void StorageFactory::register_engine(std::string name,
                                     std::function<std::shared_ptr<IStorage>(std::string)> creator) {
    registry_[std::move(name)] = std::move(creator);
}

auto StorageFactory::create(std::string name, std::string engine) -> std::shared_ptr<IStorage> {
    auto it = registry_.find(engine);
    if (it == registry_.end()) {
        throw common::Exception{
            "StorageFactory: unknown storage engine: " + engine,
            static_cast<int>(common::ErrorCode::UNKNOWN_DATABASE)};
    }
    return it->second(std::move(name));
}

auto StorageFactory::has(std::string_view engine) const -> bool {
    return registry_.find(std::string(engine)) != registry_.end();
}

auto StorageFactory::names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(registry_.size());
    for (auto& [name, _] : registry_) {
        names.push_back(name);
    }
    return names;
}

// Register builtin engines
namespace {
    struct RegisterBuiltinEngines {
        RegisterBuiltinEngines() {
            auto& factory = StorageFactory::instance();
            factory.register_engine("Memory",
                [](std::string name) {
                    return storages::MemoryStorage::create(std::move(name));
                });
            factory.register_engine("File",
                [](std::string name) {
                    return storages::FileStorage::create(std::move(name), "");
                });
            factory.register_engine("Dictionary",
                [](std::string name) {
                    return storages::DictionaryStorage::create(std::move(name));
                });
        }
    };
    static RegisterBuiltinEngines init;
}

} // namespace mnesso::storages
