#include "atom/log/atomlog.hpp"

int main() {
    using namespace atom::log;

    // Create a logger instance with a log file name, minimum log level, max
    // file size, and max files.
    Logger logger("logfile.log", LogLevel::DEBUG, 1048576, 5);

    // Set the logging level to INFO.
    logger.setLevel(LogLevel::INFO);

    // Set a custom logging pattern.
    logger.setPattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

    // Set the thread name for logging.
    logger.setThreadName("MainThread");

    // Register a custom log level.
    logger.registerCustomLogLevel("CUSTOM", 7);

    // Enable system logging.
    logger.enableSystemLogging(true);

    // Log messages with different severity levels.
    logger.trace("This is a trace message with value: {}", 42);
    logger.debug("This is a debug message with value: {}", 42);
    logger.info("This is an info message with value: {}", 42);
    logger.warn("This is a warn message with value: {}", 42);
    logger.error("This is an error message with value: {}", 42);
    logger.critical("This is a critical message with value: {}", 42);

    // Register a sink logger.
    auto sinkLogger = std::make_shared<Logger>("sinklog.log", LogLevel::DEBUG);
    logger.registerSink(sinkLogger);

    // Log a message that will also be sent to the sink logger.
    logger.info("This message will be logged to both loggers.");

    // Remove the sink logger.
    logger.removeSink(sinkLogger);

    // Clear all registered sink loggers.
    logger.clearSinks();

    return 0;
}
