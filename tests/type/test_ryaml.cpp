#ifndef ATOM_TYPE_TEST_RYAML_HPP
#define ATOM_TYPE_TEST_RYAML_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "atom/type/ryaml.hpp"

namespace atom::type::test {

class YamlValueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试用的不同类型的 YAML 值
        null_value = YamlValue();
        string_value = YamlValue("test string");
        c_string_value = YamlValue("c string");
        number_value = YamlValue(42.5);
        int_value = YamlValue(42);
        long_value = YamlValue(42L);
        bool_value = YamlValue(true);

        // 创建对象和数组
        YamlObject obj;
        obj["key1"] = YamlValue("value1");
        obj["key2"] = YamlValue(123);
        object_value = YamlValue(obj);

        YamlArray arr;
        arr.push_back(YamlValue("item1"));
        arr.push_back(YamlValue(456));
        array_value = YamlValue(arr);

        // 创建别名
        alias_value = YamlValue::create_alias("test_alias");
    }

    YamlValue null_value;
    YamlValue string_value;
    YamlValue c_string_value;
    YamlValue number_value;
    YamlValue int_value;
    YamlValue long_value;
    YamlValue bool_value;
    YamlValue object_value;
    YamlValue array_value;
    YamlValue alias_value;
};

// 类型检查测试
TEST_F(YamlValueTest, TypeChecking) {
    // 验证各个值的类型
    EXPECT_TRUE(null_value.is_null());
    EXPECT_TRUE(string_value.is_string());
    EXPECT_TRUE(c_string_value.is_string());
    EXPECT_TRUE(number_value.is_number());
    EXPECT_TRUE(int_value.is_number());
    EXPECT_TRUE(long_value.is_number());
    EXPECT_TRUE(bool_value.is_bool());
    EXPECT_TRUE(object_value.is_object());
    EXPECT_TRUE(array_value.is_array());
    EXPECT_TRUE(alias_value.is_alias());

    // 验证各个值的类型枚举
    EXPECT_EQ(null_value.type(), YamlValue::Type::Null);
    EXPECT_EQ(string_value.type(), YamlValue::Type::String);
    EXPECT_EQ(number_value.type(), YamlValue::Type::Number);
    EXPECT_EQ(bool_value.type(), YamlValue::Type::Bool);
    EXPECT_EQ(object_value.type(), YamlValue::Type::Object);
    EXPECT_EQ(array_value.type(), YamlValue::Type::Array);
    EXPECT_EQ(alias_value.type(), YamlValue::Type::Alias);

    // 验证否定情况
    EXPECT_FALSE(null_value.is_string());
    EXPECT_FALSE(string_value.is_number());
    EXPECT_FALSE(number_value.is_bool());
    EXPECT_FALSE(bool_value.is_object());
    EXPECT_FALSE(object_value.is_array());
    EXPECT_FALSE(array_value.is_alias());
    EXPECT_FALSE(alias_value.is_null());
}

// 值访问测试
TEST_F(YamlValueTest, ValueAccess) {
    // 字符串访问
    EXPECT_EQ(string_value.as_string(), "test string");
    EXPECT_EQ(c_string_value.as_string(), "c string");
    EXPECT_THROW(null_value.as_string(), YamlException);

    // 数值访问
    EXPECT_DOUBLE_EQ(number_value.as_number(), 42.5);
    EXPECT_DOUBLE_EQ(int_value.as_number(), 42.0);
    EXPECT_DOUBLE_EQ(long_value.as_number(), 42.0);
    EXPECT_THROW(string_value.as_number(), YamlException);

    // 整数访问
    EXPECT_EQ(int_value.as_int(), 42);
    EXPECT_EQ(long_value.as_int(), 42);
    EXPECT_THROW(number_value.as_int(), YamlException);  // 42.5 不是整数
    EXPECT_THROW(bool_value.as_int(), YamlException);

    // 长整数访问
    EXPECT_EQ(int_value.as_long(), 42L);
    EXPECT_EQ(long_value.as_long(), 42L);
    EXPECT_THROW(number_value.as_long(), YamlException);
    EXPECT_THROW(bool_value.as_long(), YamlException);

    // 布尔值访问
    EXPECT_TRUE(bool_value.as_bool());
    EXPECT_THROW(int_value.as_bool(), YamlException);

    // 对象访问
    EXPECT_EQ(object_value.as_object().size(), 2);
    EXPECT_THROW(array_value.as_object(), YamlException);

    // 数组访问
    EXPECT_EQ(array_value.as_array().size(), 2);
    EXPECT_THROW(object_value.as_array(), YamlException);

    // 别名访问
    EXPECT_EQ(alias_value.alias_name(), "test_alias");
    EXPECT_THROW(string_value.alias_name(), YamlException);

    // 模板方法 as<T>() 测试
    EXPECT_EQ(string_value.as<std::string>(), "test string");
    EXPECT_EQ(int_value.as<int>(), 42);
    EXPECT_EQ(long_value.as<long>(), 42L);
    EXPECT_DOUBLE_EQ(number_value.as<double>(), 42.5);
    EXPECT_TRUE(bool_value.as<bool>());
    EXPECT_EQ(object_value.as<YamlObject>().size(), 2);
    EXPECT_EQ(array_value.as<YamlArray>().size(), 2);
}

