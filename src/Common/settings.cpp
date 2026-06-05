// src/Common/settings.cpp — Configuration settings implementation
// Mnemosyne: A column-oriented analytical DBMS

#include "settings.h"

namespace mnemo::common {

Setting::Setting() : value_{false}, has_default_{false} {}

Setting::Setting(SettingValueType default_value)
    : value_(std::move(default_value)), has_default_(true) {}

auto Setting::get() const -> SettingValueType {
    return value_;
}

void Setting::set(SettingValueType value) {
    value_ = std::move(value);
}

auto Setting::name() const -> std::string { return name_; }
auto Setting::help() const -> std::string { return help_; }
auto Setting::has_default() const -> bool { return has_default_; }

Settings::Settings() = default;

void Settings::register_setting(std::string name,
                                SettingValueType default_value,
                                std::string help) {
    Setting setting{std::move(default_value)};
    setting.set_name(std::move(name));
    setting.set_help(std::move(help));
    settings_[setting.name()] = std::move(setting);
}

auto Settings::get(std::string_view name) -> std::optional<SettingValueType> {
    auto it = settings_.find(static_cast<std::string>(name));
    if (it == settings_.end()) return std::nullopt;
    return it->second.get();
}

void Settings::set(std::string_view name, SettingValueType value) {
    auto it = settings_.find(static_cast<std::string>(name));
    if (it == settings_.end()) return;
    it->second.set(std::move(value));
}

auto Settings::names() const -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(settings_.size());
    for (auto&& [k, _] : settings_) result.push_back(k);
    return result;
}

bool Settings::has(std::string_view name) const {
    return settings_.count(static_cast<std::string>(name)) > 0;
}

} // namespace mnemo::common
