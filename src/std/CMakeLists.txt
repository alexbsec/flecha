# Avoid the target name "std", which can be confused with the C++ namespace.
add_library(flecha_std STATIC std.cpp)
target_link_libraries(flecha_std PRIVATE flecha_project_options)
