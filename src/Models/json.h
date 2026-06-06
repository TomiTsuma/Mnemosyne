// src/Models/json.h — minimal JSON value (parse + serialize)
// Mnemosyne: A column-oriented analytical DBMS
//
// The repo has no JSON dependency; the MODEL layer needs to write spec.json,
// read result.json, and persist catalog.json. This is a small, dependency-free
// JSON implementation sufficient for those structured documents.

#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mnemo::models::json {

class Value;
using Array = std::vector<Value>;
// Ordered object so serialized documents are stable/readable.
using Object = std::vector<std::pair<std::string, Value>>;

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    Value() : type_(Type::Null) {}
    Value(std::nullptr_t) : type_(Type::Null) {}
    Value(bool b) : type_(Type::Bool), bool_(b) {}
    Value(int v) : type_(Type::Number), num_(static_cast<double>(v)) {}
    Value(int64_t v) : type_(Type::Number), num_(static_cast<double>(v)) {}
    Value(uint64_t v) : type_(Type::Number), num_(static_cast<double>(v)) {}
    Value(double v) : type_(Type::Number), num_(v) {}
    Value(const char* s) : type_(Type::String), str_(s) {}
    Value(std::string s) : type_(Type::String), str_(std::move(s)) {}
    Value(Array a) : type_(Type::Array), arr_(std::move(a)) {}
    Value(Object o) : type_(Type::Object), obj_(std::move(o)) {}

    [[nodiscard]] Type type() const { return type_; }
    [[nodiscard]] bool is_null() const { return type_ == Type::Null; }
    [[nodiscard]] bool is_object() const { return type_ == Type::Object; }
    [[nodiscard]] bool is_array() const { return type_ == Type::Array; }

    [[nodiscard]] bool as_bool(bool def = false) const {
        if (type_ == Type::Bool) return bool_;
        if (type_ == Type::Number) return num_ != 0.0;
        return def;
    }
    [[nodiscard]] double as_number(double def = 0.0) const {
        if (type_ == Type::Number) return num_;
        if (type_ == Type::String) { try { return std::stod(str_); } catch (...) { return def; } }
        return def;
    }
    [[nodiscard]] int64_t as_int(int64_t def = 0) const {
        return type_ == Type::Number ? static_cast<int64_t>(num_) : def;
    }
    [[nodiscard]] const std::string& as_string() const {
        static const std::string empty;
        return type_ == Type::String ? str_ : empty;
    }
    [[nodiscard]] const Array& as_array() const {
        static const Array empty;
        return type_ == Type::Array ? arr_ : empty;
    }
    [[nodiscard]] const Object& as_object() const {
        static const Object empty;
        return type_ == Type::Object ? obj_ : empty;
    }

    // Object field access (returns null Value if missing).
    [[nodiscard]] const Value& operator[](std::string_view key) const {
        static const Value null_value;
        if (type_ != Type::Object) return null_value;
        for (const auto& [k, v] : obj_) if (k == key) return v;
        return null_value;
    }
    [[nodiscard]] bool contains(std::string_view key) const {
        if (type_ != Type::Object) return false;
        for (const auto& [k, v] : obj_) if (k == key) return true;
        return false;
    }

    // ── Serialization ──
    [[nodiscard]] std::string dump(int indent = 2) const {
        std::ostringstream os;
        dump_to(os, indent, 0);
        return os.str();
    }

    // ── Parsing ──
    static Value parse(std::string_view text) {
        size_t pos = 0;
        skip_ws(text, pos);
        Value v = parse_value(text, pos);
        skip_ws(text, pos);
        return v;
    }

