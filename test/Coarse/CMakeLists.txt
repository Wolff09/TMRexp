
add_executable(CoarseQueue CoarseQueue.cpp)
target_link_libraries(CoarseQueue TMRexpLib)

add_executable(CoarseStack CoarseStack.cpp)
target_link_libraries(CoarseStack TMRexpLib)


add_test(CoarseStack_01--GC 	CoarseStack --GC --malloc --success)
add_test(CoarseStack_02--PRF	CoarseStack --PRF --malloc --success)
add_test(CoarseStack_03--MM 	CoarseStack --MM --init --success)

add_test(CoarseQueue_01--GC 	CoarseQueue --GC --malloc --success)
add_test(CoarseQueue_02--PRF	CoarseQueue --PRF --malloc --success)
add_test(CoarseQueue_03--MM 	CoarseQueue --MM --init --success)

# add_test(CoarseQueue_01--GC--malloc--success	CoarseQueue --GC --malloc --success)
# add_test(CoarseQueue_01--GC--init--success  	CoarseQueue --GC --init --success)

# add_test(CoarseQueue_02--PRF--malloc--success	CoarseQueue --PRF --malloc --success)
# add_test(CoarseQueue_02--PRF--init--success  	CoarseQueue --PRF --init --success)

# add_test(CoarseQueue_03--MM--malloc--success	CoarseQueue --MM --malloc --success)
# add_test(CoarseQueue_03--MM--init--success  	CoarseQueue --MM --init --success)
