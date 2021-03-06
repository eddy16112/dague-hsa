if( PLASMA_FOUND )

  include_directories( ${PLASMA_INCLUDE_DIR} ${PLASMA_SRC_DIR} )
  
  if( DAGUE_MPI AND MPI_FOUND )

    add_executable(mpi_llt llt.c  cholesky.c)
    set_target_properties(mpi_llt PROPERTIES LINKER_LANGUAGE Fortran)
    set_target_properties(mpi_llt PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS} -DADD_ -DUSE_MPI")
    set_target_properties(mpi_llt PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} ${LOCAL_FORTRAN_LINK_FLAGS}")
    target_link_libraries(mpi_llt dague-mpi ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS})

    # add_executable(mpi_testio testio.c)
    # set_target_properties(mpi_testio PROPERTIES LINKER_LANGUAGE Fortran)
    # set_target_properties(mpi_testio PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS} -DADD_ -DUSE_MPI")
    # set_target_properties(mpi_testio PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} ${LOCAL_FORTRAN_LINK_FLAGS}")
    # target_link_libraries(mpi_testio dague-mpi ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS})

  endif( DAGUE_MPI AND MPI_FOUND )

    add_executable(llt llt.c  cholesky.c)
    set_target_properties(llt PROPERTIES LINKER_LANGUAGE Fortran)
    set_target_properties(llt PROPERTIES COMPILE_FLAGS "-DADD_")
   set_target_properties(llt PROPERTIES LINK_FLAGS "${LOCAL_FORTRAN_LINK_FLAGS}")
    target_link_libraries(llt dague ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${EXTRA_LIBS})
 
endif( PLASMA_FOUND )

# add the command to generate the source code

add_custom_command (
  OUTPUT cholesky.c cholesky.h
  COMMAND ../../tools/dague-compiler/dc -i cholesky.jdf -o cholesky
  MAIN_DEPENDENCY cholesky.jdf
  DEPENDS dc
  )
