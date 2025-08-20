#include "print.hpp"

#include <algorithm>
#include <future>
#include <stdexcept>

namespace atom::utils {

std::unique_ptr<Logger> Logger::instance_ = nullptr;
std::once_flag Logger::init_instance_flag_;

void printProgressBar(float progress, int bar_width, ProgressBarStyle style) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    if (bar_width <= 0) {
        bar_width = DEFAULT_BAR_WIDTH;
    }

    const auto pos = static_cast<int>(bar_width * progress);
    const auto percentage = static_cast<int>(progress * PERCENTAGE_MULTIPLIER);

    try {
        switch (style) {
            case ProgressBarStyle::BASIC: {
                std::cout << "[";
                for (int i = 0; i < bar_width; ++i) {
                    if (i < pos) {
                        std::cout << "=";
                    } else if (i == pos) {
                        std::cout << ">";
                    } else {
                        std::cout << " ";
                    }
                }
                std::cout << "] " << percentage << " %\r";
                break;
            }

            case ProgressBarStyle::BLOCK: {
                std::cout << "[";
                for (int i = 0; i < bar_width; ++i) {
                    if (i < pos) {
                        std::cout << "█";
                    } else if (i == pos && pos < bar_width) {
                        std::cout << "▓";
                    } else {
                        std::cout << " ";
                    }
                }
                std::cout << "] " << percentage << " %\r";
                break;
            }

            case ProgressBarStyle::ARROW: {
                std::cout << "[";
                for (int i = 0; i < bar_width; ++i) {
                    std::cout << (i < pos ? "→" : " ");
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
        const auto columns = data[0].size();

        for (const auto& row : data) {
            if (row.size() != columns) {
                throw std::invalid_argument(
                    "All rows must have the same number of columns");
            }
        }

        std::vector<size_t> col_widths(columns, 0);

        if (data.size() > 100) {
            const auto num_threads = std::min<size_t>(8, data.size());
            std::vector<std::vector<size_t>> thread_col_widths(
                num_threads, std::vector<size_t>(columns, 0));
            std::vector<std::future<void>> futures;

            const auto rows_per_thread = data.size() / num_threads;

            for (size_t t = 0; t < num_threads; ++t) {
                futures.push_back(std::async(std::launch::async, [&, t]() {
                    const auto start_idx = t * rows_per_thread;
                    const auto end_idx = (t == num_threads - 1)
                                             ? data.size()
                                             : start_idx + rows_per_thread;

                    for (size_t r = start_idx; r < end_idx; ++r) {
                        for (size_t c = 0; c < columns; ++c) {
                            thread_col_widths[t][c] = std::max(
                                thread_col_widths[t][c], data[r][c].length());
                        }
                    }
                }));
            }

            for (auto& future : futures) {
                future.wait();
            }

            for (size_t t = 0; t < num_threads; ++t) {
                for (size_t c = 0; c < columns; ++c) {
                    col_widths[c] =
                        std::max(col_widths[c], thread_col_widths[t][c]);
                }
            }
        } else {
            for (const auto& row : data) {
                for (size_t i = 0; i < row.size(); ++i) {
                    col_widths[i] = std::max(col_widths[i], row[i].length());
                }
            }
        }

        for (size_t r = 0; r < data.size(); ++r) {
            const auto& row = data[r];

            for (size_t i = 0; i < row.size(); ++i) {
                std::cout << "| " << std::setw(static_cast<int>(col_widths[i]))
                          << std::left << row[i] << " ";
            }
            std::cout << "|\n";

            if (r == 0) {
                for (size_t i = 0; i < row.size(); ++i) {
                    std::cout << "+-" << std::string(col_widths[i], '-') << "-";
                }
                std::cout << "+\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error printing table: " << e.what() << std::endl;
    }
}

void printJson(const std::string& json, int indent) {
    if (indent < 0) {
        std::cerr
            << "Warning: Negative indent value. Using default value of 2\n";
        indent = 2;
    }

    if (json.empty()) {
        std::cout << "{}\n";
        return;
    }

    try {
        int level = 0;
        bool in_quotes = false;
        bool in_escape = false;
        std::string buffer;
        buffer.reserve(json.length() * 2);

        for (const char character : json) {
            if (in_escape) {
                buffer += character;
                in_escape = false;
                continue;
            }

            switch (character) {
                case '{':
                case '[':
                    buffer += character;
                    buffer += '\n';
                    ++level;
                    buffer +=
                        std::string(static_cast<size_t>(level) * indent, ' ');
                    break;

                case '}':
                case ']':
                    buffer += '\n';
                    --level;
                    buffer +=
                        std::string(static_cast<size_t>(level) * indent, ' ');
                    buffer += character;
                    break;

                case ',':
                    buffer += character;
                    if (!in_quotes) {
                        buffer += '\n';
                        buffer += std::string(
                            static_cast<size_t>(level) * indent, ' ');
                    }
                    break;

                case ':':
                    buffer += character;
                    if (!in_quotes) {
                        buffer += ' ';
                    }
                    break;

                case '\"':
                    if (!in_escape) {
                        in_quotes = !in_quotes;
                    }
                    buffer += character;
                    break;

                case '\\':
                    buffer += character;
                    if (in_quotes) {
                        in_escape = true;
                    }
                    break;

                case ' ':
                case '\n':
                case '\r':
                case '\t':
                    if (in_quotes) {
                        buffer += character;
                    }
                    break;

                default:
                    buffer += character;
            }
        }

        std::cout << buffer << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        std::cout << json << std::endl;
    }
}

void printBarChart(const std::map<std::string, int>& data, int max_width) {
    if (data.empty()) {
        std::cout << "No data to display\n";
        return;
    }

    if (max_width <= 0) {
        max_width = DEFAULT_BAR_WIDTH;
    }

    try {
        const auto max_iter = std::max_element(
            data.begin(), data.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        if (max_iter == data.end()) {
            throw std::runtime_error("Failed to find maximum value");
        }

        const auto max_value = max_iter->second;
        if (max_value <= 0) {
            std::cout << "All values are zero or negative\n";
            for (const auto& [label, value] : data) {
                std::cout << std::setw(MAX_LABEL_WIDTH) << std::left << label
                          << " | " << value << '\n';
            }
            return;
        }

        auto max_label_width = MAX_LABEL_WIDTH;
        for (const auto& [label, _] : data) {
            max_label_width =
                std::max(max_label_width, static_cast<int>(label.length()));
        }
        max_label_width = std::min(max_label_width, MAX_LABEL_WIDTH * 2);

        std::cout << std::string(max_label_width + 2, '-') << "+"
                  << std::string(max_width + 7, '-') << '\n';

        for (const auto& [label, value] : data) {
            const auto ratio =
                static_cast<double>(value) / static_cast<double>(max_value);
            const auto bar_width =
                static_cast<int>(std::floor(ratio * max_width));

            const auto display_label =
                (label.length() > static_cast<size_t>(max_label_width))
                    ? label.substr(0, max_label_width - 3) + "..."
                    : label;

            std::cout << std::setw(max_label_width) << std::left
                      << display_label << " | ";

            if (bar_width > 0) {
                if (ratio > 0.8) {
                    std::cout << "\033[31m";
                } else if (ratio > 0.5) {
                    std::cout << "\033[33m";
                } else {
                    std::cout << "\033[32m";
                }

                std::cout << std::string(bar_width, '#') << "\033[0m";
            }

            std::cout << std::string(max_width - bar_width, ' ') << " | "
                      << value << '\n';
        }

        std::cout << std::string(max_label_width + 2, '-') << "+"
                  << std::string(max_width + 7, '-') << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error creating bar chart: " << e.what() << std::endl;
    }
}

}  // namespace atom::utils
