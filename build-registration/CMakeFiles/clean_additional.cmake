# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "AuthServiceTest_autogen"
  "CMakeFiles\\AuthServiceTest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\AuthServiceTest_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\ContactServiceTest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\ContactServiceTest_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\ContactTest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\ContactTest_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\GroupTest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\GroupTest_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\HeadersCompileTest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\HeadersCompileTest_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\PhoneBookCore_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\PhoneBookCore_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\PhoneBookManagementQt_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\PhoneBookManagementQt_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\PhoneBookRuntimeTest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\PhoneBookRuntimeTest_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\UserTest_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\UserTest_autogen.dir\\ParseCache.txt"
  "ContactServiceTest_autogen"
  "ContactTest_autogen"
  "GroupTest_autogen"
  "HeadersCompileTest_autogen"
  "PhoneBookCore_autogen"
  "PhoneBookManagementQt_autogen"
  "PhoneBookRuntimeTest_autogen"
  "UserTest_autogen"
  )
endif()
