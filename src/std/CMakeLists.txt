# Avoid the target name "std", which can be confused with the C++ namespace.
add_library(flecha_std INTERFACE)
target_link_libraries(flecha_std INTERFACE flecha_project_options)
