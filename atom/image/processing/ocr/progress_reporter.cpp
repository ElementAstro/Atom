#include "progress_reporter.hpp"

#include <iostream>
#include <sstream>

namespace atom::image::ocr {

ProgressReporter::ProgressReporter(std::string taskName, size_t total)
    : m_taskName(std::move(taskName)),
      m_total(total),
      m_startTime(std::chrono::steady_clock::now()) {}

void ProgressReporter::update(size_t increment) {
    m_current += increment;
    reportProgress();
}

void ProgressReporter::setTotal(size_t total) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_total = total;
}

void ProgressReporter::reportProgress() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime)
            .count();

    if (m_total > 0) {
        size_t current = m_current.load();
        float percentage = static_cast<float>(current) * 100.0f / m_total;

        // Calculate ETA
        std::string eta = "N/A";
        if (current > 0 && elapsed > 0) {
            float itemsPerSecond = static_cast<float>(current) / elapsed;
            if (itemsPerSecond > 0) {
                int etaSeconds =
                    static_cast<int>((m_total - current) / itemsPerSecond);
                std::ostringstream oss;
                oss << (etaSeconds / 60) << "m " << (etaSeconds % 60) << "s";
                eta = oss.str();
            }
        }

        std::cout << "\r" << m_taskName << ": " << percentage << "% ("
                  << current << "/" << m_total << ") - Elapsed: " << elapsed
                  << "s - ETA: " << eta;
        std::cout.flush();

        if (current >= m_total) {
            std::cout << std::endl;
        }
    }
}

float ProgressReporter::getPercentage() const {
    if (m_total == 0)
        return 0.0f;
    return static_cast<float>(m_current.load()) * 100.0f / m_total;
}

int64_t ProgressReporter::getElapsedSeconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime)
        .count();
}

int64_t ProgressReporter::getETASeconds() const {
    size_t current = m_current.load();
    if (current == 0 || m_total == 0)
        return -1;

    auto elapsed = getElapsedSeconds();
    if (elapsed == 0)
        return -1;

    float itemsPerSecond = static_cast<float>(current) / elapsed;
    if (itemsPerSecond <= 0)
        return -1;

    return static_cast<int64_t>((m_total - current) / itemsPerSecond);
}

void ProgressReporter::reset() {
    m_current = 0;
    m_startTime = std::chrono::steady_clock::now();
}

bool ProgressReporter::isComplete() const {
    return m_current.load() >= m_total;
}

}  // namespace atom::image::ocr
