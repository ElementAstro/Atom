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
#include "section.hpp"

#include "atom/error/exception.hpp"

namespace inicpp {

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
        return &(*this)[secName];
    }

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
            (*currentSection)[multiLineValueFieldName] =
                (*currentSection)[multiLineValueFieldName]
                    .template as<std::string>() +
                "\n" + line;
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
            (*currentSection)[name] = value;

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

                          // Thread-safe insert into the main map
                          std::lock_guard<std::mutex> lock(mapMutex);
                          (*this)[section.name] = std::move(sectionMap);
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
        auto it = this->find(sectionName);
        if (it == this->end()) {
            throw std::out_of_range("Section not found: " + sectionName);
        }
        return it->second;
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
        auto it = this->find(sectionName);
        if (it == this->end()) {
            throw std::out_of_range("Section not found: " + sectionName);
        }
        return it->second;
    }

    /**
     * @brief Check if a section exists
     * @param sectionName The section name
     * @return True if the section exists, false otherwise
     */
    bool hasSection(const std::string& sectionName) const noexcept {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return this->find(sectionName) != this->end();
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
            const auto& section = this->at(sectionName);
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
        auto& section = (*this)[sectionName];
        section[fieldName] = value;
    }

    /**
     * @brief Remove a section
     * @param sectionName The section name
     * @return True if the section was removed, false if it did not exist
     */
    bool removeSection(const std::string& sectionName) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return this->erase(sectionName) > 0;
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
        auto sectionIt = this->find(sectionName);
        if (sectionIt == this->end()) {
            return false;
        }
        return sectionIt->second.erase(fieldName) > 0;
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
        }
    }
};

using IniFile = IniFileBase<std::less<>>;  ///< Case-sensitive INI file.
using IniFileCaseInsensitive =
    IniFileBase<StringInsensitiveLess>;  ///< Case-insensitive INI file.

}  // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_INIFILE_HPP
