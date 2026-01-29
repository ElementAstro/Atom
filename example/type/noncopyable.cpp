#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/type/noncopyable.hpp"

// Helper function to print section headersvoid print_header(const std::string&
// title) {
std::cout << "\n=== " << title << " ===" << std::endl;
std::cout << std::string(title.length() + 8, '=') << std::endl;
}

// Example 1: Resource manager that should not be copiedclass FileManager :
// public NonCopyable {
private:
std::string filename_;
bool is_open_;

public:
explicit FileManager(const std::string& filename)
    : filename_(filename), is_open_(false) {
    std::cout << "FileManager created for: " << filename_ << std::endl;
}

~FileManager() {
    if (is_open_) {
        close();
    }
    std::cout << "FileManager destroyed for: " << filename_ << std::endl;
}

void open() {
    if (!is_open_) {
        is_open_ = true;
        std::cout << "File opened: " << filename_ << std::endl;
    }
}

void close() {
    if (is_open_) {
        is_open_ = false;
        std::cout << "File closed: " << filename_ << std::endl;
    }
}

bool isOpen() const { return is_open_; }
const std::string& getFilename() const { return filename_; }
}
;

// Example 2: Singleton pattern using NonCopyableclass DatabaseConnection :
// public NonCopyable {
private:
std::string connection_string_;
bool connected_;
static std::unique_ptr<DatabaseConnection> instance_;

explicit DatabaseConnection(const std::string& conn_str)
    : connection_string_(conn_str), connected_(false) {
    std::cout << "DatabaseConnection created with: " << conn_str << std::endl;
}

public:
static DatabaseConnection& getInstance(
    const std::string& conn_str = "default://localhost") {
    if (!instance_) {
        instance_ = std::unique_ptr<DatabaseConnection>(
            new DatabaseConnection(conn_str));
    }
    return *instance_;
}

void connect() {
    if (!connected_) {
        connected_ = true;
        std::cout << "Connected to database: " << connection_string_
                  << std::endl;
    }
}

void disconnect() {
    if (connected_) {
        connected_ = false;
        std::cout << "Disconnected from database" << std::endl;
    }
}

bool isConnected() const { return connected_; }
const std::string& getConnectionString() const { return connection_string_; }
}
;

// Static member definitionstd::unique_ptr<DatabaseConnection>
// DatabaseConnection::instance_ = nullptr;

// Example 3: Thread-safe counter that should not be copiedclass
// ThreadSafeCounter : public NonCopyable {
private:
mutable std::mutex mutex_;
int count_;
std::string name_;

public:
explicit ThreadSafeCounter(const std::string& name, int initial_value = 0)
    : count_(initial_value), name_(name) {
    std::cout << "ThreadSafeCounter '" << name_
              << "' created with value: " << initial_value << std::endl;
}

~ThreadSafeCounter() {
    std::cout << "ThreadSafeCounter '" << name_
              << "' destroyed with final value: " << count_ << std::endl;
}

void increment() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++count_;
    std::cout << "Counter '" << name_ << "' incremented to: " << count_
              << std::endl;
}

void decrement() {
    std::lock_guard<std::mutex> lock(mutex_);
    --count_;
    std::cout << "Counter '" << name_ << "' decremented to: " << count_
              << std::endl;
}

int getValue() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

const std::string& getName() const { return name_; }
}
;

// Example 4: RAII wrapper that should not be copiedtemplate <typename T>
class RAIIWrapper : public NonCopyable {
private:
    T* resource_;
    std::string description_;

public:
    explicit RAIIWrapper(T* resource, const std::string& desc)
        : resource_(resource), description_(desc) {
        std::cout << "RAII wrapper acquired: " << description_ << std::endl;
    }

    ~RAIIWrapper() {
        if (resource_) {
            delete resource_;
            std::cout << "RAII wrapper released: " << description_ << std::endl;
        }
    }

    T* get() const { return resource_; }
    T& operator*() const { return *resource_; }
    T* operator->() const { return resource_; }

    // Move semantics are allowed
    RAIIWrapper(RAIIWrapper&& other) noexcept
        : resource_(other.resource_),
          description_(std::move(other.description_)) {
        other.resource_ = nullptr;
        std::cout << "RAII wrapper moved: " << description_ << std::endl;
    }

    RAIIWrapper& operator=(RAIIWrapper&& other) noexcept {
        if (this != &other) {
            if (resource_) {
                delete resource_;
            }
            resource_ = other.resource_;
            description_ = std::move(other.description_);
            other.resource_ = nullptr;
            std::cout << "RAII wrapper move-assigned: " << description_
                      << std::endl;
        }
        return *this;
    }
};

