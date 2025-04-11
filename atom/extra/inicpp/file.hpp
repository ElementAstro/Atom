#ifndef ATOM_EXTRA_INICPP_INIFILE_HPP
#define ATOM_EXTRA_INICPP_INIFILE_HPP

#include <algorithm>
#include <execution>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <vector>
#include "atom/macro.hpp"
#include "section.hpp"

#if INICPP_CONFIG_PATH_QUERY
#include "path_query.hpp"
#endif

#if INICPP_CONFIG_EVENT_LISTENERS
#include "event_listener.hpp"
#endif

#include "atom/error/exception.hpp"

namespace inicpp {

ATOM_CONSTEXPR auto pathSeparator() noexcept -> char { return '.'; }

inline auto joinPath(const std::vector<std::string>& paths) -> std::string {
    std::ostringstream oss;
    for (const auto& path : paths) {
        if (!oss.str().empty()) {
            oss << pathSeparator();
        }
        oss << path;
    }
    return oss.str();
}

/**
 * @class IniFileBase
 * @brief A class for handling INI files with customizable comparison.
 * @tparam Comparator The comparator type for section names.
 */
template <typename Comparator>
class IniFileBase
    : public std::map<std::string, IniSectionBase<Comparator>, Comparator> {
private:
    char fieldSep_ = '=';  ///< The character used to separate fields.
    char esc_ = '\\';      ///< The escape character.
    std::vector<std::string> commentPrefixes_ = {
        "#", ";"};                  ///< The prefixes for comments.
    bool multiLineValues_ = false;  ///< Flag to enable multi-line values.
    bool overwriteDuplicateFields_ =
        true;  ///< Flag to allow overwriting duplicate fields.
    mutable std::shared_mutex mutex_;  ///< Shared mutex for thread-safety

#if INICPP_CONFIG_EVENT_LISTENERS
    EventManager eventManager_;  ///< Event manager for change notifications
    std::string fileName_;       ///< Name of the loaded file
#endif

    /**
     * @brief Erases comments from a line.
     * @param str The line to process.
     * @param startpos The position to start searching for comments.
     */
    void eraseComment(std::string& str,
                      std::string::size_type startpos = 0) const noexcept {
        for (const auto& commentPrefix : commentPrefixes_) {
            auto pos = str.find(commentPrefix, startpos);
            if (pos != std::string::npos) {
                // Check for escaped comment
                if (pos > 0 && str[pos - 1] == esc_) {
                    str.erase(pos - 1, 1);
                    // Need to adjust startpos since we've modified the string
                    eraseComment(str, pos);
                    return;
                }
                str.erase(pos);
                return;
            }
        }
    }

    /**
     * @brief Writes a string to an output stream with escaping.
     * @param oss The output stream.
     * @param str The string to write.
     */
    void writeEscaped(std::ostream& oss, const std::string& str) const {
        for (size_t i = 0; i < str.length(); ++i) {
            // Check for comment prefixes
            bool isCommentPrefix = false;
            for (const auto& prefix : commentPrefixes_) {
                if (i + prefix.size() <= str.size() &&
                    str.substr(i, prefix.size()) == prefix) {
                    oss.put(esc_);
                    oss.write(prefix.data(), prefix.size());
                    i += prefix.size() - 1;
                    isCommentPrefix = true;
                    break;
                }
            }

            if (isCommentPrefix) {
                continue;
            } else if (multiLineValues_ && str[i] == '\n') {
                oss.write("\n\t", 2);
            } else {
                oss.put(str[i]);
            }
        }
    }

    /**
     * @brief Process a section line from the INI file.
     * @param line The line to process.
     * @param lineNo The line number for error reporting.
     * @return A pointer to the section.
     */
    IniSectionBase<Comparator>* processSectionLine(std::string& line,
                                                   int lineNo) {
        auto pos = line.find(']');
        if (pos == std::string::npos) {
            THROW_LOGIC_ERROR("Section not closed at line " +
                              std::to_string(lineNo));
        }
        if (pos == 1) {
            THROW_LOGIC_ERROR("Empty section name at line " +
                              std::to_string(lineNo));
        }

        std::string secName = line.substr(1, pos - 1);
        trim(secName);

#if INICPP_CONFIG_NESTED_SECTIONS
        // Handle nested sections
        auto sectionPtr = createNestedSection(secName);
#else
        // 原始代码，不处理嵌套段落
        auto sectionPtr = &(*this)[secName];
#endif

#if INICPP_CONFIG_EVENT_LISTENERS
        // 设置段落名称以便触发事件
        sectionPtr->setSectionName(secName);
#endif

        return sectionPtr;
    }

#if INICPP_CONFIG_NESTED_SECTIONS
    /**
     * @brief 创建嵌套段落，自动处理段落层次结构
     * @param fullSectionName 完整的段落名称，可能包含分隔符
     * @return 指向创建的段落的指针
     */
    IniSectionBase<Comparator>* createNestedSection(
        const std::string& fullSectionName) {
        // 检查是否包含嵌套段落分隔符
        if (fullSectionName.find(pathSeparator()) == std::string::npos) {
            // 没有嵌套，直接创建/获取段落
            auto& section = (*this)[fullSectionName];

            // 如果是新段落，标记为顶层段落
            if (this->count(fullSectionName) == 0) {
                section.setParentSectionName("");
            }

            return &section;
        }

        // 处理嵌套段落
        auto parts = splitPath(fullSectionName);
        if (parts.empty()) {
            throw std::invalid_argument("Invalid section name: " +
                                        fullSectionName);
        }

        // 创建所有父级段落
        std::string currentPath = parts[0];
        auto& rootSection = (*this)[currentPath];

        // 标记为顶层段落（如果是新创建的）
        if (this->count(currentPath) == 0) {
            rootSection.setParentSectionName("");
        }

        // 创建中间段落
        auto* currentSection = &rootSection;
        for (size_t i = 1; i < parts.size(); ++i) {
            std::string parentPath = currentPath;
            currentPath += pathSeparator() + parts[i];

            // 创建或获取子段落
            auto& childSection = (*this)[currentPath];

            // 设置父子关系
            if (this->count(currentPath) == 0) {
                childSection.setParentSectionName(parentPath);
                currentSection->addChildSection(currentPath);
            }

            currentSection = &childSection;
        }

        return currentSection;
    }

    /**
     * @brief 获取嵌套段落
     * @param sectionPath 段落路径，可能包含分隔符
     * @return 指向段落的指针，如果未找到则为nullptr
     */
    IniSectionBase<Comparator>* getNestedSection(
        const std::string& sectionPath) {
        // 直接查找完整路径
        auto it = this->find(sectionPath);
        if (it != this->end()) {
            return &(it->second);
        }

        // 如果不包含分隔符，则肯定不存在
        if (sectionPath.find(pathSeparator()) == std::string::npos) {
            return nullptr;
        }

        // 尝试递归构建路径
        auto parts = splitPath(sectionPath);
        if (parts.empty()) {
            return nullptr;
        }

        // 检查根段落是否存在
        auto rootIt = this->find(parts[0]);
        if (rootIt == this->end()) {
            return nullptr;
        }

        // 递归检查每个段落部分
        std::string currentPath = parts[0];
        for (size_t i = 1; i < parts.size(); ++i) {
            currentPath += pathSeparator() + parts[i];
            auto it = this->find(currentPath);
            if (it == this->end()) {
                return nullptr;
            }
        }

        // 找到了完整的段落路径
        return &(this->at(currentPath));
    }
#endif

    /**
     * @brief Process a field line from the INI file.
     * @param line The line to process.
     * @param currentSection The current section.
     * @param multiLineValueFieldName Reference to store the current multi-line
     * field name.
     * @param hasIndent Whether the line has indent.
     * @param lineNo The line number for error reporting.
     */
    void processFieldLine(std::string& line,
                          IniSectionBase<Comparator>* currentSection,
                          std::string& multiLineValueFieldName, bool hasIndent,
                          int lineNo) {
        if (!currentSection) {
            THROW_LOGIC_ERROR("Field without section at line " +
                              std::to_string(lineNo));
        }

        auto pos = line.find(fieldSep_);
        if (multiLineValues_ && hasIndent && !multiLineValueFieldName.empty()) {
            // Handle multi-line value continuation
            std::string oldValue = (*currentSection)[multiLineValueFieldName]
                                       .template as<std::string>();
            std::string newValue = oldValue + "\n" + line;

            // 设置新值并捕获可能的事件
#if INICPP_CONFIG_EVENT_LISTENERS
            (*currentSection)[multiLineValueFieldName] = newValue;
#else
            (*currentSection)[multiLineValueFieldName] = newValue;
#endif
        } else if (pos == std::string::npos) {
            THROW_LOGIC_ERROR("Field separator missing at line " +
                              std::to_string(lineNo));
        } else {
            std::string name = line.substr(0, pos);
            trim(name);

            if (name.empty()) {
                THROW_LOGIC_ERROR("Empty field name at line " +
                                  std::to_string(lineNo));
            }

            if (!overwriteDuplicateFields_ && currentSection->count(name)) {
                THROW_LOGIC_ERROR("Duplicate field at line " +
                                  std::to_string(lineNo));
            }

            std::string value = line.substr(pos + 1);
            trim(value);

            // 设置新值并捕获可能的事件
#if INICPP_CONFIG_EVENT_LISTENERS
            (*currentSection)[name] = value;
#else
            (*currentSection)[name] = value;
#endif

            multiLineValueFieldName = name;
        }
    }

public:
    /**
     * @brief Default constructor.
     */
    IniFileBase() = default;

    /**
     * @brief Constructs an IniFileBase from a file.
     * @param filename The path to the INI file.
     */
    explicit IniFileBase(const std::string& filename) {
#if INICPP_CONFIG_EVENT_LISTENERS
        fileName_ = filename;
#endif
        try {
            load(filename);
        } catch (const std::exception& ex) {
            THROW_LOGIC_ERROR("Failed to construct IniFile from file: " +
                              std::string(ex.what()));
        }
    }

    /**
     * @brief Constructs an IniFileBase from an input stream.
     * @param iss The input stream.
     */
    explicit IniFileBase(std::istream& iss) {
        try {
            decode(iss);
        } catch (const std::exception& ex) {
            THROW_LOGIC_ERROR("Failed to construct IniFile from stream: " +
                              std::string(ex.what()));
        }
    }

    /**
     * @brief Destructor.
     */
    ~IniFileBase() = default;

    /**
     * @brief Sets the field separator character.
     * @param sep The field separator character.
     */
    void setFieldSep(char sep) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        fieldSep_ = sep;
    }

