
add_executable(TreibersStack_EBR TreibersStack.cpp)
target_link_libraries(TreibersStack_EBR TMRexpLib)
# add_test(TreibersStack TreibersStack)

add_executable(MichaelScottQueue_EBR MichaelScottQueue.cpp)
target_link_libraries(MichaelScottQueue_EBR TMRexpLib)
# add_test(MichaelScottQueue MichaelScottQueue)

add_executable(DGLMQueue_EBR DGLMQueue.cpp)
target_link_libraries(DGLMQueue_EBR TMRexpLib)
# add_test(DGLMQueue DGLMQueue)
