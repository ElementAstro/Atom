#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "atom/system/crash_quotes.hpp"

using namespace atom::system;

class TempJsonFile {
public:
    explicit TempJsonFile(const std::string& content) {
        filename_ = "test_quotes_" +
                    std::to_string(reinterpret_cast<uintptr_t>(this)) + ".json";
        std::ofstream file(filename_);
        file << content;
        file.close();
    }

    ~TempJsonFile() { std::remove(filename_.c_str()); }

    std::string filename() const { return filename_; }

private:
    std::string filename_;
};

class QuoteTest : public ::testing::Test {
protected:
    Quote quote{"The only true wisdom is in knowing you know nothing.",
                "Socrates", "Philosophy", 399};
};

class QuoteManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<QuoteManager>();

        // 添加一些测试引用
        manager->addQuote(Quote("Quote 1", "Author 1", "Category 1", 2001));
        manager->addQuote(Quote("Quote 2", "Author 2", "Category 2", 2002));
        manager->addQuote(Quote("Quote 3", "Author 1", "Category 1", 2003));
        manager->addQuote(Quote("Quote 4", "Author 3", "Category 3", 2004));
    }

    std::unique_ptr<QuoteManager> manager;
};

// Quote 类测试
TEST_F(QuoteTest, Getters) {
    EXPECT_EQ("The only true wisdom is in knowing you know nothing.",
              quote.getText());
    EXPECT_EQ("Socrates", quote.getAuthor());
    EXPECT_EQ("Philosophy", quote.getCategory());
    EXPECT_EQ(399, quote.getYear());
}

TEST_F(QuoteTest, Setters) {
    quote.setCategory("Ancient Philosophy");
    quote.setYear(400);

    EXPECT_EQ("Ancient Philosophy", quote.getCategory());
    EXPECT_EQ(400, quote.getYear());
}

TEST_F(QuoteTest, ToString) {
    // 不包含元数据
    std::string expected =
        "\"The only true wisdom is in knowing you know nothing.\" - Socrates";
    EXPECT_EQ(expected, quote.toString(false));

    // 包含元数据
    expected =
        "\"The only true wisdom is in knowing you know nothing.\" - Socrates "
        "(Philosophy, 399)";
    EXPECT_EQ(expected, quote.toString(true));

    // 测试无元数据的情况
    Quote quoteNoMetadata("Test quote", "Test author");
    expected = "\"Test quote\" - Test author";
    EXPECT_EQ(expected, quoteNoMetadata.toString(true));
}

TEST_F(QuoteTest, Equality) {
    Quote sameQuote("The only true wisdom is in knowing you know nothing.",
                    "Socrates", "Different Category", 500);
    Quote differentQuote("Different quote", "Socrates");

    EXPECT_EQ(quote, sameQuote);
    EXPECT_NE(quote, differentQuote);
}

// QuoteManager 类测试
TEST_F(QuoteManagerTest, Size) {
    EXPECT_EQ(4, manager->size());
    EXPECT_FALSE(manager->empty());

    QuoteManager emptyManager;
    EXPECT_EQ(0, emptyManager.size());
    EXPECT_TRUE(emptyManager.empty());
}

TEST_F(QuoteManagerTest, AddQuote) {
    Quote newQuote("New quote", "New author");
    bool result = manager->addQuote(newQuote);

    EXPECT_TRUE(result);
    EXPECT_EQ(5, manager->size());

    // 添加重复引用应返回 false
    result = manager->addQuote(newQuote);
    EXPECT_FALSE(result);
    EXPECT_EQ(5, manager->size());
}

TEST_F(QuoteManagerTest, AddQuotes) {
    std::vector<Quote> newQuotes = {
        Quote("Batch quote 1", "Batch author 1"),
        Quote("Batch quote 2", "Batch author 2"),
        Quote("Quote 1", "Author 1")  // 重复引用
    };

    size_t added = manager->addQuotes(newQuotes);

    EXPECT_EQ(2, added);
    EXPECT_EQ(6, manager->size());
}

TEST_F(QuoteManagerTest, RemoveQuote) {
    Quote quote = Quote("Quote 1", "Author 1");
    bool result = manager->removeQuote(quote);

    EXPECT_TRUE(result);
    EXPECT_EQ(3, manager->size());

    // 移除不存在的引用应返回 false
    result = manager->removeQuote(Quote("Nonexistent", "Author"));
    EXPECT_FALSE(result);
    EXPECT_EQ(3, manager->size());
}

TEST_F(QuoteManagerTest, RemoveQuotesByAuthor) {
    size_t removed = manager->removeQuotesByAuthor("Author 1");

    EXPECT_EQ(2, removed);
    EXPECT_EQ(2, manager->size());

    // 移除不存在的作者应返回 0
    removed = manager->removeQuotesByAuthor("Nonexistent Author");
    EXPECT_EQ(0, removed);
    EXPECT_EQ(2, manager->size());
}