// Function to demonstrate move semanticsRAIIWrapper<std::string>
// createStringWrapper(const std::string& value) {
return RAIIWrapper<std::string>(new std::string(value), "String: " + value);
}

int main() {
    std::cout << "NonCopyable Usage Examples" << std::endl;
    std::cout << "==========================" << std::endl;

    // 1. Basic NonCopyable Usage
    print_header("Basic NonCopyable Usage");

    {
        FileManager fm("test.txt");
        fm.open();

        // This would cause a compilation error:
        // FileManager fm2 = fm;  // Error: copy constructor is deleted
        // FileManager fm3(fm);   // Error: copy constructor is deleted

        std::cout << "File manager is working with: " << fm.getFilename()
                  << std::endl;
        std::cout << "File is open: " << (fm.isOpen() ? "Yes" : "No")
                  << std::endl;
    }  // FileManager destructor called here

    // 2. Singleton Pattern
    print_header("Singleton Pattern");

    {
        auto& db1 =
            DatabaseConnection::getInstance("postgresql://localhost:5432");
        auto& db2 = DatabaseConnection::getInstance(
            "mysql://localhost:3306");  // Same instance

        std::cout << "db1 connection string: " << db1.getConnectionString()
                  << std::endl;
        std::cout << "db2 connection string: " << db2.getConnectionString()
                  << std::endl;
        std::cout << "Are db1 and db2 the same instance? "
                  << (&db1 == &db2 ? "Yes" : "No") << std::endl;

        db1.connect();
        std::cout << "db2 is connected: " << (db2.isConnected() ? "Yes" : "No")
                  << std::endl;
    }

    // 3. Thread-Safe Counter
    print_header("Thread-Safe Counter");

    {
        ThreadSafeCounter counter("MainCounter", 10);

        counter.increment();
        counter.increment();
        counter.decrement();

        std::cout << "Final counter value: " << counter.getValue() << std::endl;

        // This would cause a compilation error:
        // ThreadSafeCounter counter2 = counter;  // Error: copy constructor is
        // deleted
    }

    // 4. RAII Wrapper with Move Semantics
    print_header("RAII Wrapper with Move Semantics");

    {
        // Create RAII wrapper
        RAIIWrapper<std::string> wrapper(new std::string("Hello, World!"),
                                         "Test String");
        std::cout << "Wrapper content: " << *wrapper << std::endl;

        // Move semantics work
        auto moved_wrapper = std::move(wrapper);
        std::cout << "Moved wrapper content: " << *moved_wrapper << std::endl;

        // Create through function (demonstrates move)
        auto func_wrapper = createStringWrapper("Function Created");
        std::cout << "Function wrapper content: " << *func_wrapper << std::endl;

        // This would cause a compilation error:
        // auto copied_wrapper = moved_wrapper;  // Error: copy constructor is
        // deleted
    }

    // 5. Container of NonCopyable Objects
    print_header("Container of NonCopyable Objects");

    {
        std::vector<std::unique_ptr<FileManager>> file_managers;

        // Add file managers using move semantics
        file_managers.push_back(std::make_unique<FileManager>("file1.txt"));
        file_managers.push_back(std::make_unique<FileManager>("file2.txt"));
        file_managers.push_back(std::make_unique<FileManager>("file3.txt"));

        std::cout << "Created " << file_managers.size() << " file managers"
                  << std::endl;

        // Open all files
        for (auto& fm : file_managers) {
            fm->open();
        }

        std::cout << "All files opened" << std::endl;
    }  // All file managers destroyed here

    // 6. Demonstrating Compilation Errors (commented out)
    print_header("Compilation Error Examples (Commented Out)");

    std::cout << "The following code would cause compilation errors:"
              << std::endl;
    std::cout << std::endl;
    std::cout << "// FileManager fm1(\"test.txt\");" << std::endl;
    std::cout << "// FileManager fm2 = fm1;           // Error: copy "
                 "constructor deleted"
              << std::endl;
    std::cout << "// FileManager fm3(fm1);            // Error: copy "
                 "constructor deleted"
              << std::endl;
    std::cout << "// fm2 = fm1;                       // Error: copy "
                 "assignment deleted"
              << std::endl;
    std::cout << std::endl;
    std::cout << "// ThreadSafeCounter c1(\"test\");" << std::endl;
    std::cout << "// ThreadSafeCounter c2 = c1;       // Error: copy "
                 "constructor deleted"
              << std::endl;
    std::cout << "// c2 = c1;                         // Error: copy "
                 "assignment deleted"
              << std::endl;

    std::cout << "\nAll NonCopyable examples completed successfully!"
              << std::endl;
    return 0;
}