// 操作符测试
TEST_F(YamlValueTest, Operators) {
    // 对象下标操作符
    EXPECT_EQ(object_value["key1"].as_string(), "value1");
    EXPECT_EQ(object_value["key2"].as_int(), 123);
    EXPECT_THROW(object_value["nonexistent"], YamlException);
    EXPECT_THROW(null_value["key"], YamlException);

    // 数组下标操作符
    EXPECT_EQ(array_value[0].as_string(), "item1");
    EXPECT_EQ(array_value[1].as_int(), 456);
    EXPECT_THROW(array_value[99], YamlException);
    EXPECT_THROW(null_value[0], YamlException);

    // 可修改的对象下标操作符
    YamlValue obj_copy = object_value;
    obj_copy["key1"] = YamlValue("new value");
    EXPECT_EQ(obj_copy["key1"].as_string(), "new value");

    // 可修改的数组下标操作符
    YamlValue arr_copy = array_value;
    arr_copy[0] = YamlValue("new item");
    EXPECT_EQ(arr_copy[0].as_string(), "new item");

    // 相等操作符
    YamlValue str1("test");
    YamlValue str2("test");
    YamlValue str3("different");
    YamlValue num1(123);

    EXPECT_TRUE(str1 == str2);
    EXPECT_FALSE(str1 == str3);
    EXPECT_FALSE(str1 == num1);

    // 不等操作符
    EXPECT_FALSE(str1 != str2);
    EXPECT_TRUE(str1 != str3);
    EXPECT_TRUE(str1 != num1);
}

// 对象方法测试
TEST_F(YamlValueTest, ObjectMethods) {
    // contains
    EXPECT_TRUE(object_value.contains("key1"));
    EXPECT_FALSE(object_value.contains("nonexistent"));
    EXPECT_THROW(null_value.contains("key"), YamlException);

    // get 带默认值
    YamlValue default_value("default");
    EXPECT_EQ(object_value.get("key1", default_value).as_string(), "value1");
    EXPECT_EQ(object_value.get("nonexistent", default_value).as_string(),
              "default");
    EXPECT_THROW(null_value.get("key", default_value), YamlException);

    // try_get
    auto key1_opt = object_value.try_get("key1");
    auto nonexistent_opt = object_value.try_get("nonexistent");

    EXPECT_TRUE(key1_opt.has_value());
    EXPECT_EQ(key1_opt.value().get().as_string(), "value1");
    EXPECT_FALSE(nonexistent_opt.has_value());
    EXPECT_THROW(null_value.try_get("key"), YamlException);

    // size
    EXPECT_EQ(object_value.size(), 2);
    EXPECT_EQ(array_value.size(), 2);
    EXPECT_THROW(null_value.size(), YamlException);

    // empty
    YamlObject empty_obj;
    YamlValue empty_obj_value(empty_obj);
    EXPECT_TRUE(empty_obj_value.empty());
    EXPECT_FALSE(object_value.empty());
    EXPECT_THROW(null_value.empty(), YamlException);

    // clear
    YamlValue obj_copy = object_value;
    obj_copy.clear();
    EXPECT_TRUE(obj_copy.empty());
    EXPECT_THROW(null_value.clear(), YamlException);

    // erase (key)
    YamlValue obj_copy2 = object_value;
    EXPECT_EQ(obj_copy2.erase("key1"), 1);
    EXPECT_EQ(obj_copy2.erase("nonexistent"), 0);
    EXPECT_THROW(null_value.erase("key"), YamlException);

    // erase (index)
    YamlValue arr_copy = array_value;
    arr_copy.erase(0);
    EXPECT_EQ(arr_copy.size(), 1);
    EXPECT_THROW(arr_copy.erase(99), YamlException);
    EXPECT_THROW(null_value.erase(0), YamlException);
}

