// atom/extra/dotenv/test_dotenv.hpp

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include "dotenv.hpp"
#include "exceptions.hpp"

using namespace dotenv;

namespace {

std::filesystem::path temp_dir() {
    auto dir = std::filesystem::temp_directory_path() / "dotenv_test_dotenv";
    std::filesystem::create_directories(dir);
    return dir;
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void remove_file(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void remove_dir(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

}  // namespace

class DotenvTest : public ::testing::Test {
protected:
    std::filesystem::path dir;
    DotenvOptions options;

    void SetUp() override {
        dir = temp_dir();
        options.load_options.search_paths = {dir.string()};
        options.load_options.file_patterns = {".env", ".env.local"};
        options.parse_options.trim_whitespace = true;
        options.debug = true;
        std::filesystem::current_path(dir);
    }

    void TearDown() override { remove_dir(dir); }
};

TEST_F(DotenvTest, ConstructAndOptions) {
    Dotenv d1;
    Dotenv d2(options);
    EXPECT_EQ(d2.getOptions().debug, true);
    d2.setOptions(DotenvOptions{});
    EXPECT_EQ(d2.getOptions().debug, false);
}

TEST_F(DotenvTest, LoadSuccess) {
    auto file = dir / ".env";
    write_file(file, "A=1\nB=2\n");
    Dotenv d(options);
    auto result = d.load(file);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.variables["A"], "1");
    EXPECT_EQ(result.variables["B"], "2");
    EXPECT_EQ(result.loaded_files.size(), 1);
}

TEST_F(DotenvTest, LoadMissingFile) {
    auto file = dir / "missing.env";
    Dotenv d(options);
    auto result = d.load(file);
    EXPECT_FALSE(result.success);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("Failed to load"), std::string::npos);
}

TEST_F(DotenvTest, LoadInvalidFile) {
    auto file = dir / ".env";
    write_file(file, "INVALID_LINE");
    Dotenv d(options);
    auto result = d.load(file);
    EXPECT_FALSE(result.success);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("Parse error"), std::string::npos);
}

TEST_F(DotenvTest, LoadMultipleFiles) {
    auto file1 = dir / ".env";
    auto file2 = dir / ".env.local";
    write_file(file1, "A=1\nB=2\n");
    write_file(file2, "B=3\nC=4\n");
    Dotenv d(options);
    auto result = d.loadMultiple({file1, file2});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.variables["A"], "1");
    EXPECT_EQ(result.variables["B"], "2");  // .env wins by default
    EXPECT_EQ(result.variables["C"], "4");
    EXPECT_EQ(result.loaded_files.size(), 2);
}

TEST_F(DotenvTest, LoadMultipleOverrideExisting) {
    options.load_options.override_existing = true;
    auto file1 = dir / ".env";
    auto file2 = dir / ".env.local";
    write_file(file1, "A=1\nB=2\n");
    write_file(file2, "B=3\nC=4\n");
    Dotenv d(options);
    auto result = d.loadMultiple({file1, file2});
    EXPECT_EQ(result.variables["B"], "3");  // .env.local wins
}

TEST_F(DotenvTest, AutoLoadDiscoversFiles) {
    auto file1 = dir / ".env";
    auto file2 = dir / ".env.local";
    write_file(file1, "A=1\n");
    write_file(file2, "B=2\n");
    Dotenv d(options);
    auto result = d.autoLoad(dir);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.variables["A"], "1");
    EXPECT_EQ(result.variables["B"], "2");
}

TEST_F(DotenvTest, LoadFromStringSuccess) {
    Dotenv d(options);
    auto result = d.loadFromString("A=1\nB=2\n");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.variables["A"], "1");
    EXPECT_EQ(result.variables["B"], "2");
}

TEST_F(DotenvTest, LoadFromStringParseError) {
    Dotenv d(options);
    auto result = d.loadFromString("INVALID_LINE");
    EXPECT_FALSE(result.success);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("Parse error"), std::string::npos);
}

TEST_F(DotenvTest, LoadAndValidateSuccess) {
    auto file = dir / ".env";
    write_file(file, "A=1\nB=hello\n");
    Dotenv d(options);
    ValidationSchema schema;
    schema.required("A").optional("B", "defaultB");
    schema.rule("A", rules::notEmpty());
    schema.rule("B", rules::minLength(2));
    auto result = d.loadAndValidate(file, schema);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.variables["A"], "1");
    EXPECT_EQ(result.variables["B"], "hello");
}

TEST_F(DotenvTest, LoadAndValidateFailure) {
    auto file = dir / ".env";
    write_file(file, "A=\n");
    Dotenv d(options);
    ValidationSchema schema;
    schema.required("A").rule("A", rules::notEmpty());
    auto result = d.loadAndValidate(file, schema);
    EXPECT_FALSE(result.success);
    ASSERT_FALSE(result.errors.empty());
    EXPECT_NE(result.errors[0].find("Validation:"), std::string::npos);
}

TEST_F(DotenvTest, LoadAndValidateWithDefaults) {
    auto file = dir / ".env";
    write_file(file, "A=1\n");
    Dotenv d(options);
    ValidationSchema schema;
    schema.required("A").optional("B", "defB");
    auto result = d.loadAndValidate(file, schema);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.variables["B"], "defB");
}

TEST_F(DotenvTest, ApplyToEnvironmentSetsVars) {
    Dotenv d(options);
    std::unordered_map<std::string, std::string> vars = {
        {"DOTENV_TEST_VAR", "42"}};
    d.applyToEnvironment(vars, true);
    const char* val = std::getenv("DOTENV_TEST_VAR");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(std::string(val), "42");
}

TEST_F(DotenvTest, ApplyToEnvironmentNoOverride) {
    setenv("DOTENV_TEST_VAR", "orig", 1);
    Dotenv d(options);
    std::unordered_map<std::string, std::string> vars = {
        {"DOTENV_TEST_VAR", "new"}};
    d.applyToEnvironment(vars, false);
    const char* val = std::getenv("DOTENV_TEST_VAR");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(std::string(val), "orig");
}

TEST_F(DotenvTest, SaveAndLoadFile) {
    auto file = dir / "output.env";
    Dotenv d(options);
    std::unordered_map<std::string, std::string> vars = {{"A", "1"},
                                                         {"B", "hello world"}};
    d.save(file, vars);
    std::string content = read_file(file);
    EXPECT_NE(content.find("A=1"), std::string::npos);
    EXPECT_NE(content.find("B=\"hello world\""), std::string::npos);
}

TEST_F(DotenvTest, WatchAndStopWatching) {
    auto file = dir / "watched.env";
    write_file(file, "A=1\n");
    Dotenv d(options);
    bool callback_called = false;
    d.watch(file, [&](const LoadResult& result) {
        callback_called = true;
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.variables.at("A"), "2");
    });
    // Modify file after a short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    write_file(file, "A=2\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    d.stopWatching();
    EXPECT_TRUE(callback_called);
}

TEST_F(DotenvTest, StaticQuickLoad) {
    auto file = dir / ".env";
    write_file(file, "A=1\n");
    auto result = Dotenv::quickLoad(file);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.variables["A"], "1");
}

TEST_F(DotenvTest, StaticConfigSuccess) {
    auto file = dir / ".env";
    write_file(file, "DOTENV_TEST_VAR=abc\n");
    Dotenv::config(file, true);
    const char* val = std::getenv("DOTENV_TEST_VAR");
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(std::string(val), "abc");
}

TEST_F(DotenvTest, StaticConfigFailureThrows) {
    auto file = dir / "bad.env";
    EXPECT_THROW(Dotenv::config(file, true), DotenvException);
}
