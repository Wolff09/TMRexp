
add_executable(HPimpl Stack.cpp)
target_link_libraries(HPimpl TMRexpLib)
add_test(NAME "HPimpl" COMMAND "${BIN_PATH}/HPimpl")

add_executable(HPimpl_Queue Queue.cpp)
target_link_libraries(HPimpl_Queue TMRexpLib)
add_test(NAME "HPimpl_Queue" COMMAND "${BIN_PATH}/HPimpl_Queue")

add_executable(HPimpl_LazyUnprotect NoopUnprotect.cpp)
target_link_libraries(HPimpl_LazyUnprotect TMRexpLib)
add_test(NAME "HPimpl_LazyUnprotect" COMMAND "${BIN_PATH}/HPimpl_LazyUnprotect")

add_executable(HPimpl_TwoStepProtect TwoStepProtect.cpp)
target_link_libraries(HPimpl_TwoStepProtect TMRexpLib)
add_test(NAME "HPimpl_TwoStepProtect" COMMAND "${BIN_PATH}/HPimpl_TwoStepProtect")

add_executable(HPimpl_NoProtection NoProtection.cpp)
target_link_libraries(HPimpl_NoProtection TMRexpLib)
add_test(NAME "HPimpl_NoProtection" COMMAND "${BIN_PATH}/HPimpl_NoProtection")

add_executable(HPimpl_KeepsFreed KeepsFreed.cpp)
target_link_libraries(HPimpl_KeepsFreed TMRexpLib)
add_test(NAME "HPimpl_KeepsFreed" COMMAND "${BIN_PATH}/HPimpl_KeepsFreed")

add_executable(HPimpl_IgnoresProtections IgnoresProtection.cpp)
target_link_libraries(HPimpl_IgnoresProtections TMRexpLib)
add_test(NAME "HPimpl_IgnoresProtections" COMMAND "${BIN_PATH}/HPimpl_IgnoresProtections")

add_executable(HPimpl_BreaksLink BreakingLink.cpp)
target_link_libraries(HPimpl_BreaksLink TMRexpLib)
add_test(NAME "HPimpl_BreaksLink" COMMAND "${BIN_PATH}/HPimpl_BreaksLink")

add_executable(HPimpl_Array Array.cpp)
target_link_libraries(HPimpl_Array TMRexpLib)
add_test(NAME "HPimpl_Array" COMMAND "${BIN_PATH}/HPimpl_Array")
