
add_executable(CoarseStack_NoSMR CoarseStack.cpp)
target_link_libraries(CoarseStack_NoSMR TMRexpLib)
# add_test(CoarseStack   CoarseStack)

add_executable(CoarseQueue_NoSMR CoarseQueue.cpp)
target_link_libraries(CoarseQueue_NoSMR TMRexpLib)
# add_test(CoarseQueue CoarseQueue)