// 序列化测试
TEST_F(YamlValueTest, Serialization) {
    // to_string 基本测试
    EXPECT_EQ(null_value.to_string(), "null");
    EXPECT_EQ(string_value.to_string(), "\"test string\"");
    EXPECT_EQ(bool_value.to_string(), "true");

    // 数值格式化
    EXPECT_EQ(int_value.to_string(), "42");
    EXPECT_TRUE(number_value.to_string().find("42.5") != std::string::npos);

    // to_yaml 基本测试
    EXPECT_EQ(null_value.to_yaml(), "null");
    EXPECT_EQ(string_value.to_yaml(), "test string");
    EXPECT_EQ(bool_value.to_yaml(), "true");

    // 流式格式选项测试
    YamlSerializeOptions flow_options;
    flow_options.use_flow_style = true;

    // 对象和数组的流式格式
    std::string obj_flow = object_value.to_yaml(flow_options);
    std::string arr_flow = array_value.to_yaml(flow_options);

    EXPECT_TRUE(obj_flow.find("{") != std::string::npos);
    EXPECT_TRUE(obj_flow.find("}") != std::string::npos);
    EXPECT_TRUE(arr_flow.find("[") != std::string::npos);
    EXPECT_TRUE(arr_flow.find("]") != std::string::npos);

    // 块格式选项测试
    YamlSerializeOptions block_options;
    block_options.use_flow_style = false;

    std::string obj_block = object_value.to_yaml(block_options);
    std::string arr_block = array_value.to_yaml(block_options);

    EXPECT_TRUE(obj_block.find("key1:") != std::string::npos);
    EXPECT_TRUE(arr_block.find("-") != std::string::npos);
}

// 标签和锚点测试
TEST_F(YamlValueTest, TagsAndAnchors) {
    // 标签测试
    YamlTag str_tag = YamlTag::Str();
    YamlTag int_tag = YamlTag::Int();

    YamlValue with_tag = string_value;
    with_tag.set_tag(str_tag);

    EXPECT_EQ(with_tag.tag().tag(), "!!str");
    EXPECT_TRUE(with_tag.tag().is_default());

    with_tag.set_tag(int_tag);
    EXPECT_EQ(with_tag.tag().tag(), "!!int");
    EXPECT_FALSE(with_tag.tag().is_default());

    // 锚点测试
    YamlAnchor anchor("test_anchor");
    YamlValue with_anchor = string_value;
    with_anchor.set_anchor(anchor);

    EXPECT_EQ(with_anchor.anchor().name(), "test_anchor");
    EXPECT_TRUE(with_anchor.anchor().has_name());

    // 空锚点
    YamlAnchor empty_anchor;
    EXPECT_FALSE(empty_anchor.has_name());
    EXPECT_TRUE(empty_anchor.name().empty());
}

class YamlDocumentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试文档
        YamlObject obj;
        obj["string"] = YamlValue("value");
        obj["number"] = YamlValue(123);
        obj["boolean"] = YamlValue(true);

        YamlArray arr;
        arr.push_back(YamlValue("item1"));
        arr.push_back(YamlValue(456));
        obj["array"] = YamlValue(arr);

        YamlObject nested;
        nested["key"] = YamlValue("nested value");
        obj["object"] = YamlValue(nested);

        doc_with_object.set_root(YamlValue(obj));

        // 创建简单文档
        doc_with_string.set_root(YamlValue("simple string"));
    }

    YamlDocument empty_doc;
    YamlDocument doc_with_string;
    YamlDocument doc_with_object;
};

