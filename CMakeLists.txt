CMAKE_MINIMUM_REQUIRED(VERSION 3.21)
project(forum-gate)

set(CMAKE_CXX_STANDARD 17)

# 别人的
find_library(BoostUrl boost_url)
add_library(MimeTypes libs/MimeTypes/MimeTypes.cpp)

# 我的

aux_source_directory(src SRC_DIRS)
add_executable(ForumGate ${SRC_DIRS})
target_link_libraries(ForumGate MimeTypes ${BoostUrl})
