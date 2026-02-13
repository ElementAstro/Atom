#include "atom/error/exception.hpp"

#include <iostream>

void testException() {
    try {
        // Throw a general exception with a custom message
        THROW_EXCEPTION("General exception occurred with value: ", 42);
    } catch (const atom::error::Exception &e) {
        std::cerr << "Caught Exception: " << e.what() << std::endl;
    }

    try {
        // Throw a nested exception
        try {
            THROW_EXCEPTION("Inner exception");
        } catch (...) {
            THROW_NESTED_EXCEPTION("Outer exception wrapping inner exception");
        }
    } catch (const atom::error::Exception &e) {
        std::cerr << "Caught Nested Exception: " << e.what() << std::endl;
    }

    try {
        // Throw a system error exception
        THROW_SYSTEM_ERROR(1, "System error occurred");
    } catch (const atom::error::SystemErrorException &e) {
        std::cerr << "Caught System Error Exception: " << e.what() << std::endl;
    }

    try {
        // Throw a runtime error
        THROW_RUNTIME_ERROR("Runtime error occurred");
    } catch (const atom::error::RuntimeError &e) {
        std::cerr << "Caught Runtime Error: " << e.what() << std::endl;
    }

    try {
        // Throw a logic error
        THROW_LOGIC_ERROR("Logic error occurred");
    } catch (const atom::error::LogicError &e) {
        std::cerr << "Caught Logic Error: " << e.what() << std::endl;
    }

    try {
        // Throw an unlawful operation error
        THROW_UNLAWFUL_OPERATION("Unlawful operation occurred");
    } catch (const atom::error::UnlawfulOperation &e) {
        std::cerr << "Caught Unlawful Operation Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw an out of range error
        THROW_OUT_OF_RANGE("Out of range error occurred");
    } catch (const atom::error::OutOfRange &e) {
        std::cerr << "Caught Out Of Range Error: " << e.what() << std::endl;
    }

    try {
        // Throw an overflow error
        THROW_OVERFLOW("Overflow error occurred");
    } catch (const atom::error::OverflowException &e) {
        std::cerr << "Caught Overflow Error: " << e.what() << std::endl;
    }

    try {
        // Throw an underflow error
        THROW_UNDERFLOW("Underflow error occurred");
    } catch (const atom::error::UnderflowException &e) {
        std::cerr << "Caught Underflow Error: " << e.what() << std::endl;
    }

    try {
        // Throw a length error
        THROW_LENGTH("Length error occurred");
    } catch (const atom::error::LengthException &e) {
        std::cerr << "Caught Length Error: " << e.what() << std::endl;
    }

    try {
        // Throw an unknown error
        THROW_UNKOWN("Unknown error occurred");
    } catch (const atom::error::Unkown &e) {
        std::cerr << "Caught Unknown Error: " << e.what() << std::endl;
    }

    try {
        // Throw an object already exist error
        THROW_OBJ_ALREADY_EXIST("Object already exists error occurred");
    } catch (const atom::error::ObjectAlreadyExist &e) {
        std::cerr << "Caught Object Already Exist Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw an object already initialized error
        THROW_OBJ_ALREADY_INITIALIZED(
            "Object already initialized error occurred");
    } catch (const atom::error::ObjectAlreadyInitialized &e) {
        std::cerr << "Caught Object Already Initialized Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw an object not exist error
        THROW_OBJ_NOT_EXIST("Object not exist error occurred");
    } catch (const atom::error::ObjectNotExist &e) {
        std::cerr << "Caught Object Not Exist Error: " << e.what() << std::endl;
    }

    try {
        // Throw an object uninitialized error
        THROW_OBJ_UNINITIALIZED("Object uninitialized error occurred");
    } catch (const atom::error::ObjectUninitialized &e) {
        std::cerr << "Caught Object Uninitialized Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a null pointer error
        THROW_NULL_POINTER("Null pointer error occurred");
    } catch (const atom::error::NullPointer &e) {
        std::cerr << "Caught Null Pointer Error: " << e.what() << std::endl;
    }

    try {
        // Throw a not found error
        THROW_NOT_FOUND("Not found error occurred");
    } catch (const atom::error::NotFound &e) {
        std::cerr << "Caught Not Found Error: " << e.what() << std::endl;
    }

    try {
        // Throw a wrong argument error
        THROW_WRONG_ARGUMENT("Wrong argument error occurred");
    } catch (const atom::error::WrongArgument &e) {
        std::cerr << "Caught Wrong Argument Error: " << e.what() << std::endl;
    }

    try {
        // Throw an invalid argument error
        THROW_INVALID_ARGUMENT("Invalid argument error occurred");
    } catch (const atom::error::InvalidArgument &e) {
        std::cerr << "Caught Invalid Argument Error: " << e.what() << std::endl;
    }

    try {
        // Throw a missing argument error
        THROW_MISSING_ARGUMENT("Missing argument error occurred");
    } catch (const atom::error::MissingArgument &e) {
        std::cerr << "Caught Missing Argument Error: " << e.what() << std::endl;
    }

    try {
        // Throw a file not found error
        THROW_FILE_NOT_FOUND("File not found error occurred");
    } catch (const atom::error::FileNotFound &e) {
        std::cerr << "Caught File Not Found Error: " << e.what() << std::endl;
    }

    try {
        // Throw a file not readable error
        THROW_FILE_NOT_READABLE("File not readable error occurred");
    } catch (const atom::error::FileNotReadable &e) {
        std::cerr << "Caught File Not Readable Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a file not writable error
        THROW_FILE_NOT_WRITABLE("File not writable error occurred");
    } catch (const atom::error::FileNotWritable &e) {
        std::cerr << "Caught File Not Writable Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to open file error
        THROW_FAIL_TO_OPEN_FILE("Fail to open file error occurred");
    } catch (const atom::error::FailToOpenFile &e) {
        std::cerr << "Caught Fail To Open File Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to close file error
        THROW_FAIL_TO_CLOSE_FILE("Fail to close file error occurred");
    } catch (const atom::error::FailToCloseFile &e) {
        std::cerr << "Caught Fail To Close File Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to create file error
        THROW_FAIL_TO_CREATE_FILE("Fail to create file error occurred");
    } catch (const atom::error::FailToCreateFile &e) {
        std::cerr << "Caught Fail To Create File Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to delete file error
        THROW_FAIL_TO_DELETE_FILE("Fail to delete file error occurred");
    } catch (const atom::error::FailToDeleteFile &e) {
        std::cerr << "Caught Fail To Delete File Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to copy file error
        THROW_FAIL_TO_COPY_FILE("Fail to copy file error occurred");
    } catch (const atom::error::FailToCopyFile &e) {
        std::cerr << "Caught Fail To Copy File Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to move file error
        THROW_FAIL_TO_MOVE_FILE("Fail to move file error occurred");
    } catch (const atom::error::FailToMoveFile &e) {
        std::cerr << "Caught Fail To Move File Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to read file error
        THROW_FAIL_TO_READ_FILE("Fail to read file error occurred");
    } catch (const atom::error::FailToReadFile &e) {
        std::cerr << "Caught Fail To Read File Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to write file error
        THROW_FAIL_TO_WRITE_FILE("Fail to write file error occurred");
    } catch (const atom::error::FailToWriteFile &e) {
        std::cerr << "Caught Fail To Write File Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to load DLL error
        THROW_FAIL_TO_LOAD_DLL("Fail to load DLL error occurred");
    } catch (const atom::error::FailToLoadDll &e) {
        std::cerr << "Caught Fail To Load DLL Error: " << e.what() << std::endl;
    }

    try {
        // Throw a fail to unload DLL error
        THROW_FAIL_TO_UNLOAD_DLL("Fail to unload DLL error occurred");
    } catch (const atom::error::FailToUnloadDll &e) {
        std::cerr << "Caught Fail To Unload DLL Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to load symbol error
        THROW_FAIL_TO_LOAD_SYMBOL("Fail to load symbol error occurred");
    } catch (const atom::error::FailToLoadSymbol &e) {
        std::cerr << "Caught Fail To Load Symbol Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to create process error
        THROW_FAIL_TO_CREATE_PROCESS("Fail to create process error occurred");
    } catch (const atom::error::FailToCreateProcess &e) {
        std::cerr << "Caught Fail To Create Process Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a fail to terminate process error
        THROW_FAIL_TO_TERMINATE_PROCESS(
            "Fail to terminate process error occurred");
    } catch (const atom::error::FailToTerminateProcess &e) {
        std::cerr << "Caught Fail To Terminate Process Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a JSON parse error
        THROW_JSON_PARSE_ERROR("JSON parse error occurred");
    } catch (const atom::error::JsonParseError &e) {
        std::cerr << "Caught JSON Parse Error: " << e.what() << std::endl;
    }

    try {
        // Throw a JSON value error
        THROW_JSON_VALUE_ERROR("JSON value error occurred");
    } catch (const atom::error::JsonValueError &e) {
        std::cerr << "Caught JSON Value Error: " << e.what() << std::endl;
    }

    try {
        // Throw a CURL initialization error
        THROW_CURL_INITIALIZATION_ERROR("CURL initialization error occurred");
    } catch (const atom::error::CurlInitializationError &e) {
        std::cerr << "Caught CURL Initialization Error: " << e.what()
                  << std::endl;
    }

    try {
        // Throw a CURL runtime error
        THROW_CURL_RUNTIME_ERROR("CURL runtime error occurred");
    } catch (const atom::error::CurlRuntimeError &e) {
        std::cerr << "Caught CURL Runtime Error: " << e.what() << std::endl;
    }
}

int main() {
    testException();
    return 0;
}
