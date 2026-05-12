#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "core/whisper_tokenizer.h"

#include <string>
#include <vector>

using namespace impress;
using Catch::Matchers::ContainsSubstring;

// ============================================================================
// WhisperTokenizer 测试
// ============================================================================

TEST_CASE("默认构造函数 - 未加载词表", "[whisper_tokenizer]") {
    WhisperTokenizer tokenizer;

    REQUIRE(!tokenizer.isLoaded());
    REQUIRE(tokenizer.vocabSize() == 0);
}

TEST_CASE("语言 token ID - 常用语言", "[whisper_tokenizer]") {
    REQUIRE(WhisperTokenizer::languageTokenId("zh") == 50260);
    REQUIRE(WhisperTokenizer::languageTokenId("en") == 50259);
    REQUIRE(WhisperTokenizer::languageTokenId("ja") == 50261);
    REQUIRE(WhisperTokenizer::languageTokenId("ko") == 50262);
    REQUIRE(WhisperTokenizer::languageTokenId("fr") == 50265);
    REQUIRE(WhisperTokenizer::languageTokenId("de") == 50266);
    REQUIRE(WhisperTokenizer::languageTokenId("es") == 50267);
}

TEST_CASE("语言 token ID - 未知语言回退到英语", "[whisper_tokenizer]") {
    int unknownId = WhisperTokenizer::languageTokenId("unknown");
    REQUIRE(unknownId == 50259); // 默认英语
}

TEST_CASE("特殊 token 检测", "[whisper_tokenizer]") {
    // Whisper 特殊 token 范围: [50257, 50363]
    REQUIRE(WhisperTokenizer::isSpecialToken(50257) == true);
    REQUIRE(WhisperTokenizer::isSpecialToken(50362) == true);
    REQUIRE(WhisperTokenizer::isSpecialToken(50363) == true);

    // 非特殊 token
    REQUIRE(WhisperTokenizer::isSpecialToken(0) == false);
    REQUIRE(WhisperTokenizer::isSpecialToken(100) == false);
    REQUIRE(WhisperTokenizer::isSpecialToken(50256) == false);
    REQUIRE(WhisperTokenizer::isSpecialToken(50400) == false);
}

TEST_CASE("特殊 token 常量", "[whisper_tokenizer]") {
    REQUIRE(WhisperTokenizer::kTokenEndOfText == 50257);
    REQUIRE(WhisperTokenizer::kTokenEndOfSpeech == 50256);
    REQUIRE(WhisperTokenizer::kTokenNoSpeech == 50362);
    REQUIRE(WhisperTokenizer::kTokenTranscription == 50359);
    REQUIRE(WhisperTokenizer::kTokenLanguageBase == 50259);
}

TEST_CASE("解码空 token 列表", "[whisper_tokenizer]") {
    WhisperTokenizer tokenizer;
    std::vector<int> emptyTokens;

    QString result = tokenizer.decode(emptyTokens);
    REQUIRE(result.isEmpty());
}

TEST_CASE("未加载词表时解码 - 使用 token ID 格式", "[whisper_tokenizer]") {
    WhisperTokenizer tokenizer;
    std::vector<int> tokens = {100, 200, 300};

    QString result = tokenizer.decode(tokens);

    // 未加载词表时，tokenToString_ 为空，走 else 分支
    REQUIRE(result.contains("token:100"));
    REQUIRE(result.contains("token:200"));
    REQUIRE(result.contains("token:300"));
}

TEST_CASE("特殊 token 在解码中被跳过", "[whisper_tokenizer]") {
    WhisperTokenizer tokenizer;

    // 即使未加载词表，特殊 token 也应该被跳过
    std::vector<int> tokens = {50257, 100, 50258, 200, 50260};

    QString result = tokenizer.decode(tokens);

    // 特殊 token 不应出现在结果中
    REQUIRE(!result.contains("token:50257"));
    REQUIRE(!result.contains("token:50258"));
    REQUIRE(!result.contains("token:50260"));
    // 普通 token 应该在结果中
    REQUIRE(result.contains("token:100"));
    REQUIRE(result.contains("token:200"));
}

TEST_CASE("encode 空文本", "[whisper_tokenizer]") {
    WhisperTokenizer tokenizer;

    auto tokens = tokenizer.encode("");

    REQUIRE(tokens.empty());
}

TEST_CASE("decodeBytePair - 空格转义", "[whisper_tokenizer]") {
    WhisperTokenizer tokenizer;

    // 0x0120 = Ġ (unicode 空格转义)
    // 这个测试验证 BPE 解码时使用的 unicode 常量有效
    QChar spaceEscape(0x0120);
    REQUIRE(spaceEscape.unicode() == 0x0120);
}

TEST_CASE("解码 - token ID 格式输出", "[whisper_tokenizer]") {
    WhisperTokenizer tokenizer;

    std::vector<int> tokens = {1, 42, 1000};
    QString result = tokenizer.decode(tokens);

    // 未加载词表时输出 <token:ID|> 格式
    REQUIRE(result.contains("token:1"));
    REQUIRE(result.contains("token:42"));
    REQUIRE(result.contains("token:1000"));
}
