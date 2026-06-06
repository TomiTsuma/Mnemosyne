// src/Parsers/lexer.cpp — SQL lexer for Mnemosyne
// Mnemosyne: A column-oriented analytical DBMS

#include "Parsers/lexer.h"
#include "Common/exceptions.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace mnemo::parsers {

// ── Lexer ──

Lexer::Lexer(std::string source, std::string file)
    : source_{std::move(source)}, file_{std::move(file)} {}

auto Lexer::next() -> Token {
    skip_whitespace();
    if (pos_ >= source_.size()) {
        return Token{TokenType::EndOfQuery, "", {file_, line_, col_}};
    }

    char c = source_[pos_];

    // Single-character tokens
    switch (c) {
        case '(': {
            auto token = make_token(TokenType::LParen, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case ')': {
            auto token = make_token(TokenType::RParen, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case ',': {
            auto token = make_token(TokenType::Comma, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case '.': {
            if (pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1])) {
                return read_number();
            }
            auto token = make_token(TokenType::Dot, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case ';': {
            auto token = make_token(TokenType::Semicolon, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case ':': {
            auto token = make_token(TokenType::Colon, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case '*': {
            auto token = make_token(TokenType::Star, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case '+': {
            auto token = make_token(TokenType::Plus, std::string(1, c));
            advance_pos(1);
            return token;
        }
        case '%': {
            auto token = make_token(TokenType::Percent, std::string(1, c));
            advance_pos(1);
            return token;
        }
    }

    // - or ->
    if (c == '-') {
        size_t next = pos_ + 1;
        if (next < source_.size() && source_[next] == '>') {
            advance_pos(2);
            return make_token(TokenType::Concat, "->");
        }
        auto token = make_token(TokenType::Minus, std::string(1, c));
        advance_pos(1);
        return token;
    }

    // /
    if (c == '/') {
        // Check for // comment
        if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '/') {
            skip_line_comment();
            return next();
        }
        // Check for /* comment */
        if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '*') {
            skip_block_comment();
            return next();
        }
        auto token = make_token(TokenType::Slash, std::string(1, c));
        advance_pos(1);
        return token;
    }

    // = or != or <> or <= or >=
    if (c == '=') {
        auto token = make_token(TokenType::Eq, std::string(1, c));
        advance_pos(1);
        return token;
    }
    if (c == '!') {
        if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
            advance_pos(2);
            return make_token(TokenType::Ne, "!=");
        }
        auto token = make_token(TokenType::Ne, std::string(1, c));
        advance_pos(1);
        return token;
    }
    if (c == '<') {
        if (pos_ + 1 < source_.size()) {
            if (source_[pos_ + 1] == '=') {
                advance_pos(2);
                return make_token(TokenType::Le, "<=");
            }
            if (source_[pos_ + 1] == '>') {
                advance_pos(2);
                return make_token(TokenType::Ne, "<>");
            }
        }
        auto token = make_token(TokenType::Lt, std::string(1, c));
        advance_pos(1);
        return token;
    }
    if (c == '>') {
        if (pos_ + 1 < source_.size() && source_[pos_ + 1] == '=') {
            advance_pos(2);
            return make_token(TokenType::Ge, ">=");
        }
        auto token = make_token(TokenType::Gt, std::string(1, c));
        advance_pos(1);
        return token;
    }

    // Numbers (integers and floats)
    if (std::isdigit(c) || (c == '.' && pos_ + 1 < source_.size() && std::isdigit(source_[pos_ + 1]))) {
        return read_number();
    }

    // Strings
    if (c == '\'') {
        return read_string();
    }

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return read_identifier();
    }

    // Unknown character
    advance_pos(1);
    return make_token(TokenType::Identifier, std::string(1, c));
}

void Lexer::skip_whitespace() {
    while (pos_ < source_.size() && std::isspace(source_[pos_])) {
        pos_++;
        if (source_[pos_ - 1] == '\n') {
            line_++;
            col_ = 1;
        } else {
            col_++;
        }
    }
}

void Lexer::skip_line_comment() {
    pos_ += 2; // skip //
    while (pos_ < source_.size() && source_[pos_] != '\n') {
        pos_++;
        col_++;
    }
}

void Lexer::skip_block_comment() {
    pos_ += 2; // skip /*
    while (pos_ + 1 < source_.size()) {
        if (source_[pos_] == '*' && source_[pos_ + 1] == '/') {
            pos_ += 2;
            col_ += 2;
            return;
        }
        pos_++;
        col_++;
    }
}

auto Lexer::read_number() -> Token {
    size_t start = pos_;
    bool has_dot = false;

    while (pos_ < source_.size()) {
        if (source_[pos_] == '.') {
            if (has_dot) break;
            has_dot = true;
        } else if (!std::isdigit(source_[pos_])) {
            break;
        }
        pos_++;
        col_++;
    }

    std::string num_str{source_.substr(start, pos_ - start)};
    return make_token(has_dot ? TokenType::FloatLiteral : TokenType::IntegerLiteral, num_str);
}

auto Lexer::read_string() -> Token {
    size_t start = pos_;
    pos_++; // skip opening quote
    std::string str;
    col_++;

    while (pos_ < source_.size() && source_[pos_] != '\'') {
        str += source_[pos_];
        pos_++;
        col_++;
    }

    if (pos_ < source_.size()) {
        pos_++; // skip closing quote
        col_++;
    }

    return make_token(TokenType::StringLiteral, std::move(str));
}

namespace {

auto keyword_map() -> const std::unordered_map<std::string, TokenType>& {
    static const std::unordered_map<std::string, TokenType> map = {
        {"SELECT", TokenType::KeywordSelect}, {"FROM", TokenType::KeywordFrom},
        {"WHERE", TokenType::KeywordWhere}, {"ORDER", TokenType::KeywordOrder},
        {"BY", TokenType::KeywordBy}, {"GROUP", TokenType::KeywordGroup},
        {"HAVING", TokenType::KeywordHaving}, {"LIMIT", TokenType::KeywordLimit},
        {"OFFSET", TokenType::KeywordOffset}, {"AS", TokenType::KeywordAs},
        {"AND", TokenType::KeywordAnd}, {"OR", TokenType::KeywordOr},
        {"NOT", TokenType::KeywordNot}, {"NULL", TokenType::KeywordNull},
        {"TRUE", TokenType::KeywordTrue}, {"FALSE", TokenType::KeywordFalse},
        {"SUM", TokenType::KeywordSum}, {"COUNT", TokenType::KeywordCount},
        {"AVG", TokenType::KeywordAvg}, {"MIN", TokenType::KeywordMin},
        {"MAX", TokenType::KeywordMax}, {"INSERT", TokenType::KeywordInsert},
        {"INTO", TokenType::KeywordInto}, {"VALUES", TokenType::KeywordValues},
        {"CREATE", TokenType::KeywordCreate}, {"TABLE", TokenType::KeywordTable},
        {"DROP", TokenType::KeywordDrop}, {"SHOW", TokenType::KeywordShow},
        {"DATABASES", TokenType::KeywordShow}, {"DATABASE", TokenType::KeywordDatabase},
        {"TABLES", TokenType::KeywordTable}, {"DESCRIBE", TokenType::KeywordDescribe},
        {"DESC", TokenType::KeywordDesc}, {"EXPLAIN", TokenType::KeywordExplain},
        {"JOIN", TokenType::KeywordJoin}, {"LEFT", TokenType::KeywordLeft},
        {"RIGHT", TokenType::KeywordRight}, {"INNER", TokenType::KeywordInner},
        {"ON", TokenType::KeywordOn}, {"UNION", TokenType::KeywordAll},
        {"ALL", TokenType::KeywordAll}, {"WITH", TokenType::KeywordWith},
        {"ROLLUP", TokenType::KeywordRollup}, {"ARRAY", TokenType::KeywordArray},
        {"LATERAL", TokenType::KeywordLateral}, {"ANY", TokenType::KeywordAny},
        {"DISTINCT", TokenType::KeywordDistinct}, {"ASC", TokenType::KeywordAsc},
        {"USE", TokenType::KeywordUse}, {"ALTER", TokenType::KeywordAlter},
        {"IF", TokenType::KeywordIf}, {"EXISTS", TokenType::KeywordExists},
        {"ENGINE", TokenType::KeywordEngine}, {"TRUNCATE", TokenType::KeywordTruncate},
        {"DETACH", TokenType::KeywordDetach}, {"ADD", TokenType::KeywordAdd},
        {"COLUMN", TokenType::KeywordColumn}, {"MODIFY", TokenType::KeywordModify},
        {"IN", TokenType::KeywordIn}, {"OVER", TokenType::KeywordOver},
        {"PARTITION", TokenType::KeywordPartition}, {"VIEW", TokenType::KeywordView},
        {"VIEWS", TokenType::KeywordViews}, {"MATERIALIZED", TokenType::KeywordMaterialized},
        {"REFRESH", TokenType::KeywordRefresh}, {"STORAGE", TokenType::KeywordStorage},
        {"UNIT", TokenType::KeywordUnit}, {"UNITS", TokenType::KeywordUnits},
        {"TYPE", TokenType::KeywordType}, {"PATH", TokenType::KeywordPath},
        {"BUCKET", TokenType::KeywordBucket}, {"ENDPOINT", TokenType::KeywordEndpoint},
        {"REGION", TokenType::KeywordRegion}, {"USAGE", TokenType::KeywordUsage},
        {"NODE", TokenType::KeywordNode}, {"NODES", TokenType::KeywordNodes},
        {"REGISTER", TokenType::KeywordRegister}, {"DRAIN", TokenType::KeywordDrain},
        {"REMOVE", TokenType::KeywordRemove}, {"HOST", TokenType::KeywordHost},
        {"PORT", TokenType::KeywordPort}, {"ROLE", TokenType::KeywordRole},
        {"SET", TokenType::KeywordSet}, {"METRICS", TokenType::KeywordMetrics},
        {"CAPABILITIES", TokenType::KeywordCapabilities}, {"WORKER", TokenType::KeywordWorker},
        {"COORDINATOR", TokenType::KeywordCoordinator}, {"OBSERVER", TokenType::KeywordObserver},
        {"COMPUTE", TokenType::KeywordCompute}, {"HYBRID", TokenType::KeywordHybrid},
        {"GPU", TokenType::KeywordGpu}, {"CLUSTER", TokenType::KeywordCluster},
        {"CLUSTERS", TokenType::KeywordClusters}, {"REPLICAS", TokenType::KeywordReplicas},
        {"REPLICA", TokenType::KeywordReplica}, {"REPLICA_GROUP", TokenType::KeywordReplicaGroup},
        {"REPLICA_GROUPS", TokenType::KeywordReplicaGroups},
        {"REPLICATION", TokenType::KeywordReplication}, {"STATUS", TokenType::KeywordStatus},
        {"CONSISTENCY", TokenType::KeywordConsistency}, {"QUORUM", TokenType::KeywordQuorum},
        {"SYNCHRONOUS", TokenType::KeywordSynchronous},
        {"ASYNCHRONOUS", TokenType::KeywordAsynchronous},
        {"PLACEMENT", TokenType::KeywordPlacement}, {"NODE_AWARE", TokenType::KeywordNodeAware},
        {"SHARD", TokenType::KeywordShard}, {"SHARDS", TokenType::KeywordShards},
        {"SHARD_GROUP", TokenType::KeywordShardGroup},
        {"SHARD_GROUPS", TokenType::KeywordShardGroups}, {"KEY", TokenType::KeywordKey},
        {"CONNECTOR", TokenType::KeywordConnector}, {"CONNECTORS", TokenType::KeywordConnectors},
        {"TEST", TokenType::KeywordTest}, {"DISCOVER", TokenType::KeywordDiscover},
        {"AUTH", TokenType::KeywordAuth}, {"SCHEMA", TokenType::KeywordSchema},
        {"PIPELINE", TokenType::KeywordPipeline}, {"PIPELINES", TokenType::KeywordPipelines},
        {"STAGE", TokenType::KeywordStage}, {"STAGES", TokenType::KeywordStages},
        {"TASK", TokenType::KeywordTask}, {"TASKS", TokenType::KeywordTasks},
        {"TRIGGER", TokenType::KeywordTrigger}, {"TRIGGERS", TokenType::KeywordTriggers},
        {"RUN", TokenType::KeywordRun}, {"PAUSE", TokenType::KeywordPause},
        {"RESUME", TokenType::KeywordResume}, {"SCHEDULE", TokenType::KeywordSchedule},
        {"DEPENDS", TokenType::KeywordDepends}, {"BODY", TokenType::KeywordBody},
        {"OWNER", TokenType::KeywordOwner}, {"BUILTIN", TokenType::KeywordBuiltin},
        {"BUILT_IN", TokenType::KeywordBuiltin},         {"PIPELINE_RUNS", TokenType::KeywordPipelineRuns},
        {"FOR", TokenType::KeywordFor},
        {"STREAM", TokenType::KeywordStream}, {"STREAMS", TokenType::KeywordStreams},
        {"TOPIC", TokenType::KeywordTopic}, {"TOPICS", TokenType::KeywordTopics},
        {"CONSUMER_GROUP", TokenType::KeywordConsumerGroup},
        {"CONSUMER_GROUPS", TokenType::KeywordConsumerGroups},
        {"PUBLISH", TokenType::KeywordPublish}, {"SUBSCRIBE", TokenType::KeywordSubscribe},
        {"RETAIN", TokenType::KeywordRetain},
        {"STREAM_METRICS", TokenType::KeywordStreamMetrics},
        {"PARTITIONS", TokenType::KeywordPartitions},
        {"DAYS", TokenType::KeywordDays}, {"FOREVER", TokenType::KeywordForever},
        // MODEL layer verbs
        {"DEPLOY", TokenType::KeywordDeploy}, {"PREDICT", TokenType::KeywordPredict},
        {"EVALUATE", TokenType::KeywordEvaluate}, {"COMPARE", TokenType::KeywordCompare},
        {"GENERATE", TokenType::KeywordGenerate},
    };
    return map;
}

} // namespace

auto Lexer::read_identifier() -> Token {
    size_t start = pos_;
    while (pos_ < source_.size() && (std::isalnum(source_[pos_]) || source_[pos_] == '_')) {
        pos_++;
        col_++;
    }

    std::string id{source_.substr(start, pos_ - start)};
    std::string upper = id;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    TokenType type = TokenType::Identifier;
    if (const auto it = keyword_map().find(upper); it != keyword_map().end()) {
        type = it->second;
    }

    return make_token(type, std::move(id));
}

auto Lexer::make_token(TokenType type, std::string value) const -> Token {
    return Token{type, std::move(value), {file_, line_, col_}};
}

void Lexer::advance_pos(size_t n) {
    pos_ += n;
    col_ += n;
}

auto Lexer::peek() -> Token {
    size_t saved_pos = pos_;
    uint32_t saved_line = line_;
    uint32_t saved_col = col_;
    Token t = next();
    pos_  = saved_pos;
    line_ = saved_line;
    col_  = saved_col;
    return t;
}

void Lexer::reset() {
    pos_ = 0;
    line_ = 1;
    col_ = 1;
}

auto Lexer::eof() const -> bool {
    return pos_ >= source_.size();
}

} // namespace mnemo::parsers
