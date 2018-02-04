cmake_minimum_required(VERSION 3.5)
project( Lexer VERSION 0.1.1 )

add_library(Lexer STATIC src/Lexer.c)

target_include_directories( Lexer PUBLIC ${PROJECT_SOURCE_DIR}/include )
target_sources( Lexer PRIVATE ${PROJECT_SOURCE_DIR}/src/Lexer )
target_sources( Lexer PUBLIC ${PROJECT_SOURCE_DIR}/include/Lexer )

set_target_properties(Lexer
	PROPERTIES
	ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/lib"
	RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

if(NOT TARGET Dfa)
	add_subdirectory(${CMAKE_SOURCE_DIR}/ext/Dfa ${CMAKE_SOURCE_DIR}/ext/Dfa/build/bin)
endif(NOT TARGET Dfa)
target_link_libraries(Lexer Dfa)

if(NOT TARGET LinkedList)
	add_subdirectory(${CMAKE_SOURCE_DIR}/ext/LinkedList ${CMAKE_SOURCE_DIR}/ext/LinkedList/build/bin)
endif(NOT TARGET LinkedList)
target_link_libraries(Lexer LinkedList)

if(NOT TARGET Token)
	add_subdirectory(${CMAKE_SOURCE_DIR}/ext/Token ${CMAKE_SOURCE_DIR}/ext/Token/build/bin)
endif(NOT TARGET Token)
target_link_libraries(Lexer Token)
