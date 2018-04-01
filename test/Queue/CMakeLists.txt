
# add_executable(CoarseQueue CoarseQueue.cpp)
# target_link_libraries(CoarseQueue TMRexpLib)
# add_test(CoarseQueue CoarseQueue)

add_executable(MichaelScottQueue MichaelScottQueue.cpp)
target_link_libraries(MichaelScottQueue TMRexpLib)
add_test(MichaelScottQueue MichaelScottQueue)

add_executable(DGLMQueue DGLMQueue.cpp)
target_link_libraries(DGLMQueue TMRexpLib)
add_test(DGLMQueue DGLMQueue)
