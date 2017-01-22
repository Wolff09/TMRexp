
add_executable(CoarseStack CoarseStack.cpp)
target_link_libraries(CoarseStack TMRexpLib)
add_test(CoarseStack   CoarseStack)

add_executable(TreibersStack TreibersStack.cpp)
target_link_libraries(TreibersStack TMRexpLib)
add_test(TreibersStack TreibersStack)
