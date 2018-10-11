
add_executable(TreibersStack_HP TreibersStack.cpp)
target_link_libraries(TreibersStack_HP TMRexpLib)
# add_test(TreibersStack TreibersStack)

add_executable(OptTreibersStack_HP OptTreibersStack.cpp)
target_link_libraries(OptTreibersStack_HP TMRexpLib)
# add_test(OptTreibersStack OptTreibersStack)

add_executable(MichaelScottQueue_HP MichaelScottQueue.cpp)
target_link_libraries(MichaelScottQueue_HP TMRexpLib)
# add_test(MichaelScottQueue MichaelScottQueue)

add_executable(DGLMQueue_HP DGLMQueue.cpp)
target_link_libraries(DGLMQueue_HP TMRexpLib)
# add_test(DGLMQueue DGLMQueue)
