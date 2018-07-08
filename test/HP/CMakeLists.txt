
add_executable(TreibersStack_HP TreibersStack.cpp)
target_link_libraries(TreibersStack_HP TMRexpLib)
# add_test(TreibersStack TreibersStack)

add_executable(TreibersRace_HP TreibersRace.cpp)
target_link_libraries(TreibersRace_HP TMRexpLib)
# add_test(TreibersRace TreibersRace)

add_executable(MichaelScottQueue_HP MichaelScottQueue.cpp)
target_link_libraries(MichaelScottQueue_HP TMRexpLib)
# add_test(MichaelScottQueue MichaelScottQueue)

add_executable(DGLMQueue_HP DGLMQueue.cpp)
target_link_libraries(DGLMQueue_HP TMRexpLib)
# add_test(DGLMQueue DGLMQueue)
