include(precision_generation)
include(JDFsupport)

macro(testings_addexec OUTPUTLIST PRECISIONS ZSOURCES)

  # Set flags for compilation
  if( DAGUE_MPI AND MPI_FOUND )
    set(testings_addexec_CFLAGS  "-I${CMAKE_CURRENT_SOURCE_DIR}/common ${MPI_COMPILE_FLAGS} -DADD_ -DUSE_MPI")
    set(testings_addexec_LDFLAGS "${MPI_LINK_FLAGS} ${LOCAL_FORTRAN_LINK_FLAGS}")
    set(testings_addexec_LIBS   
      dplasma-mpi  dplasmatest-mpi dague-mpi  dague_distribution_matrix-mpi 
      ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS}
      )
  else ( DAGUE_MPI AND MPI_FOUND )
    set(testings_addexe_CFLAGS  "-I${CMAKE_CURRENT_SOURCE_DIR}/common -DADD_")
    set(testings_addexe_LDFLAGS "${LOCAL_FORTRAN_LINK_FLAGS}")
    set(testings_addexe_LIBS   
      dplasma  dplasmatest dague  dague_distribution_matrix 
      ${PLASMA_LIBRARIES} ${BLAS_LIBRARIES} ${MPI_LIBRARIES} ${EXTRA_LIBS}
      )
  endif()

  precisions_rules(testings_addexec_GENFILES "${PRECISIONS}" "${ZSOURCES}")
  
  foreach(testings_addexec_GENFILE ${testings_addexec_GENFILES})
    string(REGEX REPLACE "\\.[scdz]" "" testings_addexec_EXEC ${testings_addexec_GENFILE})

    add_executable(${testings_addexec_EXEC} ${testings_addexec_GENFILE})
    set_target_properties(${testings_addexec_EXEC} PROPERTIES LINKER_LANGUAGE Fortran)
    set_target_properties(${testings_addexec_EXEC} PROPERTIES COMPILE_FLAGS "${testings_addexec_CFLAGS}")
    set_target_properties(${testings_addexec_EXEC} PROPERTIES LINK_FLAGS "${testings_addexec_LDFLAGS}")
    target_link_libraries(${testings_addexec_EXEC} ${testings_addexec_LIBS})
    list(APPEND ${OUTPUTLIST} ${testings_addexec_EXEC})
  endforeach()

endmacro(testings_addexec)

