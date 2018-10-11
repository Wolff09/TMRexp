
add_executable(CoarseStack_GC CoarseStack.cpp)
target_link_libraries(CoarseStack_GC TMRexpLib)
# add_test(CoarseStack   CoarseStack)

add_executable(TreibersStack_GC TreibersStack.cpp)
target_link_libraries(TreibersStack_GC TMRexpLib)
# add_test(TreibersStack TreibersStack)

add_executable(CoarseQueue_GC CoarseQueue.cpp)
target_link_libraries(CoarseQueue_GC TMRexpLib)
# add_test(CoarseQueue CoarseQueue)

add_executable(MichaelScottQueue_GC MichaelScottQueue.cpp)
target_link_libraries(MichaelScottQueue_GC TMRexpLib)
# add_test(MichaelScottQueue MichaelScottQueue)

add_executable(DGLMQueue_GC DGLMQueue.cpp)
target_link_libraries(DGLMQueue_GC TMRexpLib)
# add_test(DGLMQueue DGLMQueue)
