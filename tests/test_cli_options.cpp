// CLI flag semantics for prompt-rendering options.
//
// Covers the parts of parse_options() that carry real behaviour rather than a straight
// assignment: the `none` reasoning effort normalising to thinking-off, the chat-style gate
// on the effort rungs the stock template cannot render, and the fact that both are resolved
// after the whole argv is parsed, so flag order does not matter.
#include "options.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

ninfer::cli::Options parse(std::vector<std::string> arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (std::string& argument : arguments) { argv.push_back(argument.data()); }
    return ninfer::cli::parse_options(static_cast<int>(argv.size()), argv.data());
}

bool rejects(const std::function<void()>& operation) {
    try {
        operation();
    } catch (const std::invalid_argument&) { return true; }
    return false;
}

int check(bool condition, const char* message) {
    if (condition) { return 0; }
    std::cerr << message << '\n';
    return 1;
}

// Prompt-rendering flags carry real behaviour rather than a straight assignment: the `none`
// reasoning effort normalises to thinking-off, the chat style gates the effort rungs the stock
// template cannot render, and both are resolved after the whole argv is parsed, so flag order
// does not matter.
std::vector<std::string> with(std::vector<std::string> extra) {
    std::vector<std::string> arguments{"ninfer-cli", "model.ninfer", "--prompt", "hi"};
    arguments.insert(arguments.end(), std::make_move_iterator(extra.begin()),
                     std::make_move_iterator(extra.end()));
    return arguments;
}

int test_chat_style_flags() {
    int failures = 0;

    const ninfer::cli::Options defaults = parse(with({}));
    failures += check(defaults.chat_style == ninfer::ChatStyle::Default,
                      "chat style did not default to the artifact template");
    failures += check(defaults.enable_thinking, "thinking was not on by default");
    failures += check(!defaults.reasoning_effort, "a reasoning effort was set by default");

    failures += check(parse(with({"--chat-style", "sharp-v22.1"})).chat_style ==
                          ninfer::ChatStyle::SharpV22_1,
                      "--chat-style sharp-v22.1 was not parsed");
    failures +=
        check(parse(with({"--chat-style", "default"})).chat_style == ninfer::ChatStyle::Default,
              "--chat-style default was not parsed");
    failures += check(rejects([] { (void)parse(with({"--chat-style", "sharp"})); }),
                      "an unknown chat style was accepted");
    failures += check(rejects([] { (void)parse(with({"--chat-style"})); }),
                      "--chat-style without a value was accepted");

    // `none` is the seventh rung and means "no thinking at all", so it must land on exactly the
    // state --no-thinking produces rather than reaching the renderer as an effort.
    const ninfer::cli::Options none        = parse(with({"--reasoning-effort", "none"}));
    const ninfer::cli::Options no_thinking = parse(with({"--no-thinking"}));
    failures += check(!none.enable_thinking && !none.reasoning_effort,
                      "--reasoning-effort none did not disable thinking outright");
    failures += check(none.enable_thinking == no_thinking.enable_thinking &&
                          none.reasoning_effort == no_thinking.reasoning_effort,
                      "--reasoning-effort none did not match --no-thinking");

    failures += check(parse(with({"--reasoning-effort", "low"})).reasoning_effort ==
                          ninfer::ReasoningEffort::Low,
                      "low effort was not parsed");
    failures += check(parse(with({"--reasoning-effort", "medium"})).reasoning_effort ==
                          ninfer::ReasoningEffort::Medium,
                      "medium effort was not parsed");
    failures += check(parse(with({"--reasoning-effort", "xhigh"})).reasoning_effort ==
                          ninfer::ReasoningEffort::XHigh,
                      "xhigh effort was not parsed");
    failures += check(rejects([] { (void)parse(with({"--reasoning-effort", "extreme"})); }),
                      "an unknown reasoning effort was accepted");

    // minimal/high/max have no instruction block in the stock template, so they are refused
    // unless the Sharp overlay is selected.
    for (const char* effort : {"minimal", "high", "max"}) {
        failures += check(rejects([effort] { (void)parse(with({"--reasoning-effort", effort})); }),
                          "an extended reasoning effort was accepted under the default chat style");
    }
    failures += check(parse(with({"--reasoning-effort", "minimal", "--chat-style", "sharp-v22.1"}))
                              .reasoning_effort == ninfer::ReasoningEffort::Minimal,
                      "minimal effort was not accepted under sharp-v22.1");
    failures += check(parse(with({"--reasoning-effort", "high", "--chat-style", "sharp-v22.1"}))
                              .reasoning_effort == ninfer::ReasoningEffort::High,
                      "high effort was not accepted under sharp-v22.1");
    failures += check(parse(with({"--reasoning-effort", "max", "--chat-style", "sharp-v22.1"}))
                              .reasoning_effort == ninfer::ReasoningEffort::Max,
                      "max effort was not accepted under sharp-v22.1");
    // The gate runs after the whole argv is parsed, so --chat-style may come first or last.
    failures += check(parse(with({"--chat-style", "sharp-v22.1", "--reasoning-effort", "high"}))
                              .reasoning_effort == ninfer::ReasoningEffort::High,
                      "the chat-style gate depended on flag order");

    failures += check(ninfer::cli::usage_text("ninfer-cli").find("--chat-style") !=
                          std::string::npos,
                      "CLI help omits --chat-style");
    return failures;
}

} // namespace

