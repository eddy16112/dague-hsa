if( PLASMA_FOUND )
  include_directories( ${PLASMA_INCLUDE_DIR} ${PLASMA_INCLUDE_DIR}/../src )
  
 add_executable(ladgels dgels.c QR.c)
  set_target_properties(ladgels PROPERTIES LINKER_LANGUAGE Fortran)
  set_target_properties(ladgels PROPERTIES COMPILE_FLAGS "-DADD_")
  set_target_properties(ladgels PROPERTIES LINK_FLAGS "${LOCAL_FORTRAN_LINK_FLAGS}")
  target_link_libraries(ladgels dague ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${EXTRA_LIBS})

 if( DAGUE_MPI AND MPI_FOUND )
    add_executable(mpi_ladgels dgels.c QR.c)
    set_target_properties(mpi_ladgels PROPERTIES LINKER_LANGUAGE Fortran)
    set_target_properties(mpi_ladgels PROPERTIES COMPILE_FLAGS "-DADD_ -DUSE_MPI")
    set_target_properties(mpi_ladgels PROPERTIES LINK_FLAGS "${LOCAL_FORTRAN_LINK_FLAGS}")
    target_link_libraries(mpi_ladgels dague-mpi ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS})
  endif( DAGUE_MPI AND MPI_FOUND )

endif( PLASMA_FOUND )

# add the command to generate the source code
add_custom_command (
  OUTPUT QR.c QR.h
  COMMAND ../../tools/dague-compiler/dc -i QR.jdf -o QR
  MAIN_DEPENDENCY QR.jdf
  DEPENDS dc
  )