    /**
     * @brief Sets the comment prefixes.
     * @param commentPrefixes The vector of comment prefixes.
     */
    void setCommentPrefixes(std::span<const std::string> commentPrefixes) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        commentPrefixes_.assign(commentPrefixes.begin(), commentPrefixes.end());
    }

    /**
     * @brief Sets the escape character.
     * @param esc The escape character.
     */
    void setEscapeChar(char esc) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        esc_ = esc;
    }

    /**
     * @brief Enables or disables multi-line values.
     * @param enable True to enable multi-line values, false to disable.
     */
    void setMultiLineValues(bool enable) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        multiLineValues_ = enable;
    }

    /**
     * @brief Allows or disallows overwriting duplicate fields.
     * @param allowed True to allow overwriting, false to disallow.
     */
    void allowOverwriteDuplicateFields(bool allowed) noexcept {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        overwriteDuplicateFields_ = allowed;
    }

#if INICPP_CONFIG_EVENT_LISTENERS
    /**
     * @brief 获取事件管理器以添加/移除监听器
     * @return 事件管理器的引用
     */
    EventManager& getEventManager() noexcept { return eventManager_; }

    /**
     * @brief 获取事件管理器以添加/移除监听器（const版本）
     * @return 事件管理器的常量引用
     */
    const EventManager& getEventManager() const noexcept {
        return eventManager_;
    }

    /**
     * @brief 设置文件名
     * @param filename 文件名
     */
    void setFileName(const std::string& filename) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        fileName_ = filename;
    }

    /**
     * @brief 获取文件名
     * @return 当前文件名
     */
    std::string getFileName() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return fileName_;
    }
