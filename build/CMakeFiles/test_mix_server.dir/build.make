# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/build

# Include any dependencies generated for this target.
include CMakeFiles/test_mix_server.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/test_mix_server.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/test_mix_server.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/test_mix_server.dir/flags.make

CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o: CMakeFiles/test_mix_server.dir/flags.make
CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o: ../src/test_mix_server.cpp
CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o: CMakeFiles/test_mix_server.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o -MF CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o.d -o CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o -c /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/src/test_mix_server.cpp

CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/src/test_mix_server.cpp > CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.i

CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/src/test_mix_server.cpp -o CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.s

CMakeFiles/test_mix_server.dir/src/common.cpp.o: CMakeFiles/test_mix_server.dir/flags.make
CMakeFiles/test_mix_server.dir/src/common.cpp.o: ../src/common.cpp
CMakeFiles/test_mix_server.dir/src/common.cpp.o: CMakeFiles/test_mix_server.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/test_mix_server.dir/src/common.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/test_mix_server.dir/src/common.cpp.o -MF CMakeFiles/test_mix_server.dir/src/common.cpp.o.d -o CMakeFiles/test_mix_server.dir/src/common.cpp.o -c /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/src/common.cpp

CMakeFiles/test_mix_server.dir/src/common.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/test_mix_server.dir/src/common.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/src/common.cpp > CMakeFiles/test_mix_server.dir/src/common.cpp.i

CMakeFiles/test_mix_server.dir/src/common.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/test_mix_server.dir/src/common.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/src/common.cpp -o CMakeFiles/test_mix_server.dir/src/common.cpp.s

# Object files for target test_mix_server
test_mix_server_OBJECTS = \
"CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o" \
"CMakeFiles/test_mix_server.dir/src/common.cpp.o"

# External object files for target test_mix_server
test_mix_server_EXTERNAL_OBJECTS =

test_mix_server: CMakeFiles/test_mix_server.dir/src/test_mix_server.cpp.o
test_mix_server: CMakeFiles/test_mix_server.dir/src/common.cpp.o
test_mix_server: CMakeFiles/test_mix_server.dir/build.make
test_mix_server: /home/hikerlee02/workspace/x86target/lib/libvsomeip3.so.3.4.10
test_mix_server: CMakeFiles/test_mix_server.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable test_mix_server"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/test_mix_server.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/test_mix_server.dir/build: test_mix_server
.PHONY : CMakeFiles/test_mix_server.dir/build

CMakeFiles/test_mix_server.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/test_mix_server.dir/cmake_clean.cmake
.PHONY : CMakeFiles/test_mix_server.dir/clean

CMakeFiles/test_mix_server.dir/depend:
	cd /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/build /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/build /home/hikerlee02/workspace/vsomeip-test/gitworks/vsomeip_performance_test/build/CMakeFiles/test_mix_server.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/test_mix_server.dir/depend

