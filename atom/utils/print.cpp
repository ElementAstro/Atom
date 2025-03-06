#include "print.hpp"

#include <algorithm>
#include <future>
#include <stdexcept>

namespace atom::utils {

// Initialize static members
std::unique_ptr<Logger> Logger::instance = nullptr;
std::once_flag Logger::initInstanceFlag;

void printProgressBar(float progress, int barWidth, ProgressBarStyle style) {
    // Input validation
    if (progress < 0.0f)
        progress = 0.0f;
    if (progress > 1.0f)
        progress = 1.0f;
    if (barWidth <= 0)
        barWidth = DEFAULT_BAR_WIDTH;

    const int pos = static_cast<int>(barWidth * progress);
    const int percentage = static_cast<int>(progress * PERCENTAGE_MULTIPLIER);

    try {
        switch (style) {
            case ProgressBarStyle::BASIC: {
                std::cout << "[";
                for (int i = 0; i < barWidth; ++i) {
                    if (i < pos)
                        std::cout << "=";
                    else if (i == pos)
                        std::cout << ">";
                    else
                        std::cout << " ";
                }
                std::cout << "] " << percentage << " %\r";
                break;
            }

            case ProgressBarStyle::BLOCK: {
                std::cout << "[";
                for (int i = 0; i < barWidth; ++i) {
                    if (i < pos)
                        std::cout << "█";
                    else if (i == pos && pos < barWidth)
                        std::cout << "▓";
                    else
                        std::cout << " ";
                }
                std::cout << "] " << percentage << " %\r";
                break;
            }

            case ProgressBarStyle::ARROW: {
                std::cout << "[";
                for (int i = 0; i < barWidth; ++i) {
                    if (i < pos)
                        std::cout << "→";
                    else
                        std::cout << " ";
                }
                std::cout << "] " << percentage << " %\r";
                break;
            }

            case ProgressBarStyle::PERCENTAGE: {
                std::cout << percentage << "% completed\r";
                break;
            }
        }

        std::cout.flush();
    } catch (const std::exception& e) {
        std::cerr << "Error rendering progress bar: " << e.what() << std::endl;
    }
}

void printTable(const std::vector<std::vector<std::string>>& data) {
    if (data.empty()) {
        return;
    }

    try {
        // Validate data structure
        size_t columns = data[0].size();
        for (const auto& row : data) {
            if (row.size() != columns) {
                throw std::invalid_argument(
                    "All rows must have the same number of columns");
            }
        }

        // Calculate column widths using parallel algorithm for large tables
        std::vector<size_t> colWidths(columns, 0);

        if (data.size() > 100) {
            // For large tables, use parallel algorithm
            std::vector<std::vector<size_t>> threadColWidths(
                std::min(static_cast<size_t>(8), data.size()),
                std::vector<size_t>(columns, 0));

            std::vector<std::future<void>> futures;

            size_t rows_per_thread = data.size() / threadColWidths.size();

            for (size_t t = 0; t < threadColWidths.size(); ++t) {
                futures.push_back(std::async(std::launch::async, [&, t]() {
                    size_t start_idx = t * rows_per_thread;
                    size_t end_idx = (t == threadColWidths.size() - 1)
                                         ? data.size()
                                         : start_idx + rows_per_thread;

                    for (size_t r = start_idx; r < end_idx; ++r) {
                        for (size_t c = 0; c < columns; ++c) {
                            threadColWidths[t][c] = std::max(
                                threadColWidths[t][c], data[r][c].length());
                        }
                    }
                }));
            }

            for (auto& future : futures) {
                future.wait();
            }

            // Combine results
            for (size_t t = 0; t < threadColWidths.size(); ++t) {
                for (size_t c = 0; c < columns; ++c) {
                    colWidths[c] =
                        std::max(colWidths[c], threadColWidths[t][c]);
                }
            }
        } else {
            // For small tables, use direct approach
            for (const auto& row : data) {
                for (size_t i = 0; i < row.size(); ++i) {
                    colWidths[i] = std::max(colWidths[i], row[i].length());
                }
            }
        }

        // Print table with validated width
        for (size_t r = 0; r < data.size(); ++r) {
            const auto& row = data[r];

            // Print row data
            for (size_t i = 0; i < row.size(); ++i) {
                std::cout << "| " << std::setw(static_cast<int>(colWidths[i]))
                          << std::left << row[i] << " ";
            }
            std::cout << "|" << std::endl;

            // Print header separator
            if (r == 0) {
                for (size_t i = 0; i < row.size(); ++i) {
                    std::cout << "+-" << std::string(colWidths[i], '-') << "-";
                }
                std::cout << "+" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error printing table: " << e.what() << std::endl;
    }
}

void printJson(const std::string& json, int indent) {
    if (indent < 0) {
        std::cerr << "Warning: Negative indent value. Using default value of 2"
                  << std::endl;
        indent = 2;
    }

    if (json.empty()) {
        std::cout << "{}" << std::endl;
        return;
    }

    try {
        int level = 0;
        bool inQuotes = false;
        bool inEscape = false;
        std::string buffer;
        buffer.reserve(json.length() *
                       2);  // Pre-allocate buffer to avoid reallocations

        for (char character : json) {
            if (inEscape) {
                buffer += character;
                inEscape = false;
            } else {
                switch (character) {
                    case '{':
                    case '[':
                        buffer += character;
                        buffer += '\n';
                        level++;
                        buffer += std::string(
                            static_cast<size_t>(level) * indent, ' ');
                        break;
                    case '}':
                    case ']':
                        buffer += '\n';
                        level--;
                        buffer += std::string(
                            static_cast<size_t>(level) * indent, ' ');
                        buffer += character;
                        break;
                    case ',':
                        buffer += character;
                        if (!inQuotes) {
                            buffer += '\n';
                            buffer += std::string(
                                static_cast<size_t>(level) * indent, ' ');
                        }
                        break;
                    case ':':
                        buffer += character;
                        if (!inQuotes) {
                            buffer += ' ';
                        }
                        break;
                    case '\"':
                        if (!inEscape) {
                            inQuotes = !inQuotes;
                        }
                        buffer += character;
                        break;
                    case '\\':
                        buffer += character;
                        if (inQuotes) {
                            inEscape = true;
                        }
                        break;
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                        // Skip whitespace outside of quotes
                        if (inQuotes) {
                            buffer += character;
                        }
                        break;
                    default:
                        buffer += character;
                }
            }
        }

        std::cout << buffer << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        // Print the raw JSON as a fallback
        std::cout << json << std::endl;
    }
}

void printBarChart(const std::map<std::string, int>& data, int maxWidth) {
    if (data.empty()) {
        std::cout << "No data to display" << std::endl;
        return;
    }

    // Input validation
    if (maxWidth <= 0) {
        maxWidth = DEFAULT_BAR_WIDTH;
    }

    try {
        // Find max value using std::ranges
        auto max_iter = std::max_element(
            data.begin(), data.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        if (max_iter == data.end()) {
            throw std::runtime_error("Failed to find maximum value");
        }

        int maxValue = max_iter->second;
        if (maxValue <= 0) {
            // Handle edge case of all zero or negative values
            std::cout << "All values are zero or negative" << std::endl;
            for (const auto& [label, value] : data) {
                std::cout << std::setw(MAX_LABEL_WIDTH) << std::left << label
                          << " | " << value << std::endl;
            }
            return;
        }

        // Cache label width for better formatting
        size_t maxLabelWidth = MAX_LABEL_WIDTH;
        for (const auto& [label, _] : data) {
            maxLabelWidth = std::max(maxLabelWidth, label.length());
        }
        maxLabelWidth =
            std::min(maxLabelWidth,
                     static_cast<size_t>(MAX_LABEL_WIDTH * 2));  // Cap width

        // Print chart header
        std::cout << std::string(maxLabelWidth + 2, '-') << "+";
        std::cout << std::string(maxWidth + 7, '-') << std::endl;

        // Print each bar
        for (const auto& [label, value] : data) {
            // Calculate bar width based on value proportion to max
            double ratio =
                static_cast<double>(value) / static_cast<double>(maxValue);
            int barWidth = static_cast<int>(std::floor(ratio * maxWidth));

            // Print label with right padding
            std::cout << std::setw(static_cast<int>(maxLabelWidth)) << std::left
                      << (label.length() > maxLabelWidth
                              ? label.substr(0, maxLabelWidth - 3) + "..."
                              : label);

            // Print bar
            std::cout << " | ";
            if (barWidth > 0) {
                // Use different colors based on ratio
                if (ratio > 0.8) {
                    std::cout << "\033[31m";  // Red for high values
                } else if (ratio > 0.5) {
                    std::cout << "\033[33m";  // Yellow for medium values
                } else {
                    std::cout << "\033[32m";  // Green for low values
                }

                std::cout << std::string(barWidth, '#');
                std::cout << "\033[0m";  // Reset color
            }

            std::cout << std::string(maxWidth - barWidth, ' ');

            // Print value
            std::cout << " | " << value << std::endl;
        }

        // Print footer
        std::cout << std::string(maxLabelWidth + 2, '-') << "+";
        std::cout << std::string(maxWidth + 7, '-') << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error creating bar chart: " << e.what() << std::endl;
    }
}

}  // namespace atom::utils