TEST_F(QuoteManagerTest, ClearQuotes) {
    manager->clearQuotes();

    EXPECT_TRUE(manager->empty());
    EXPECT_EQ(0, manager->size());
}

TEST_F(QuoteManagerTest, SearchQuotes) {
    // 区分大小写测试
    std::vector<Quote> results = manager->searchQuotes("Quote", true);
    EXPECT_EQ(4, results.size());

    results = manager->searchQuotes("quote", true);
    EXPECT_EQ(0, results.size());

    // 不区分大小写测试
    results = manager->searchQuotes("quote", false);
    EXPECT_EQ(4, results.size());

    // 查找作者
    results = manager->searchQuotes("author 1", false);
    EXPECT_EQ(2, results.size());

    // 查找不存在的关键字
    results = manager->searchQuotes("nonexistent", false);
    EXPECT_TRUE(results.empty());
}

TEST_F(QuoteManagerTest, FilterQuotesByAuthor) {
    std::vector<Quote> results = manager->filterQuotesByAuthor("Author 1");

    EXPECT_EQ(2, results.size());
    EXPECT_EQ("Quote 1", results[0].getText());
    EXPECT_EQ("Quote 3", results[1].getText());

    // 不存在的作者
    results = manager->filterQuotesByAuthor("Nonexistent Author");
    EXPECT_TRUE(results.empty());
}

TEST_F(QuoteManagerTest, FilterQuotesByCategory) {
    std::vector<Quote> results = manager->filterQuotesByCategory("Category 1");

    EXPECT_EQ(2, results.size());
    EXPECT_EQ("Quote 1", results[0].getText());
    EXPECT_EQ("Quote 3", results[1].getText());

    // 不存在的类别
    results = manager->filterQuotesByCategory("Nonexistent Category");
    EXPECT_TRUE(results.empty());
}

TEST_F(QuoteManagerTest, FilterQuotesByYear) {
    std::vector<Quote> results = manager->filterQuotesByYear(2001);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ("Quote 1", results[0].getText());

    // 不存在的年份
    results = manager->filterQuotesByYear(1999);
    EXPECT_TRUE(results.empty());
}

TEST_F(QuoteManagerTest, FilterQuotes) {
    // 过滤出文本长度大于等于7的引用
    auto results = manager->filterQuotes(
        [](const Quote& q) { return q.getText().length() >= 7; });

    EXPECT_EQ(4, results.size());

    // 过滤出具有特定条件的引用
    results = manager->filterQuotes([](const Quote& q) {
        return q.getAuthor() == "Author 1" && q.getYear() > 2001;
    });

    EXPECT_EQ(1, results.size());
    EXPECT_EQ("Quote 3", results[0].getText());
}

TEST_F(QuoteManagerTest, GetRandomQuote) {
    // 测试随机引用
    std::string quote = manager->getRandomQuote();
    EXPECT_FALSE(quote.empty());

    // 空管理器
    QuoteManager emptyManager;
    EXPECT_TRUE(emptyManager.getRandomQuote().empty());
}

TEST_F(QuoteManagerTest, GetRandomQuoteObject) {
    // 测试随机引用对象
    auto quoteOpt = manager->getRandomQuoteObject();
    EXPECT_TRUE(quoteOpt.has_value());

    // 空管理器
    QuoteManager emptyManager;
    EXPECT_FALSE(emptyManager.getRandomQuoteObject().has_value());
}

TEST_F(QuoteManagerTest, GetAllQuotes) {
    const auto& allQuotes = manager->getAllQuotes();

    EXPECT_EQ(4, allQuotes.size());
    EXPECT_EQ("Quote 1", allQuotes[0].getText());
    EXPECT_EQ("Quote 2", allQuotes[1].getText());
    EXPECT_EQ("Quote 3", allQuotes[2].getText());
    EXPECT_EQ("Quote 4", allQuotes[3].getText());
}

TEST_F(QuoteManagerTest, ShuffleQuotes) {
    // 保存原始顺序
    const auto& beforeShuffle = manager->getAllQuotes();
    std::vector<std::string> originalTexts;
    for (const auto& quote : beforeShuffle) {
        originalTexts.push_back(quote.getText());
    }

    // 洗牌
    manager->shuffleQuotes();

    // 获取洗牌后的顺序
    const auto& afterShuffle = manager->getAllQuotes();
    std::vector<std::string> shuffledTexts;
    for (const auto& quote : afterShuffle) {
        shuffledTexts.push_back(quote.getText());
    }

    // 排序可能相同，但我们至少确保内容相同
    EXPECT_EQ(4, afterShuffle.size());
    EXPECT_THAT(shuffledTexts,
                ::testing::UnorderedElementsAreArray(originalTexts));

    // 注意：无法确保洗牌会改变顺序，因为随机排序可能会产生相同的顺序
}

