
add_executable(CoarseStack_NoReclamation CoarseStack.cpp)
target_link_libraries(CoarseStack_NoReclamation TMRexpLib)
# add_test(CoarseStack   CoarseStack)

add_executable(TreibersStack_NoReclamation TreibersStack.cpp)
target_link_libraries(TreibersStack_NoReclamation TMRexpLib)
# add_test(TreibersStack TreibersStack)

add_executable(CoarseQueue_NoReclamation CoarseQueue.cpp)
target_link_libraries(CoarseQueue_NoReclamation TMRexpLib)
# add_test(CoarseQueue CoarseQueue)

add_executable(MichaelScottQueue_NoReclamation MichaelScottQueue.cpp)
target_link_libraries(MichaelScottQueue_NoReclamation TMRexpLib)
# add_test(MichaelScottQueue MichaelScottQueue)

add_executable(DGLMQueue_NoReclamation DGLMQueue.cpp)
target_link_libraries(DGLMQueue_NoReclamation TMRexpLib)
# add_test(DGLMQueue DGLMQueue)
