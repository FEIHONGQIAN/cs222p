# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.15

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /Applications/CLion.app/Contents/bin/cmake/mac/bin/cmake

# The command to remove a file.
RM = /Applications/CLion.app/Contents/bin/cmake/mac/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/rmtest_14.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/rmtest_14.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/rmtest_14.dir/flags.make

CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.o: CMakeFiles/rmtest_14.dir/flags.make
CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.o: ../rm/rmtest_14.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.o"
	/Library/Developer/CommandLineTools/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.o -c /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/rm/rmtest_14.cc

CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.i"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/rm/rmtest_14.cc > CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.i

CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.s"
	/Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/rm/rmtest_14.cc -o CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.s

# Object files for target rmtest_14
rmtest_14_OBJECTS = \
"CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.o"

# External object files for target rmtest_14
rmtest_14_EXTERNAL_OBJECTS =

rmtest_14: CMakeFiles/rmtest_14.dir/rm/rmtest_14.cc.o
rmtest_14: CMakeFiles/rmtest_14.dir/build.make
rmtest_14: libRM.a
rmtest_14: libRBFM.a
rmtest_14: libPFM.a
rmtest_14: CMakeFiles/rmtest_14.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable rmtest_14"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/rmtest_14.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/rmtest_14.dir/build: rmtest_14

.PHONY : CMakeFiles/rmtest_14.dir/build

CMakeFiles/rmtest_14.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/rmtest_14.dir/cmake_clean.cmake
.PHONY : CMakeFiles/rmtest_14.dir/clean

CMakeFiles/rmtest_14.dir/depend:
	cd /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6 /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6 /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/cmake-build-debug /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/cmake-build-debug /Users/qianfeihong/CLionProjects/cs222p-winter20-team-6/cmake-build-debug/CMakeFiles/rmtest_14.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/rmtest_14.dir/depend

