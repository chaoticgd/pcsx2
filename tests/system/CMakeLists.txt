add_executable(system_test harness/HarnessHost.cpp)
target_include_directories(system_test PRIVATE "include")
target_link_libraries(system_test PRIVATE
	PCSX2_FLAGS
	PCSX2
)

#add_custom_command(
#	OUTPUT ${CMAKE_BUILD_DIR}/tests
#)
