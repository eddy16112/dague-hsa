BISON_TARGET(dague_yacc dague.y ${CMAKE_CURRENT_BINARY_DIR}/dague.tab.c)
FLEX_TARGET(dague_flex dague.l  ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
ADD_FLEX_BISON_DEPENDENCY(dague_flex dague_yacc)

add_executable(dc main.c jdf.c jdf2c.c ${BISON_dague_yacc_OUTPUTS} ${FLEX_dague_flex_OUTPUTS})
set_target_properties(dc PROPERTIES LINKER_LANGUAGE C)
set_target_properties(dc PROPERTIES LINK_FLAGS "${LOCAL_C_LINK_FLAGS}")
target_link_libraries(dc ${EXTRA_LIBS} dague -lm)

#add_subdirectory(example)

