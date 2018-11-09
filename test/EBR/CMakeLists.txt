
add_executable(EBRimpl Stack.cpp)
target_link_libraries(EBRimpl TMRexpLib)
add_test(NAME "EBRimpl" COMMAND "${BIN_PATH}/EBRimpl")

add_executable(EBRimpl_2Set LessBacklog.cpp)
target_link_libraries(EBRimpl_2Set TMRexpLib)
add_test(NAME "EBRimpl_2Set" COMMAND "${BIN_PATH}/EBRimpl_2Set")

add_executable(EBRimpl_1Set NoBacklog.cpp)
target_link_libraries(EBRimpl_1Set TMRexpLib)
add_test(NAME "EBRimpl_1Set" COMMAND "${BIN_PATH}/EBRimpl_1Set")