TEST_F(QuoteManagerTest, JsonOperations) {
    // 创建测试 JSON 内容
    std::string jsonContent = R"({
        "quotes": [
            {
                "text": "JSON Quote 1",
                "author": "JSON Author 1",
                "category": "JSON Category",
                "year": 2010
            },
            {
                "text": "JSON Quote 2",
                "author": "JSON Author 2"
            }
        ]
    })";

    TempJsonFile tempFile(jsonContent);

    // 测试加载 JSON（替换现有引用）
    bool loadResult = manager->loadQuotesFromJson(tempFile.filename(), false);
    EXPECT_TRUE(loadResult);
    EXPECT_EQ(2, manager->size());

    // 重新加载 JSON（追加）
    loadResult = manager->loadQuotesFromJson(tempFile.filename(), true);
    EXPECT_TRUE(loadResult);
    EXPECT_EQ(4, manager->size());

    // 测试不存在的文件
    loadResult = manager->loadQuotesFromJson("nonexistent_file.json");
    EXPECT_FALSE(loadResult);

    // 测试保存 JSON
    std::string saveFilename = "test_save_quotes.json";
    bool saveResult = manager->saveQuotesToJson(saveFilename);
    EXPECT_TRUE(saveResult);

    // 验证保存的文件
    QuoteManager loadedManager;
    loadResult = loadedManager.loadQuotesFromJson(saveFilename);
    EXPECT_TRUE(loadResult);
    EXPECT_EQ(4, loadedManager.size());

    // 清理
    std::remove(saveFilename.c_str());
}

// 测试 Quote 类中未实现的 toString 方法的模拟实现
TEST(QuoteToStringTest, MockImplementation) {
    // 这是 Quote::toString 的模拟实现，用于测试直到实际实现完成
    auto mockToString = [](const Quote& quote,
                           bool includeMetadata) -> std::string {
        std::string result =
            "\"" + quote.getText() + "\" - " + quote.getAuthor();

        if (includeMetadata &&
            (!quote.getCategory().empty() || quote.getYear() != 0)) {
            result += " (";
            if (!quote.getCategory().empty()) {
                result += quote.getCategory();
                if (quote.getYear() != 0) {
                    result += ", ";
                }
            }
            if (quote.getYear() != 0) {
                result += std::to_string(quote.getYear());
            }
            result += ")";
        }

        return result;
    };

    Quote quote("Test quote", "Test author", "Test category", 2023);

    std::string expected = "\"Test quote\" - Test author";
    EXPECT_EQ(expected, mockToString(quote, false));

    expected = "\"Test quote\" - Test author (Test category, 2023)";
    EXPECT_EQ(expected, mockToString(quote, true));
}

// 边缘案例测试
TEST(QuoteEdgeCases, EmptyFields) {
    Quote emptyTextQuote("", "Author");
    EXPECT_EQ("\"\" - Author", emptyTextQuote.toString());

    Quote emptyAuthorQuote("Text", "");
    EXPECT_EQ("\"Text\" - ", emptyAuthorQuote.toString());
}

TEST(QuoteManagerEdgeCases, MultipleIdenticalQuotes) {
    QuoteManager manager;

    // 添加相同的引用多次，应该只添加一次
    Quote quote("Duplicate", "Author");

    EXPECT_TRUE(manager.addQuote(quote));
    EXPECT_FALSE(manager.addQuote(quote));
    EXPECT_FALSE(manager.addQuote(quote));

    EXPECT_EQ(1, manager.size());
}

TEST(QuoteManagerEdgeCases, CacheConsistency) {
    QuoteManager manager;

    // 添加引用以生成缓存
    manager.addQuote(Quote("Quote 1", "Author 1", "Category 1", 2001));
    manager.addQuote(Quote("Quote 2", "Author 1", "Category 2", 2002));

    // 过滤以触发缓存构建
    auto byAuthor = manager.filterQuotesByAuthor("Author 1");
    EXPECT_EQ(2, byAuthor.size());

    // 删除一个引用，应该使缓存失效
    EXPECT_TRUE(manager.removeQuote(Quote("Quote 1", "Author 1")));

    // 再次过滤，应重新构建缓存
    byAuthor = manager.filterQuotesByAuthor("Author 1");
    EXPECT_EQ(1, byAuthor.size());
    EXPECT_EQ("Quote 2", byAuthor[0].getText());
}

// 性能测试
TEST(QuoteManagerPerformance, DISABLED_LargeCollection) {
    QuoteManager manager;

    // 添加大量引用
    for (int i = 0; i < 10000; i++) {
        manager.addQuote(Quote("Performance test quote " + std::to_string(i),
                               "Author " + std::to_string(i % 100),
                               "Category " + std::to_string(i % 10),
                               2000 + (i % 20)));
    }

    EXPECT_EQ(10000, manager.size());

    // 测试各种操作的性能
    auto start = std::chrono::high_resolution_clock::now();

    auto byAuthor = manager.filterQuotesByAuthor("Author 50");

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "Time to filter 10000 quotes by author: " << duration << "ms"
              << std::endl;
    EXPECT_EQ(100, byAuthor.size());
}