int main() {
    int failures = 0;
    const ninfer::cli::Options configured =
        parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--thinking-budget", "37"});
    failures += check(configured.thinking_budget == 37,
                      "--thinking-budget did not preserve its positive value");
    failures +=
        check(ninfer::cli::usage_text("ninfer-cli").find("--thinking-budget") != std::string::npos,
              "CLI help omits --thinking-budget");
    failures += check(rejects([] {
                          (void)parse({"ninfer-cli", "model.ninfer", "--prompt", "hello",
                                       "--thinking-budget", "0"});
                      }),
                      "zero --thinking-budget was accepted");
    failures += check(rejects([] {
                          (void)parse({"ninfer-cli", "model.ninfer", "--prompt", "hello",
                                       "--thinking-budget", "8", "--no-thinking"});
                      }),
                      "--thinking-budget was accepted with --no-thinking");
    const ninfer::cli::Options with_effort =
        parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--thinking-budget", "8",
               "--reasoning-effort", "medium"});
    failures += check(with_effort.thinking_budget == 8 && with_effort.reasoning_effort,
                      "thinking budget did not coexist with reasoning effort");
    const ninfer::cli::Options dflash_vision =
        parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--vision", "--spec", "dflash",
               "--draft-tokens", "7"});
    failures += check(dflash_vision.enable_vision &&
                          dflash_vision.speculative.backend == ninfer::SpeculativeBackend::DFlash &&
                          dflash_vision.speculative.draft_tokens == 7,
                      "CLI did not preserve the combined DFlash and Vision startup features");
    for (const auto k : {1U, 2U, 7U, 15U}) {
        const auto dflash2 = parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--spec",
                                    "dflash2", "--draft-tokens", std::to_string(k)});
        failures += check(dflash2.speculative.backend == ninfer::SpeculativeBackend::DFlash2 &&
                              dflash2.speculative.draft_tokens == k,
                          "CLI did not preserve the DFlash2 draft count");
    }
    for (const auto k : {0U, 16U}) {
        failures +=
            check(rejects([&] {
                      (void)parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--spec",
                                   "dflash2", "--draft-tokens", std::to_string(k)});
                  }),
                  "CLI accepted an unsupported DFlash2 draft count");
    }
    const ninfer::cli::Options nvfp4 =
        parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--kv-dtype", "nvfp4"});
    failures += check(nvfp4.kv_cache == ninfer::KvCacheStorage::Nvfp4Group16,
                      "--kv-dtype nvfp4 did not select group-16 NVFP4 KV");
    const ninfer::cli::Options k8v4 =
        parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--kv-dtype", "k8v4"});
    failures += check(k8v4.kv_cache == ninfer::KvCacheStorage::Fp8KeyNvfp4Value,
                      "--kv-dtype k8v4 did not select asymmetric K8V4 KV");
    const std::string help = ninfer::cli::usage_text("ninfer-cli");
    failures +=
        check(help.find("nvfp4") != std::string::npos && help.find("k8v4") != std::string::npos,
              "CLI help omits a production KV storage mode");
    const ninfer::cli::Options logging =
        parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--log-level", "debug"});
    failures += check(logging.log_level == ninfer::product::LogLevel::Debug,
                      "CLI log level was not parsed");
    failures += check(help.find("--log-level") != std::string::npos,
                      "CLI help omits the log-level control");
    failures += check(rejects([] {
                          (void)parse({"ninfer-cli", "model.ninfer", "--prompt", "hello",
                                       "--log-level", "verbose"});
                      }),
                      "CLI accepted an unknown log level");
    failures +=
        check(rejects([] {
                  (void)parse({"ninfer-cli", "model.ninfer", "--prompt", "hello", "--top-k", "21"});
              }),
              "CLI accepted top_k beyond the executable candidate domain");
    failures += test_chat_style_flags();
    return failures == 0 ? 0 : 1;
}