#endif

    /**
     * @brief Decodes an INI file from an input stream.
     * @param iss The input stream.
     */
    void decode(std::istream& iss) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        this->clear();

        // Read entire file content into memory for better performance
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }

        // Process lines in parallel if there are enough lines
        if (lines.size() >
            100) {  // Only use parallel processing for larger files
            processLinesParallel(lines);
        } else {
            processLinesSequential(lines);
        }

#if INICPP_CONFIG_EVENT_LISTENERS
        // 触发文件加载事件
        if (eventManager_.isEnabled()) {
            FileEventData eventData{.fileName = fileName_,
                                    .eventType = FileEventType::FILE_LOADED};
            eventManager_.notifyFileEvent(eventData);
        }
#endif
    }

    /**
     * @brief Process lines sequentially
     * @param lines The lines to process
     */
    void processLinesSequential(const std::vector<std::string>& lines) {
        IniSectionBase<Comparator>* currentSection = nullptr;
        std::string multiLineValueFieldName;

        int lineNo = 0;
        for (const auto& originalLine : lines) {
            ++lineNo;
            std::string line = originalLine;

            eraseComment(line);
            bool hasIndent = line.find_first_not_of(indents()) != 0;
            trim(line);

            if (line.empty()) {
                continue;
            }

            if (line.front() == '[') {
                // Section line
                currentSection = processSectionLine(line, lineNo);
                multiLineValueFieldName.clear();
            } else {
                processFieldLine(line, currentSection, multiLineValueFieldName,
                                 hasIndent, lineNo);
            }
        }
    }

    /**
     * @brief Process lines in parallel
     * @param lines The lines to process
     */
    void processLinesParallel(const std::vector<std::string>& lines) {
        struct Section {
            std::string name;
            std::vector<std::pair<std::string, std::string>> fields;
        };

        std::vector<Section> sections;
        Section currentSection;

        // First pass: identify sections and their fields
        int lineNo = 0;
        std::string multiLineValueFieldName;

        for (const auto& originalLine : lines) {
            ++lineNo;
            std::string line = originalLine;

            eraseComment(line);
            bool hasIndent = line.find_first_not_of(indents()) != 0;
            trim(line);

            if (line.empty()) {
                continue;
            }

            if (line.front() == '[') {
                // New section found, store the previous one if it has a name
                if (!currentSection.name.empty()) {
                    sections.push_back(currentSection);
                }

                // Parse section name
                auto pos = line.find(']');
                if (pos == std::string::npos) {
                    THROW_LOGIC_ERROR("Section not closed at line " +
                                      std::to_string(lineNo));
                }
                if (pos == 1) {
                    THROW_LOGIC_ERROR("Empty section name at line " +
                                      std::to_string(lineNo));
                }

                // Start a new section
                currentSection = Section{};
                currentSection.name = line.substr(1, pos - 1);
                trim(currentSection.name);

                multiLineValueFieldName.clear();
            } else {
                // Process field
                if (currentSection.name.empty()) {
                    THROW_LOGIC_ERROR("Field without section at line " +
                                      std::to_string(lineNo));
                }

                auto pos = line.find(fieldSep_);
                if (multiLineValues_ && hasIndent &&
                    !multiLineValueFieldName.empty()) {
                    // Multi-line continuation
                    if (!currentSection.fields.empty()) {
                        auto& lastField = currentSection.fields.back();
                        if (lastField.first == multiLineValueFieldName) {
                            lastField.second += "\n" + line;
                        }
                    }
                } else if (pos == std::string::npos) {
                    THROW_LOGIC_ERROR("Field separator missing at line " +
                                      std::to_string(lineNo));
                } else {
                    std::string name = line.substr(0, pos);
                    trim(name);

                    if (name.empty()) {
                        THROW_LOGIC_ERROR("Empty field name at line " +
                                          std::to_string(lineNo));
                    }

                    std::string value = line.substr(pos + 1);
                    trim(value);

                    // Check for duplicate fields
                    if (!overwriteDuplicateFields_) {
                        auto it = std::find_if(currentSection.fields.begin(),
                                               currentSection.fields.end(),
                                               [&name](const auto& pair) {
                                                   return pair.first == name;
                                               });
                        if (it != currentSection.fields.end()) {
                            THROW_LOGIC_ERROR("Duplicate field at line " +
                                              std::to_string(lineNo));
                        }
                    }

                    currentSection.fields.emplace_back(name, value);
                    multiLineValueFieldName = name;
                }
            }
        }

        // Don't forget to add the last section
        if (!currentSection.name.empty()) {
            sections.push_back(currentSection);
        }

        // Second pass: process sections in parallel
        std::mutex mapMutex;
        std::for_each(std::execution::par, sections.begin(), sections.end(),
                      [this, &mapMutex](const Section& section) {
                          IniSectionBase<Comparator> sectionMap;

                          for (const auto& [name, value] : section.fields) {
                              sectionMap[name] = value;
                          }

#if INICPP_CONFIG_EVENT_LISTENERS
                          // 设置段落名称
                          sectionMap.setSectionName(section.name);
#endif

#if INICPP_CONFIG_NESTED_SECTIONS
                          // Thread-safe 段落处理
                          std::lock_guard<std::mutex> lock(mapMutex);

                          // 创建嵌套段落
                          IniSectionBase<Comparator>* sectionPtr =
                              createNestedSection(section.name);

                          // 复制字段
                          for (const auto& [name, value] : section.fields) {
                              (*sectionPtr)[name] = value;
                          }
#else
                          // Thread-safe insert into the main map
                          std::lock_guard<std::mutex> lock(mapMutex);
                          (*this)[section.name] = std::move(sectionMap);
#endif
                      });
    }

    /**
     * @brief Decodes an INI file from a string.
     * @param content The string content of the INI file.
     */
    void decode(const std::string& content) {
        std::istringstream ss(content);
        decode(ss);
    }

    /**
     * @brief Loads and decodes an INI file from a file path.
     * @param fileName The path to the INI file.
     * @throws FAIL_TO_OPEN_FILE_EXCEPTION if the file cannot be opened.
     */
    void load(const std::string& fileName) {
        // Check if file exists
        if (!std::filesystem::exists(fileName)) {
            THROW_FAIL_TO_OPEN_FILE("File does not exist: " + fileName);
        }

        // Check if file is readable
        std::error_code ec;
        if (!std::filesystem::is_regular_file(fileName, ec) || ec) {
            THROW_FAIL_TO_OPEN_FILE("Not a regular file: " + fileName);
        }

#if INICPP_CONFIG_EVENT_LISTENERS
        // 更新文件名
        fileName_ = fileName;
#endif

        try {
            // For large files, use memory mapping for better performance
            if (std::filesystem::file_size(fileName) >
                1024 * 1024) {  // 1MB threshold
                loadLargeFile(fileName);
            } else {
                // For smaller files, use standard ifstream
                std::ifstream iss(fileName);
                if (!iss.is_open()) {
                    THROW_FAIL_TO_OPEN_FILE("Unable to open file " + fileName);
                }
                decode(iss);
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            THROW_FAIL_TO_OPEN_FILE("Filesystem error: " +
                                    std::string(ex.what()));
        }
    }

    /**
     * @brief Load a large file using memory-mapped IO
     * @param fileName The path to the file
     */
    void loadLargeFile(const std::string& fileName) {
        // Implementation note:
        // This would use memory mapping APIs like mmap on POSIX systems
        // or CreateFileMapping/MapViewOfFile on Windows
        // For simplicity, we'll just use a standard ifstream with a large
        // buffer
        std::ifstream iss(fileName, std::ios::binary);
        if (!iss.is_open()) {
            THROW_FAIL_TO_OPEN_FILE("Unable to open file " + fileName);
        }

        // Set a large buffer for better performance
        std::vector<char> buffer(1024 * 1024);  // 1MB buffer
        iss.rdbuf()->pubsetbuf(buffer.data(), buffer.size());

        decode(iss);
    }

    /**
     * @brief Encodes the INI file to an output stream.
     * @param oss The output stream.
     */
    void encode(std::ostream& oss) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        // Collect sections and sort them for consistent output
        std::vector<std::reference_wrapper<
            const std::pair<const std::string, IniSectionBase<Comparator>>>>
            sections;
        sections.reserve(this->size());

        for (const auto& section : *this) {
            sections.push_back(std::cref(section));
        }

#if INICPP_CONFIG_NESTED_SECTIONS
        // 对于嵌套段落，先输出顶层段落，然后是其子段落
        std::sort(sections.begin(), sections.end(),
                  [](const auto& a, const auto& b) {
                      const auto& sectionA = a.get().second;
                      const auto& sectionB = b.get().second;

                      bool aIsTop = sectionA.isTopLevel();
                      bool bIsTop = sectionB.isTopLevel();

                      if (aIsTop && !bIsTop)
                          return true;
                      if (!aIsTop && bIsTop)
                          return false;

                      // 同层级按名称排序
                      return a.get().first < b.get().first;
                  });
#endif

        // Process sections in parallel for large files
        if (sections.size() > 10) {
            std::stringstream result;
            std::vector<std::string> sectionStrings(sections.size());

            std::transform(
                std::execution::par, sections.begin(), sections.end(),
                sectionStrings.begin(),
                [this](const auto& sectionRef) -> std::string {
                    const auto& [name, section] = sectionRef.get();
                    std::stringstream ss;
                    ss << '[' << name << "]\n";

                    for (const auto& fieldPair : section) {
                        ss << fieldPair.first << fieldSep_;
                        try {
                            ss << fieldPair.second.template as<std::string>();
                        } catch (const std::exception&) {
                            ss << "ERROR";  // Fallback for unparseable values
                        }
                        ss << '\n';
                    }
                    ss << '\n';
                    return ss.str();
                });

            for (const auto& sectionStr : sectionStrings) {
                oss << sectionStr;
            }
        } else {
            // Sequential processing for small files
            for (const auto& [name, section] : *this) {
                oss << '[' << name << "]\n";
                for (const auto& fieldPair : section) {
                    oss << fieldPair.first << fieldSep_
                        << fieldPair.second.template as<std::string>() << "\n";
                }
                oss << "\n";
            }
        }
    }

    /**
     * @brief Encodes the INI file to a string and returns it.
     * @return The encoded INI file as a string.
     */
    [[nodiscard]] auto encode() const -> std::string {
        std::ostringstream sss;
        encode(sss);
        return sss.str();
    }

    /**
     * @brief Saves the INI file to a given file path.
     * @param fileName The path to the file.
     * @throws FAIL_TO_OPEN_FILE_EXCEPTION if the file cannot be opened.
     */
    void save(const std::string& fileName) const {
        try {
            // Create parent directories if they don't exist
            std::filesystem::path filePath(fileName);
            if (auto parentPath = filePath.parent_path(); !parentPath.empty()) {
                std::filesystem::create_directories(parentPath);
            }

            // Open file and write content
            std::ofstream oss(fileName);
            if (!oss.is_open()) {
                THROW_FAIL_TO_OPEN_FILE("Unable to open file for writing: " +
                                        fileName);
            }

            // Use a larger buffer for better performance
            std::vector<char> buffer(64 * 1024);  // 64KB buffer
            oss.rdbuf()->pubsetbuf(buffer.data(), buffer.size());

            encode(oss);

            // Ensure data is written to disk
            oss.flush();
            if (oss.fail()) {
                THROW_FAIL_TO_OPEN_FILE("Failed to write to file: " + fileName);
            }

#if INICPP_CONFIG_EVENT_LISTENERS
            if (eventManager_.isEnabled()) {
                FileEventData eventData{.fileName = fileName,
                                        .sectionName = "",
                                        .eventType = FileEventType::FILE_SAVED};
                eventManager_.notifyFileEvent(eventData);
            }
#endif

        } catch (const std::filesystem::filesystem_error& ex) {
            THROW_FAIL_TO_OPEN_FILE("Filesystem error while saving: " +
                                    std::string(ex.what()));
        }
    }

    /**
     * @brief Get a section by name
     * @param sectionName The section name
     * @return Reference to the section
     * @throws std::out_of_range if the section does not exist
     */
    IniSectionBase<Comparator>& getSection(const std::string& sectionName) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
