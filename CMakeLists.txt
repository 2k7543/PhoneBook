cmake_minimum_required(VERSION 3.16)

project(PhoneBookManagementQt VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

set(PHONEBOOK_CORE_SOURCES
    src/database/Database.cpp
    src/database/DatabaseInitializer.cpp
    src/models/Contact.cpp
    src/models/ContactEmail.cpp
    src/models/ContactPhoneNumber.cpp
    src/repositories/AdminLogRepository.cpp
    src/repositories/ContactRepository.cpp
    src/repositories/GroupRepository.cpp
    src/repositories/UserRepository.cpp
    src/services/AdminService.cpp
    src/services/AuthService.cpp
    src/services/ContactService.cpp
    src/services/CsvService.cpp
    src/services/GroupService.cpp
    src/services/PasswordResetService.cpp
    src/services/PhoneBookRuntime.cpp
    src/utils/DateTime.cpp
    src/utils/PasswordHasher.cpp
    src/utils/Validator.cpp
)

# Refactor note: compile every domain, repository, service, and utility source
# into one library consumed by the Qt app and runtime integration tests.
add_library(PhoneBookCore STATIC ${PHONEBOOK_CORE_SOURCES})
target_include_directories(PhoneBookCore PUBLIC Include)
target_link_libraries(PhoneBookCore PUBLIC SQLite::SQLite3 OpenSSL::Crypto)

add_executable(PhoneBookManagementQt
    app/qt/main.cpp
    app/qt/resources/resources.qrc
)

target_include_directories(PhoneBookManagementQt PRIVATE Include)
target_link_libraries(PhoneBookManagementQt PRIVATE
    PhoneBookCore Qt6::Widgets Qt6::Svg
)

find_path(HTTPLIB_INCLUDE_DIR httplib.h)
find_library(HTTPLIB_LIBRARY NAMES cpp-httplib)
find_package(nlohmann_json CONFIG REQUIRED)
if(NOT HTTPLIB_INCLUDE_DIR OR NOT HTTPLIB_LIBRARY)
    message(FATAL_ERROR "cpp-httplib is required to build PhoneBookWebServer")
endif()

add_executable(PhoneBookWebServer app/server/main.cpp)
target_sources(PhoneBookWebServer PRIVATE app/server/EmailSender.cpp)
target_include_directories(PhoneBookWebServer PRIVATE ${HTTPLIB_INCLUDE_DIR})
target_compile_definitions(PhoneBookWebServer PRIVATE
    PHONEBOOK_WEB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/app/web"
)
target_link_libraries(PhoneBookWebServer PRIVATE
    PhoneBookCore ${HTTPLIB_LIBRARY} nlohmann_json::nlohmann_json CURL::libcurl
)

find_package(Qt6 QUIET COMPONENTS Test)
if(Qt6Test_FOUND)
    enable_testing()

    function(add_phonebook_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Test SQLite::SQLite3)
        target_include_directories(${name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Include
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
        )
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    add_phonebook_test(UserTest tests/UserTest.cpp)
    add_phonebook_test(AuthServiceTest tests/AuthServiceTest.cpp)
    target_link_libraries(AuthServiceTest PRIVATE PhoneBookCore)
    add_phonebook_test(ContactTest tests/ContactTest.cpp)
    add_phonebook_test(GroupTest tests/GroupTest.cpp)
    add_phonebook_test(ContactServiceTest tests/ContactServiceTest.cpp)
    add_subdirectory(unit_tests)

    add_executable(PhoneBookRuntimeTest
        tests/PhoneBookRuntimeTest.cpp
    )
    target_include_directories(PhoneBookRuntimeTest PRIVATE Include)
    target_link_libraries(PhoneBookRuntimeTest PRIVATE
        PhoneBookCore Qt6::Core Qt6::Test
    )
    add_test(NAME PhoneBookRuntimeTest COMMAND PhoneBookRuntimeTest)

    add_executable(HeadersCompileTest tests/HeadersCompileTest.cpp)
    target_include_directories(HeadersCompileTest PRIVATE Include .)
    target_link_libraries(HeadersCompileTest PRIVATE PhoneBookCore)
    add_test(NAME HeadersCompileTest COMMAND HeadersCompileTest)
endif()