// 文档基本操作测试
TEST_F(YamlDocumentTest, BasicOperations) {
    // 根值访问
    EXPECT_TRUE(empty_doc.root().is_null());
    EXPECT_TRUE(doc_with_string.root().is_string());
    EXPECT_TRUE(doc_with_object.root().is_object());

    // 设置根值
    YamlValue new_root(42);
    empty_doc.set_root(new_root);
    EXPECT_TRUE(empty_doc.root().is_number());
    EXPECT_EQ(empty_doc.root().as_int(), 42);

    // 可修改的根值
    YamlDocument mutable_doc;
    mutable_doc.root() = YamlValue("mutable");
    EXPECT_EQ(mutable_doc.root().as_string(), "mutable");
}

// 文档序列化测试
TEST_F(YamlDocumentTest, Serialization) {
    // 基本序列化
    std::string simple_yaml = doc_with_string.to_yaml();
    EXPECT_TRUE(simple_yaml.find("simple string") != std::string::npos);

    // 带选项的序列化
    YamlSerializeOptions options;
    options.explicit_start = true;
    options.explicit_end = true;

    std::string with_markers = doc_with_string.to_yaml(options);
    EXPECT_TRUE(with_markers.find("---") != std::string::npos);
    EXPECT_TRUE(with_markers.find("...") != std::string::npos);

    // 复杂对象序列化
    std::string complex_yaml = doc_with_object.to_yaml();
    EXPECT_TRUE(complex_yaml.find("string:") != std::string::npos);
    EXPECT_TRUE(complex_yaml.find("array:") != std::string::npos);
    EXPECT_TRUE(complex_yaml.find("object:") != std::string::npos);
}

class YamlParserTest : public ::testing::Test {
protected:
    // 使用基本的解析选项
    YamlParseOptions options;
};

// 基本解析测试
TEST_F(YamlParserTest, BasicParsing) {
    // 空文档
    YamlValue empty = YamlParser::parse("", options);
    EXPECT_TRUE(empty.is_null());

    // 基本标量
    YamlValue null_val = YamlParser::parse("null", options);
    YamlValue string_val = YamlParser::parse("\"test string\"", options);
    YamlValue number_val = YamlParser::parse("42.5", options);
    YamlValue int_val = YamlParser::parse("42", options);
    YamlValue bool_val = YamlParser::parse("true", options);

    EXPECT_TRUE(null_val.is_null());
    EXPECT_TRUE(string_val.is_string());
    EXPECT_EQ(string_val.as_string(), "test string");
    EXPECT_TRUE(number_val.is_number());
    EXPECT_DOUBLE_EQ(number_val.as_number(), 42.5);
    EXPECT_TRUE(int_val.is_number());
    EXPECT_EQ(int_val.as_int(), 42);
    EXPECT_TRUE(bool_val.is_bool());
    EXPECT_TRUE(bool_val.as_bool());
}

// 流式对象和数组解析测试
TEST_F(YamlParserTest, FlowCollections) {
    // 流式对象
    std::string obj_yaml = "{\"key1\": \"value1\", \"key2\": 123}";
    YamlValue obj = YamlParser::parse(obj_yaml, options);

    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj.size(), 2);
    EXPECT_TRUE(obj.contains("key1"));
    EXPECT_TRUE(obj.contains("key2"));
    EXPECT_EQ(obj["key1"].as_string(), "value1");
    EXPECT_EQ(obj["key2"].as_int(), 123);

    // 流式数组
    std::string arr_yaml = "[\"item1\", 456, true]";
    YamlValue arr = YamlParser::parse(arr_yaml, options);

    EXPECT_TRUE(arr.is_array());
    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0].as_string(), "item1");
    EXPECT_EQ(arr[1].as_int(), 456);
    EXPECT_TRUE(arr[2].as_bool());

    // 嵌套集合
    std::string nested_yaml =
        "{\"array\": [1, 2, 3], \"object\": {\"nested\": \"value\"}}";
    YamlValue nested = YamlParser::parse(nested_yaml, options);

    EXPECT_TRUE(nested.is_object());
    EXPECT_TRUE(nested["array"].is_array());
    EXPECT_TRUE(nested["object"].is_object());
    EXPECT_EQ(nested["array"].size(), 3);
    EXPECT_EQ(nested["object"]["nested"].as_string(), "value");
}

