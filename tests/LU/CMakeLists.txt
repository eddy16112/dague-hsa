if( PLASMA_FOUND )
  include_directories( ${PLASMA_INCLUDE_DIR} ${PLASMA_INCLUDE_DIR}/../src )
  
  add_executable(dgesv dgesv.c LU.c)
  set_target_properties(dgesv PROPERTIES LINKER_LANGUAGE Fortran)
  set_target_properties(dgesv PROPERTIES COMPILE_FLAGS "-I${PLASMA_SRC_DIR} -DADD_")
  set_target_properties(dgesv PROPERTIES LINK_FLAGS "${LOCAL_FORTRAN_LINK_FLAGS}")
  target_link_libraries(dgesv dague ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${EXTRA_LIBS})

  add_executable(dgesv_sd dgesv_sd.c LU_sd.c)
  set_target_properties(dgesv_sd PROPERTIES LINKER_LANGUAGE Fortran)
  set_target_properties(dgesv_sd PROPERTIES COMPILE_FLAGS "-DADD_")
  set_target_properties(dgesv_sd PROPERTIES LINK_FLAGS "${LOCAL_FORTRAN_LINK_FLAGS}")
  target_link_libraries(dgesv_sd dague ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${EXTRA_LIBS})

  if( DAGUE_MPI AND MPI_FOUND )
    add_executable(mpi_dgesv dgesv.c LU.c)
    set_target_properties(mpi_dgesv PROPERTIES LINKER_LANGUAGE Fortran)
    set_target_properties(mpi_dgesv PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS} -I${PLASMA_SRC_DIR}  -DADD_ -DUSE_MPI")
    set_target_properties(mpi_dgesv PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} ${LOCAL_FORTRAN_LINK_FLAGS}")
    target_link_libraries(mpi_dgesv dague-mpi ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS})

    add_executable(mpi_dgesv_sd dgesv_sd.c LU_sd.c)
    set_target_properties(mpi_dgesv_sd PROPERTIES LINKER_LANGUAGE Fortran)
    set_target_properties(mpi_dgesv_sd PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS} -DADD_ -DUSE_MPI")
    set_target_properties(mpi_dgesv_sd PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} ${LOCAL_FORTRAN_LINK_FLAGS}")
    target_link_libraries(mpi_dgesv_sd dague-mpi ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS})

#    add_executable(mpi_dgesv_kmn dgesv.c LU_kmn.c)
#    set_target_properties(mpi_dgesv_kmn PROPERTIES LINKER_LANGUAGE Fortran)
#    set_target_properties(mpi_dgesv_kmn PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS} -DADD_ -DUSE_MPI")
#    set_target_properties(mpi_dgesv_kmn PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} ${LOCAL_FORTRAN_LINK_FLAGS}")
#    target_link_libraries(mpi_dgesv_kmn dague-mpi ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS})
  endif( DAGUE_MPI AND MPI_FOUND )

endif( PLASMA_FOUND )

# add the command to generate the source code
add_custom_command (
  OUTPUT LU_kmn.c LU_kmn.h
  COMMAND ../../tools/dague-compiler/dc -i LU_kmn.jdf -o LU_kmn
  MAIN_DEPENDENCY LU_kmn.jdf
  DEPENDS dc
  )

add_custom_command (
  OUTPUT LU.c LU.h
  COMMAND ../../tools/dague-compiler/dc -i LU.jdf -o LU
  MAIN_DEPENDENCY LU.jdf
  DEPENDS dc
  )

add_custom_command (
  OUTPUT LU_sd.c LU_sd.h
  COMMAND ../../tools/dague-compiler/dc -i LU_sd.jdf -o LU_sd --noline
  MAIN_DEPENDENCY LU_sd.jdf
  DEPENDS dc
  )
