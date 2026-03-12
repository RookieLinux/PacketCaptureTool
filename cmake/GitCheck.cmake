# 最低Git版本
set(GIT_MIN_VERSION "2.0")

# ------------------------------------------------------------
# 1. 检查 Git 是否存在
# ------------------------------------------------------------
find_package(Git QUIET)

if(NOT GIT_FOUND)
    message(FATAL_ERROR
            "\n[GitCheck] 未检测到 Git 工具\n"
            "请安装 Git:\n"
            "https://git-scm.com/downloads\n"
    )
endif()

message(STATUS "[GitCheck] Git executable: ${GIT_EXECUTABLE}")

# ------------------------------------------------------------
# 2. 获取 Git 版本
# ------------------------------------------------------------
execute_process(
        COMMAND ${GIT_EXECUTABLE} --version
        OUTPUT_VARIABLE PROJECT_GIT_VERSION_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" PROJECT_GIT_VERSION ${PROJECT_GIT_VERSION_STRING})

if(PROJECT_GIT_VERSION VERSION_LESS GIT_MIN_VERSION)
    message(FATAL_ERROR
            "[GitCheck] Git version too old (${PROJECT_GIT_VERSION})\n"
            "Required >= ${GIT_MIN_VERSION}"
    )
endif()

message(STATUS "[GitCheck] Git version: ${PROJECT_GIT_VERSION}")

# ------------------------------------------------------------
# 3. 检测是否在 Git 仓库
# ------------------------------------------------------------
execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --is-inside-work-tree
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_REPO_RESULT
        OUTPUT_VARIABLE GIT_REPO_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT GIT_REPO_RESULT EQUAL 0)
    message(FATAL_ERROR
            "[GitCheck] 当前目录不是 Git 仓库\n"
            "${CMAKE_SOURCE_DIR}"
    )
endif()

message(STATUS "[GitCheck] Git repository detected")

# ------------------------------------------------------------
# 4. 检查 Git 用户配置
# ------------------------------------------------------------
#execute_process(
#        COMMAND ${GIT_EXECUTABLE} config --global user.name
#        OUTPUT_VARIABLE GIT_USER_NAME
#        OUTPUT_STRIP_TRAILING_WHITESPACE
#)
#
#execute_process(
#        COMMAND ${GIT_EXECUTABLE} config --global user.email
#        OUTPUT_VARIABLE GIT_USER_EMAIL
#        OUTPUT_STRIP_TRAILING_WHITESPACE
#)
#
#if(GIT_USER_NAME STREQUAL "" OR GIT_USER_EMAIL STREQUAL "")
#    message(FATAL_ERROR
#            "\n[GitCheck] Git 用户未配置\n"
#            "请运行:\n"
#            "  git config --global user.name \"Your Name\"\n"
#            "  git config --global user.email \"you@example.com\"\n"
#    )
#endif()
#
#message(STATUS "[GitCheck] Git user: ${GIT_USER_NAME} <${GIT_USER_EMAIL}>")

# ------------------------------------------------------------
# 5. 检测 remote
# ------------------------------------------------------------
execute_process(
        COMMAND ${GIT_EXECUTABLE} remote
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REMOTE_LIST
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(GIT_REMOTE_LIST STREQUAL "")
    message(WARNING "[GitCheck] 仓库没有 remote (origin)")
else()
    message(STATUS "[GitCheck] Remote(s): ${GIT_REMOTE_LIST}")
endif()

# ------------------------------------------------------------
# 6. 获取当前分支
# ------------------------------------------------------------
execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ------------------------------------------------------------
# 7. 获取 commit hash
# ------------------------------------------------------------
execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ------------------------------------------------------------
# 8. 获取 commit 时间
# ------------------------------------------------------------
execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%cd --date=iso
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_TIME
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ------------------------------------------------------------
# 9. 输出信息
# ------------------------------------------------------------
message(STATUS "[GitCheck] Branch      : ${GIT_BRANCH}")
message(STATUS "[GitCheck] Commit Hash : ${GIT_COMMIT_HASH}")
message(STATUS "[GitCheck] Short Hash  : ${GIT_COMMIT_SHORT}")
message(STATUS "[GitCheck] Commit Time : ${GIT_COMMIT_TIME}")

# ------------------------------------------------------------
# 10. 导出变量
# ------------------------------------------------------------
set(PROJECT_GIT_BRANCH ${GIT_BRANCH} CACHE INTERNAL "")
set(PROJECT_GIT_COMMIT ${GIT_COMMIT_HASH} CACHE INTERNAL "")
set(PROJECT_GIT_COMMIT_SHORT ${GIT_COMMIT_SHORT} CACHE INTERNAL "")
set(PROJECT_GIT_COMMIT_TIME ${GIT_COMMIT_TIME} CACHE INTERNAL "")