private:
    Type type_;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    Array arr_;
    Object obj_;

    void dump_to(std::ostringstream& os, int indent, int depth) const {
        const std::string pad(static_cast<size_t>(indent) * (depth + 1), ' ');
        const std::string pad_close(static_cast<size_t>(indent) * depth, ' ');
        switch (type_) {
            case Type::Null: os << "null"; break;
            case Type::Bool: os << (bool_ ? "true" : "false"); break;
            case Type::Number: {
                if (num_ == static_cast<int64_t>(num_) &&
                    num_ < 9e15 && num_ > -9e15) {
                    os << static_cast<int64_t>(num_);
                } else {
                    std::ostringstream tmp;
                    tmp.precision(12);
                    tmp << num_;
                    os << tmp.str();
                }
                break;
            }
            case Type::String: dump_string(os, str_); break;
            case Type::Array: {
                if (arr_.empty()) { os << "[]"; break; }
                os << "[\n";
                for (size_t i = 0; i < arr_.size(); ++i) {
                    os << pad;
                    arr_[i].dump_to(os, indent, depth + 1);
                    if (i + 1 < arr_.size()) os << ",";
                    os << "\n";
                }
                os << pad_close << "]";
                break;
            }
            case Type::Object: {
                if (obj_.empty()) { os << "{}"; break; }
                os << "{\n";
                for (size_t i = 0; i < obj_.size(); ++i) {
                    os << pad;
                    dump_string(os, obj_[i].first);
                    os << ": ";
                    obj_[i].second.dump_to(os, indent, depth + 1);
                    if (i + 1 < obj_.size()) os << ",";
                    os << "\n";
                }
                os << pad_close << "}";
                break;
            }
        }
    }

    static void dump_string(std::ostringstream& os, const std::string& s) {
        os << '"';
        for (char c : s) {
            switch (c) {
                case '"':  os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\n': os << "\\n"; break;
                case '\r': os << "\\r"; break;
                case '\t': os << "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        os << buf;
                    } else {
                        os << c;
                    }
            }
        }
        os << '"';
    }

    static void skip_ws(std::string_view t, size_t& p) {
        while (p < t.size() &&
               (t[p] == ' ' || t[p] == '\t' || t[p] == '\n' || t[p] == '\r'))
            ++p;
    }

    static Value parse_value(std::string_view t, size_t& p) {
        skip_ws(t, p);
        if (p >= t.size()) throw std::runtime_error("json: unexpected end");
        char c = t[p];
        if (c == '{') return parse_object(t, p);
        if (c == '[') return parse_array(t, p);
        if (c == '"') return Value{parse_string(t, p)};
        if (c == 't' || c == 'f') return parse_bool(t, p);
        if (c == 'n') { p += 4; return Value{}; }
        return parse_number(t, p);
    }

    static Value parse_object(std::string_view t, size_t& p) {
        Object obj;
        ++p; // {
        skip_ws(t, p);
        if (p < t.size() && t[p] == '}') { ++p; return Value{obj}; }
        while (p < t.size()) {
            skip_ws(t, p);
            std::string key = parse_string(t, p);
            skip_ws(t, p);
            if (p >= t.size() || t[p] != ':') throw std::runtime_error("json: expected ':'");
            ++p;
            Value v = parse_value(t, p);
            obj.emplace_back(std::move(key), std::move(v));
            skip_ws(t, p);
            if (p < t.size() && t[p] == ',') { ++p; continue; }
            if (p < t.size() && t[p] == '}') { ++p; break; }
            throw std::runtime_error("json: expected ',' or '}'");
        }
        return Value{obj};
    }

    static Value parse_array(std::string_view t, size_t& p) {
        Array arr;
        ++p; // [
        skip_ws(t, p);
        if (p < t.size() && t[p] == ']') { ++p; return Value{arr}; }
        while (p < t.size()) {
            arr.push_back(parse_value(t, p));
            skip_ws(t, p);
            if (p < t.size() && t[p] == ',') { ++p; continue; }
            if (p < t.size() && t[p] == ']') { ++p; break; }
            throw std::runtime_error("json: expected ',' or ']'");
        }
        return Value{arr};
    }

    static std::string parse_string(std::string_view t, size_t& p) {
        if (p >= t.size() || t[p] != '"') throw std::runtime_error("json: expected string");
        ++p;
        std::string out;
        while (p < t.size()) {
            char c = t[p++];
            if (c == '"') return out;
            if (c == '\\') {
                if (p >= t.size()) break;
                char e = t[p++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        if (p + 4 <= t.size()) {
                            int code = std::stoi(std::string{t.substr(p, 4)}, nullptr, 16);
                            p += 4;
                            if (code < 0x80) out += static_cast<char>(code);
                            else if (code < 0x800) {
                                out += static_cast<char>(0xC0 | (code >> 6));
                                out += static_cast<char>(0x80 | (code & 0x3F));
                            } else {
                                out += static_cast<char>(0xE0 | (code >> 12));
                                out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                                out += static_cast<char>(0x80 | (code & 0x3F));
                            }
                        }
                        break;
                    }
                    default: out += e;
                }
            } else {
                out += c;
            }
        }
        throw std::runtime_error("json: unterminated string");
    }

    static Value parse_bool(std::string_view t, size_t& p) {
        if (t.substr(p, 4) == "true") { p += 4; return Value{true}; }
        if (t.substr(p, 5) == "false") { p += 5; return Value{false}; }
        throw std::runtime_error("json: invalid literal");
    }

    static Value parse_number(std::string_view t, size_t& p) {
        size_t start = p;
        while (p < t.size() &&
               (std::isdigit(static_cast<unsigned char>(t[p])) || t[p] == '-' ||
                t[p] == '+' || t[p] == '.' || t[p] == 'e' || t[p] == 'E'))
            ++p;
        if (p == start) throw std::runtime_error("json: invalid number");
        return Value{std::stod(std::string{t.substr(start, p - start)})};
    }
};

} // namespace mnemo::models::json
