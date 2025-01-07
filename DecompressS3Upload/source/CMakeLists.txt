# CMakeList.txt : CMake project for DecompressS3Upload, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("DecompressS3Upload")

# Add source to this project's executable.
add_executable (${PROJECT_NAME} "DecompressS3Upload.cpp" "DecompressS3Upload.h" "serviceS3.h" "serviceSqs.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
set(SERVICE_COMPONENTS s3-crt s3 sqs lambda)

find_package(aws-lambda-runtime)
find_package(AWSSDK REQUIRED COMPONENTS ${SERVICE_COMPONENTS})
find_package(libzip REQUIRED)

pkg_check_modules(ZIP REQUIRED libzip)

if(MSVC AND BUILD_SHARED_LIBS)
    set(CMAKE_BUILD_TYPE Release) # Explicitly setting CMAKE_BUILD_TYPE is necessary in Windows to copy DLLs.

    list(APPEND SERVICE_LIST ${SERVICE_COMPONENTS})

    AWSSDK_CPY_DYN_LIBS(SERVICE_LIST "" ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
endif()

include_directories(${ZIP_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE AWS::aws-lambda-runtime ${AWSSDK_LINK_LIBRARIES} ${ZIP_LIBRARIES})

target_compile_options(${PROJECT_NAME} PRIVATE
    "-Wall"
    "-Wextra"
    "-Wconversion"
    "-Wshadow"
    "-Wno-sign-conversion")

target_compile_features(${PROJECT_NAME} PRIVATE "cxx_std_11")

aws_lambda_package_target(${PROJECT_NAME})