#if INICPP_CONFIG_NESTED_SECTIONS
        IniSectionBase<Comparator>* section = getNestedSection(sectionName);
        if (!section) {
            throw std::out_of_range("Section not found: " + sectionName);
        }
        return *section;
#else
        auto it = this->find(sectionName);
        if (it == this->end()) {
            throw std::out_of_range("Section not found: " + sectionName);
        }
        return it->second;
#endif
    }

    /**
     * @brief Get a section by name (const version)
     * @param sectionName The section name
     * @return Const reference to the section
     * @throws std::out_of_range if the section does not exist
     */
    const IniSectionBase<Comparator>& getSection(
        const std::string& sectionName) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
#if INICPP_CONFIG_NESTED_SECTIONS
        IniSectionBase<Comparator>* section = getNestedSection(sectionName);
        if (!section) {
            throw std::out_of_range("Section not found: " + sectionName);
        }
        return *section;
#else
        auto it = this->find(sectionName);
        if (it == this->end()) {
            throw std::out_of_range("Section not found: " + sectionName);
        }
        return it->second;
#endif
    }

    /**
     * @brief Check if a section exists
     * @param sectionName The section name
     * @return True if the section exists, false otherwise
     */
    bool hasSection(const std::string& sectionName) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
#if INICPP_CONFIG_NESTED_SECTIONS
        return getNestedSection(sectionName) != nullptr;