// 块格式解析测试
TEST_F(YamlParserTest, BlockCollections) {
    // 块格式对象
    std::string block_obj_yaml = R"(
key1: value1
key2: 123
key3: true
    )";

    YamlValue block_obj = YamlParser::parse(block_obj_yaml, options);

    EXPECT_TRUE(block_obj.is_object());
    EXPECT_EQ(block_obj.size(), 3);
    EXPECT_EQ(block_obj["key1"].as_string(), "value1");
    EXPECT_EQ(block_obj["key2"].as_int(), 123);
    EXPECT_TRUE(block_obj["key3"].as_bool());

    // 块格式数组
    std::string block_arr_yaml = R"(
- item1
- 456
- true
    )";

    YamlValue block_arr = YamlParser::parse(block_arr_yaml, options);

    EXPECT_TRUE(block_arr.is_array());
    EXPECT_EQ(block_arr.size(), 3);
    EXPECT_EQ(block_arr[0].as_string(), "item1");
    EXPECT_EQ(block_arr[1].as_int(), 456);
    EXPECT_TRUE(block_arr[2].as_bool());

    // 混合格式
    std::string mixed_yaml = R"(
object:
  key1: value1
  key2: 123
array:
  - item1
  - item2
nested:
  - key: value
  - [1, 2, 3]
    )";

    YamlValue mixed = YamlParser::parse(mixed_yaml, options);

    EXPECT_TRUE(mixed.is_object());
    EXPECT_TRUE(mixed["object"].is_object());
    EXPECT_TRUE(mixed["array"].is_array());
    EXPECT_TRUE(mixed["nested"].is_array());
    EXPECT_TRUE(mixed["nested"][0].is_object());
    EXPECT_TRUE(mixed["nested"][1].is_array());
}

// 文档标记和多文档测试
TEST_F(YamlParserTest, DocumentMarkers) {
    // 带文档标记的文档
    std::string doc_with_markers = R"(
---
key: value
...
    )";

    YamlValue doc_value = YamlParser::parse(doc_with_markers, options);
    EXPECT_TRUE(doc_value.is_object());
    EXPECT_EQ(doc_value["key"].as_string(), "value");

    // 多文档解析
    std::string multi_doc = R"(
---
doc1: value1
...
---
doc2: value2
...
---
- item1
- item2
...
    )";

    std::vector<YamlDocument> docs =
        YamlParser::parse_multi_documents(multi_doc, options);

    EXPECT_EQ(docs.size(), 3);
    EXPECT_TRUE(docs[0].root().is_object());
    EXPECT_TRUE(docs[1].root().is_object());
    EXPECT_TRUE(docs[2].root().is_array());
    EXPECT_EQ(docs[0].root()["doc1"].as_string(), "value1");
    EXPECT_EQ(docs[1].root()["doc2"].as_string(), "value2");
    EXPECT_EQ(docs[2].root()[0].as_string(), "item1");
}

// 标签、锚点和别名测试
TEST_F(YamlParserTest, TagsAnchorsAndAliases) {
    // 标签
    std::string with_tags = R"(
tagged_string: !str string value
tagged_int: !!int 42
tagged_null: !!null
    )";

    YamlValue tags_obj = YamlParser::parse(with_tags, options);

    EXPECT_TRUE(tags_obj.is_object());
    EXPECT_EQ(tags_obj["tagged_string"].tag().tag(), "!str");
    EXPECT_EQ(tags_obj["tagged_int"].tag().tag(), "!!int");
    EXPECT_EQ(tags_obj["tagged_null"].tag().tag(), "!!null");

    // 锚点和别名
    std::string with_anchors = R"(
anchored: &anchor_name anchor value
alias: *anchor_name
nested:
  - &item_anchor item value
  - *item_anchor
    )";

    // 确保启用锚点支持
    YamlParseOptions anchor_options = options;
    anchor_options.support_anchors = true;

    YamlValue anchors_obj = YamlParser::parse(with_anchors, anchor_options);

    EXPECT_TRUE(anchors_obj.is_object());
    EXPECT_EQ(anchors_obj["anchored"].as_string(), "anchor value");
    EXPECT_EQ(anchors_obj["alias"].as_string(), "anchor value");
    EXPECT_EQ(anchors_obj["nested"][0].as_string(), "item value");
    EXPECT_EQ(anchors_obj["nested"][1].as_string(), "item value");

    // 锚点被禁用时应该抛出异常
    YamlParseOptions no_anchor_options = options;
    no_anchor_options.support_anchors = false;

    EXPECT_THROW(YamlParser::parse(with_anchors, no_anchor_options),
                 YamlException);
}

