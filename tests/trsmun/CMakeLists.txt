if( PLASMA_FOUND )

  include_directories( ${PLASMA_INCLUDE_DIR} ${PLASMA_INCLUDE_DIR}/../src)
  
  if( DPLASMA_MPI AND MPI_FOUND )
    add_executable(pdtrsmun dtrsmun.c data_management.c trsmun_jdf.c)
    set_target_properties(pdtrsmun PROPERTIES LINKER_LANGUAGE Fortran)
    set_target_properties(pdtrsmun PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS} -DADD_ -DUSE_MPI")
    set_target_properties(pdtrsmun PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS} ${LOCAL_FORTRAN_LINK_FLAGS}")
    target_link_libraries(pdtrsmun dplasma-mpi ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS})
  endif( DPLASMA_MPI AND MPI_FOUND )

  add_executable(dtrsmun dtrsmun.c data_management.c trsmun_jdf.c)
  set_target_properties(dtrsmun PROPERTIES LINKER_LANGUAGE Fortran)
  set_target_properties(dtrsmun PROPERTIES COMPILE_FLAGS "-DADD_")
  set_target_properties(dtrsmun PROPERTIES LINK_FLAGS "${LOCAL_FORTRAN_LINK_FLAGS}")
  target_link_libraries(dtrsmun dplasma ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${EXTRA_LIBS})

endif( PLASMA_FOUND )

# add the command to generate the source code
add_custom_command (
  OUTPUT trsmun_jdf.c
  COMMAND ../../tools/dpc trsmun.jdf trsmun_jdf.c
  MAIN_DEPENDENCY trsmun.jdf
  DEPENDS dpc
  )
 
