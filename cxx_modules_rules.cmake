
# Set up GCC specific stuff
# ?? stil required in 3.28+ ??
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	string(CONCAT CMAKE_EXPERIMENTAL_CXX_SCANDEP_SOURCE
      "<CMAKE_CXX_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -E -x c++ <SOURCE>"
      " -MT <DYNDEP_FILE> -MD -MF <DEP_FILE>"
      " -fmodules-ts -fdeps-file=<DYNDEP_FILE> -fdeps-target=<OBJECT> -fdeps-format=p1689r5"
      " -o <PREPROCESSED_SOURCE>")
	set(CMAKE_EXPERIMENTAL_CXX_MODULE_MAP_FORMAT "gcc")
	set(CMAKE_EXPERIMENTAL_CXX_MODULE_MAP_FLAG "-fmodules-ts -fmodule-mapper=<MODULE_MAP_FILE> -fdeps-format=p1689r5 -x c++")
endif ()