#else
        return this->find(sectionName) != this->end();
#endif
    }

    /**
     * @brief Get a value from a section
     * @tparam T The value type
     * @param sectionName The section name
     * @param fieldName The field name
     * @return The field value
     * @throws std::out_of_range if the section or field does not exist
     */
    template <typename T>
    T getValue(const std::string& sectionName,
               const std::string& fieldName) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        const auto& section = getSection(sectionName);
        return section.template get<T>(fieldName);
    }

    /**
     * @brief Get a value from a section with a default value
     * @tparam T The value type
     * @param sectionName The section name
     * @param fieldName The field name
     * @param defaultValue The default value to return if the section or field
     * does not exist
     * @return The field value or the default value
     */
    template <typename T>
    T getValue(const std::string& sectionName, const std::string& fieldName,
               const T& defaultValue) const noexcept {
        try {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            if (!hasSection(sectionName)) {
                return defaultValue;
            }
            const auto& section = getSection(sectionName);
            return section.template get<T>(fieldName, defaultValue);
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief Set a value in a section
     * @tparam T The value type
     * @param sectionName The section name
     * @param fieldName The field name
     * @param value The value to set
     */
    template <typename T>
    void setValue(const std::string& sectionName, const std::string& fieldName,
                  const T& value) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
#if INICPP_CONFIG_NESTED_SECTIONS
        IniSectionBase<Comparator>* section = getNestedSection(sectionName);
        if (!section) {
            // 创建嵌套段落
            section = createNestedSection(sectionName);
        }

        // 准备旧值用于事件通知
#if INICPP_CONFIG_EVENT_LISTENERS
        std::string oldValue;
        bool fieldExists = section->hasField(fieldName);
        if (fieldExists && eventManager_.isEnabled()) {
            try {
                oldValue = section->template get<std::string>(fieldName);
            } catch (...) {
                // 忽略转换错误
            }
        }
#endif

        // 设置值
        section->template set<T>(fieldName, value);

#if INICPP_CONFIG_EVENT_LISTENERS
        // 触发路径变更事件
        if (eventManager_.isEnabled()) {
            std::string path = sectionName + pathSeparator() + fieldName;
            std::string newValue;
            try {
                newValue = section->template get<std::string>(fieldName);
            } catch (...) {
                // 忽略转换错误
            }

            PathChangedEventData eventData{.path = path,
                                           .oldValue = oldValue,
                                           .newValue = newValue,
                                           .isNew = !fieldExists,
                                           .isRemoved = false};

            eventManager_.notifyPathChanged(eventData);
        }
#endif
#else
        (*this)[sectionName][fieldName] = value;
#endif
    }

#if INICPP_CONFIG_PATH_QUERY
    /**
     * @brief 使用路径查询获取值
     * @tparam T 值类型
     * @param path 路径（例如 "section.subsection.field"）
     * @return 转换后的值
     * @throws std::out_of_range 如果路径无效
     */
    template <typename T>
    T getValueByPath(const std::string& path) const {
        PathQuery query(path);
        if (!query.isValid() || query.size() < 2) {
            throw std::out_of_range("Invalid path: " + path);
        }

        // 最后一部分是字段名，之前的部分是段落路径
        auto sectionParts = query.getSectionPath();
        std::string sectionName = joinPath(sectionParts);
        std::string fieldName = query.getFieldName();

        return getValue<T>(sectionName, fieldName);
    }

    /**
     * @brief 使用路径查询获取值（带默认值）
     * @tparam T 值类型
     * @param path 路径（例如 "section.subsection.field"）
     * @param defaultValue 默认值
     * @return 转换后的值或默认值
     */
    template <typename T>
    T getValueByPath(const std::string& path,
                     const T& defaultValue) const noexcept {
        try {
            return getValueByPath<T>(path);
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief 使用路径查询设置值
     * @tparam T 值类型
     * @param path 路径（例如 "section.subsection.field"）
     * @param value 要设置的值
     * @throws std::out_of_range 如果路径无效
     */
    template <typename T>
    void setValueByPath(const std::string& path, const T& value) {
        PathQuery query(path);
        if (!query.isValid() || query.size() < 2) {
            throw std::out_of_range("Invalid path: " + path);
        }

        auto sectionParts = query.getSectionPath();
        std::string sectionName = joinPath(sectionParts);
        std::string fieldName = query.getFieldName();

        setValue<T>(sectionName, fieldName, value);
    }

    /**
     * @brief 检查路径是否存在
     * @param path 要检查的路径
     * @return 如果路径存在则为true
     */
    bool hasPath(const std::string& path) const noexcept {
        try {
            PathQuery query(path);
            if (!query.isValid() || query.size() < 2) {
                return false;
            }

            auto sectionParts = query.getSectionPath();
            std::string sectionName = joinPath(sectionParts);
            std::string fieldName = query.getFieldName();

            std::shared_lock<std::shared_mutex> lock(mutex_);
            if (!hasSection(sectionName)) {
                return false;
            }

            const auto& section = getSection(sectionName);
            return section.hasField(fieldName);
        } catch (...) {
            return false;
        }
    }
#endif

    /**
     * @brief Remove a section
     * @param sectionName The section name
     * @return True if the section was removed, false if it did not exist
     */
    bool removeSection(const std::string& sectionName) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
#if INICPP_CONFIG_NESTED_SECTIONS
        // 首先检查段落是否存在
        IniSectionBase<Comparator>* section = getNestedSection(sectionName);
        if (!section) {
            return false;
        }

        // 如果有子段落，也需要移除它们
        if (section->hasChildSections()) {
            auto childNames = section->getChildSectionNames();
            for (const auto& childName : childNames) {
                this->erase(childName);
            }
        }

        // 从父段落中移除此段落
        if (!section->isTopLevel()) {
            IniSectionBase<Comparator>* parentSection =
                getNestedSection(section->getParentSectionName());
            if (parentSection) {
                parentSection->removeChildSection(sectionName);
            }
        }

#if INICPP_CONFIG_EVENT_LISTENERS
        // 触发段落移除事件
        if (eventManager_.isEnabled()) {
            FileEventData eventData{
                .fileName = fileName_,
                .sectionName = sectionName,
                .eventType = FileEventType::SECTION_REMOVED};
            eventManager_.notifyFileEvent(eventData);
        }
#endif

        // 最后移除段落
        return this->erase(sectionName) > 0;
#else
        return this->erase(sectionName) > 0;
#endif
    }

    /**
     * @brief Remove a field from a section
     * @param sectionName The section name
     * @param fieldName The field name
     * @return True if the field was removed, false if it did not exist
     */
    bool removeField(const std::string& sectionName,
                     const std::string& fieldName) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

#if INICPP_CONFIG_NESTED_SECTIONS
        IniSectionBase<Comparator>* section = getNestedSection(sectionName);
        if (!section) {
            return false;
        }

        // 保存旧值以便通知事件
#if INICPP_CONFIG_EVENT_LISTENERS
        std::string oldValue;
        bool fieldExists = section->hasField(fieldName);
        if (fieldExists && eventManager_.isEnabled()) {
            try {
                oldValue = section->template get<std::string>(fieldName);
            } catch (...) {
                // 忽略转换错误
            }
        }
#endif

        // 删除字段
        bool result = section->deleteField(fieldName);

#if INICPP_CONFIG_EVENT_LISTENERS
        // 触发路径变更事件
        if (result && eventManager_.isEnabled()) {
            std::string path = sectionName + pathSeparator() + fieldName;

            PathChangedEventData eventData{.path = path,
                                           .oldValue = oldValue,
                                           .newValue = "",
                                           .isNew = false,
                                           .isRemoved = true};

            eventManager_.notifyPathChanged(eventData);
        }
#endif

        return result;
#else
        auto sectionIt = this->find(sectionName);
        if (sectionIt == this->end()) {
            return false;
        }
        return sectionIt->second.erase(fieldName) > 0;
#endif
    }

    /**
     * @brief Merge another INI file into this one
     * @param other The other INI file to merge
     * @param overwrite Whether to overwrite existing sections and fields
     */
    void merge(const IniFileBase& other, bool overwrite = true) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        std::shared_lock<std::shared_mutex> otherLock(other.mutex_);

        for (const auto& [sectionName, otherSection] : other) {
#if INICPP_CONFIG_NESTED_SECTIONS
            // 获取或创建段落
            IniSectionBase<Comparator>* section = getNestedSection(sectionName);
            if (!section) {
                section = createNestedSection(sectionName);
            }

            // 合并字段
            for (const auto& [fieldName, otherField] : otherSection) {
                if (overwrite || !section->hasField(fieldName)) {
                    (*section)[fieldName] = otherField;

#if INICPP_CONFIG_EVENT_LISTENERS
                    // 触发事件
                    if (eventManager_.isEnabled()) {
                        std::string path =
                            sectionName + pathSeparator() + fieldName;
                        std::string newValue =
                            otherField.template as<std::string>();
                        std::string oldValue;
                        bool isNew = !section->hasField(fieldName);

                        if (!isNew) {
                            try {
                                oldValue = section->template get<std::string>(
                                    fieldName);
                            } catch (...) {
                                // 忽略转换错误
                            }
                        }

                        PathChangedEventData eventData{.path = path,
                                                       .oldValue = oldValue,
                                                       .newValue = newValue,
                                                       .isNew = isNew,
                                                       .isRemoved = false};

                        eventManager_.notifyPathChanged(eventData);
                    }
#endif
                }
            }
#else
            auto it = this->find(sectionName);
            if (it == this->end()) {
                // Section doesn't exist in this file, add it
                (*this)[sectionName] = otherSection;
            } else if (overwrite) {
                // Section exists and we should overwrite fields
                for (const auto& [fieldName, otherField] : otherSection) {
                    it->second[fieldName] = otherField;
                }
            } else {
                // Section exists but we should only add new fields, not
                // overwrite existing ones
                for (const auto& [fieldName, otherField] : otherSection) {
                    if (it->second.find(fieldName) == it->second.end()) {
                        it->second[fieldName] = otherField;
                    }
                }
            }
#endif
        }
    }