// 特殊字符串格式测试
TEST_F(YamlParserTest, StringFormats) {
    // 单引号字符串
    std::string single_quoted = R"('single quoted string')";
    YamlValue single = YamlParser::parse(single_quoted, options);
    EXPECT_EQ(single.as_string(), "single quoted string");

    // 双引号字符串
    std::string double_quoted = R"("double quoted string")";
    YamlValue dbl = YamlParser::parse(double_quoted, options);
    EXPECT_EQ(dbl.as_string(), "double quoted string");

    // 不带引号的字符串
    std::string unquoted = R"(unquoted string)";
    YamlValue unq = YamlParser::parse(unquoted, options);
    EXPECT_EQ(unq.as_string(), "unquoted string");

    // 块标量 (|)
    std::string literal_block = R"(|
  Line one
  Line two
  Line three
)";
    YamlValue literal = YamlParser::parse(literal_block, options);
    EXPECT_EQ(literal.as_string(), "Line one\nLine two\nLine three\n");

    // 折叠块标量 (>)
    std::string folded_block = R"(>
  Line one
  Line two
  Line three
)";
    YamlValue folded = YamlParser::parse(folded_block, options);
    EXPECT_EQ(folded.as_string(), "Line one Line two Line three\n");
}

// 错误处理测试
TEST_F(YamlParserTest, ErrorHandling) {
    // 不匹配的括号
    EXPECT_THROW(YamlParser::parse("{unclosed", options), YamlException);
    EXPECT_THROW(YamlParser::parse("[unclosed", options), YamlException);
    EXPECT_THROW(YamlParser::parse("\"unclosed", options), YamlException);

    // 无效的别名
    EXPECT_THROW(YamlParser::parse("*unknown_alias", options), YamlException);

    // 无效的JSON数字格式
    EXPECT_THROW(YamlParser::parse("12.34.56", options), YamlException);

    // 重复键（当禁用允许重复键时）
    YamlParseOptions no_dup_options = options;
    no_dup_options.allow_duplicate_keys = false;
    std::string duplicate_keys = R"(
key: value1
key: value2
    )";
    EXPECT_THROW(YamlParser::parse(duplicate_keys, no_dup_options),
                 YamlException);

    // 允许重复键时应该使用最后一个值
    YamlParseOptions allow_dup_options = options;
    allow_dup_options.allow_duplicate_keys = true;
    YamlValue with_dups = YamlParser::parse(duplicate_keys, allow_dup_options);
    EXPECT_EQ(with_dups["key"].as_string(), "value2");
}

// 注释测试
TEST_F(YamlParserTest, Comments) {
    // 带注释的YAML
    std::string with_comments = R"(
# This is a comment
key1: value1  # Inline comment
key2: value2
# Another comment
    )";

    // 启用注释支持
    YamlParseOptions comments_options = options;
    comments_options.support_comments = true;

    YamlValue obj = YamlParser::parse(with_comments, comments_options);
    EXPECT_TRUE(obj.is_object());
    EXPECT_EQ(obj.size(), 2);
    EXPECT_EQ(obj["key1"].as_string(), "value1");
    EXPECT_EQ(obj["key2"].as_string(), "value2");

    // 禁用注释支持
    YamlParseOptions no_comments_options = options;
    no_comments_options.support_comments = false;

    EXPECT_THROW(YamlParser::parse(with_comments, no_comments_options),
                 YamlException);
}

// 特殊数值测试
TEST_F(YamlParserTest, SpecialNumbers) {
    // 无穷大和NaN
    std::string special_numbers = R"(
positive_inf: .inf
negative_inf: -.inf
not_a_number: .nan
    )";

    YamlValue obj = YamlParser::parse(special_numbers, options);
    EXPECT_TRUE(obj.is_object());

    double pos_inf = obj["positive_inf"].as_number();
    double neg_inf = obj["negative_inf"].as_number();
    double nan_val = obj["not_a_number"].as_number();

    EXPECT_TRUE(std::isinf(pos_inf) && pos_inf > 0);
    EXPECT_TRUE(std::isinf(neg_inf) && neg_inf < 0);
    EXPECT_TRUE(std::isnan(nan_val));
}

}  // namespace atom::type::test

#endif  // ATOM_TYPE_TEST_RYAML_HPP