# - Try to find Assimp
# Once done, this will define
#
# ASSIMP_FOUND - system has Assimp
# ASSIMP_INCLUDE_DIR - the Assimp include directories
# ASSIMP_LIBRARIES - link these to use Assimp
#
# On UNIX, prefer system headers (/usr/include) over the bundled LearnOpenGL
# includes/assimp — Assimp 5 changed aiString::length from size_t to uint32_t.
FIND_PATH(ASSIMP_INCLUDE_DIR assimp/mesh.h
	/usr/include
	/usr/local/include
	/opt/local/include
	${CMAKE_SOURCE_DIR}/includes
)
FIND_LIBRARY(ASSIMP_LIBRARY assimp
	/usr/lib/aarch64-linux-gnu
	/usr/lib/x86_64-linux-gnu
	/usr/lib64
	/usr/lib
	/usr/local/lib
	/opt/local/lib
	${CMAKE_SOURCE_DIR}/lib
)
IF(ASSIMP_INCLUDE_DIR AND ASSIMP_LIBRARY)
	SET(ASSIMP_FOUND TRUE)
	SET(ASSIMP_LIBRARIES ${ASSIMP_LIBRARY})
ENDIF(ASSIMP_INCLUDE_DIR AND ASSIMP_LIBRARY)
IF(ASSIMP_FOUND)
	IF(NOT ASSIMP_FIND_QUIETLY)
		MESSAGE(STATUS "Found ASSIMP: ${ASSIMP_LIBRARY}")
		MESSAGE(STATUS "ASSIMP_INCLUDE_DIR: ${ASSIMP_INCLUDE_DIR}")
	ENDIF(NOT ASSIMP_FIND_QUIETLY)
ELSE(ASSIMP_FOUND)
	IF(ASSIMP_FIND_REQUIRED)
		MESSAGE(FATAL_ERROR "Could not find libASSIMP")
	ENDIF(ASSIMP_FIND_REQUIRED)
ENDIF(ASSIMP_FOUND)
