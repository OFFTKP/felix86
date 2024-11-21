add_executable(literal literal.cpp)
target_link_libraries(literal biscuit)
set_property(TARGET literal PROPERTY CXX_STANDARD 20)
