# CMakeList.txt : CMake project for ProcessCsvToDatabase, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("ProcessCsvToDatabase")

# Add source to this project's executable.
add_executable (ProcessCsvToDatabase "ProcessCsvToDatabase.cpp" "ProcessCsvToDatabase.h" "serviceSqs.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ProcessCsvToDatabase PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
set(SERVICE_COMPONENTS sqs lambda)

find_package(aws-lambda-runtime)
find_package(AWSSDK REQUIRED COMPONENTS ${SERVICE_COMPONENTS})
find_package(ASPELL)

target_link_libraries(${PROJECT_NAME} PRIVATE AWS::aws-lambda-runtime aspell mysqlcppconn ${AWSSDK_LINK_LIBRARIES})

target_compile_options(${PROJECT_NAME} PRIVATE
    "-Wall"
    "-Wextra"
    "-Wconversion"
    "-Wshadow"
    "-Wno-sign-conversion")

target_compile_features(${PROJECT_NAME} PRIVATE "cxx_std_11")

aws_lambda_package_target(${PROJECT_NAME})