#if INICPP_CONFIG_NESTED_SECTIONS
    /**
     * @brief 获取段落的所有子段落名称
     * @param sectionName 段落名称
     * @return 子段落名称的向量
     * @throws std::out_of_range 如果段落不存在
     */
    std::vector<std::string> getChildSections(
        const std::string& sectionName) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        IniSectionBase<Comparator>* section = getNestedSection(sectionName);
        if (!section) {
            throw std::out_of_range("Section not found: " + sectionName);
        }

        return section->getChildSectionNames();
    }

    /**
     * @brief 获取顶层段落名称列表
     * @return 顶层段落名称的向量
     */
    std::vector<std::string> getTopLevelSections() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::string> result;

        for (const auto& [name, section] : *this) {
            if (section.isTopLevel()) {
                result.push_back(name);
            }
        }

        return result;
    }
#endif
};

using IniFile = IniFileBase<std::less<>>;  ///< Case-sensitive INI file.
using IniFileCaseInsensitive =
    IniFileBase<StringInsensitiveLess>;  ///< Case-insensitive INI file.

#if INICPP_HAS_BOOST && INICPP_CONFIG_USE_BOOST_CONTAINERS
/**
 * @brief Hash-based INI file for faster lookups with large data sets.
 */
using IniFileHash = IniFileBase<std::equal_to<>>;

/**
 * @brief Case-insensitive hash-based INI file.
 */
using IniFileHashCaseInsensitive = IniFileBase<StringInsensitiveEqual>;
#endif

}  // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_INIFILE_HPP
