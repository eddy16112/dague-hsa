#
# Shared Memory Testings
#

#
# Check BLAS/Lapack subroutines in shared memory
#
foreach(prec ${DPLASMA_PRECISIONS})

  # check the control and test matrices generation (zplrnt, zplghe, zplgsy, zpltmg) in shared memory
  add_test(shm_${prec}print ${SHM_TEST_CMD_LIST} ./testing_${prec}print -N 64 -t 7 -x -v=5)

  # check the norms that are used in all other testings
  add_test(shm_${prec}lange ${SHM_TEST_CMD_LIST} ./testing_${prec}lange -M 287 -N 283 -K 97 -t 56 -x -v=5)
  set_tests_properties("shm_${prec}lange" PROPERTIES DEPENDS "shm_${prec}print")

  # Need to add testings on zlacpy, zlaset, zgeadd, zlascal, zger, (zlaswp?)

  # BLAS Shared memory
  add_test(shm_${prec}trmm  ${SHM_TEST_CMD_LIST} ./testing_${prec}trmm  -M 106 -N 150 -K 97 -t 56 -x -v=5)
  set_tests_properties("shm_${prec}trmm" PROPERTIES DEPENDS "shm_${prec}lange")

  add_test(shm_${prec}trsm  ${SHM_TEST_CMD_LIST} ./testing_${prec}trsm  -M 106 -N 150 -K 97 -t 56 -x -v=5)
  set_tests_properties("shm_${prec}trsm" PROPERTIES DEPENDS "shm_${prec}trmm")

  add_test(shm_${prec}gemm  ${SHM_TEST_CMD_LIST} ./testing_${prec}gemm  -M 106 -N 283 -K 97 -t 56 -x -v=5)
  set_tests_properties("shm_${prec}trmm" PROPERTIES DEPENDS "shm_${prec}lange")

  add_test(shm_${prec}symm  ${SHM_TEST_CMD_LIST} ./testing_${prec}symm  -M 106 -N 283 -K 97 -t 56 -x -v=5)
  set_tests_properties("shm_${prec}trmm" PROPERTIES DEPENDS "shm_${prec}lange")

  add_test(shm_${prec}syrk  ${SHM_TEST_CMD_LIST} ./testing_${prec}syrk  -M 287 -N 283 -K 97 -t 56 -x -v=5)
  add_test(shm_${prec}syr2k ${SHM_TEST_CMD_LIST} ./testing_${prec}syr2k -M 287 -N 283 -K 97 -t 56 -x -v=5)

  if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
    add_test(shm_${prec}hemm  ${SHM_TEST_CMD_LIST} ./testing_${prec}hemm  -M 106 -N 283 -K 97 -t 56 -x -v=5)
    add_test(shm_${prec}herk  ${SHM_TEST_CMD_LIST} ./testing_${prec}herk  -M 287 -N 283 -K 97 -t 56 -x -v=5)
    add_test(shm_${prec}her2k ${SHM_TEST_CMD_LIST} ./testing_${prec}her2k -M 287 -N 283 -K 97 -t 56 -x -v=5)
  endif()

  # Cholesky
  add_test(shm_${prec}potrf ${SHM_TEST_CMD_LIST} ./testing_${prec}potrf -N 378 -t 93        -x -v=5)
  add_test(shm_${prec}posv  ${SHM_TEST_CMD_LIST} ./testing_${prec}posv  -N 457 -t 93 -K 367 -x -v=5)

  # QR / LQ
  add_test(shm_${prec}geqrf ${SHM_TEST_CMD_LIST} ./testing_${prec}geqrf -M 487 -N 283 -K 97 -t 56 -x -v=5)
  add_test(shm_${prec}gelqf ${SHM_TEST_CMD_LIST} ./testing_${prec}gelqf -M 287 -N 383 -K 97 -t 56 -x -v=5)
  if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
    add_test(shm_${prec}unmqr ${SHM_TEST_CMD_LIST} ./testing_${prec}unmqr -M 487 -N 283 -K 97 -t 56 -x -v=5)
    add_test(shm_${prec}unmlq ${SHM_TEST_CMD_LIST} ./testing_${prec}unmlq -M 287 -N 383 -K 97 -t 56 -x -v=5)
  else()
    add_test(shm_${prec}ormqr ${SHM_TEST_CMD_LIST} ./testing_${prec}ormqr -M 487 -N 283 -K 97 -t 56 -x -v=5)
    add_test(shm_${prec}ormlq ${SHM_TEST_CMD_LIST} ./testing_${prec}ormlq -M 287 -N 383 -K 97 -t 56 -x -v=5)
  endif()

  # QR / LQ: HQR
  add_test(shm_${prec}geqrf_hqr ${SHM_TEST_CMD_LIST} ./testing_${prec}geqrf_hqr -M 487 -N 283 -K 97 -t 56 -x -v=5)
  add_test(shm_${prec}gelqf_hqr ${SHM_TEST_CMD_LIST} ./testing_${prec}gelqf_hqr -M 287 -N 383 -K 97 -t 56 -x -v=5)
  if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
    add_test(shm_${prec}unmqr_hqr ${SHM_TEST_CMD_LIST} ./testing_${prec}unmqr_hqr -M 487 -N 283 -K 97 -t 56 -x -v=5)
    add_test(shm_${prec}unmlq_hqr ${SHM_TEST_CMD_LIST} ./testing_${prec}unmlq_hqr -M 287 -N 383 -K 97 -t 56 -x -v=5)
  else()
    add_test(shm_${prec}ormqr_hqr ${SHM_TEST_CMD_LIST} ./testing_${prec}ormqr_hqr -M 487 -N 283 -K 97 -t 56 -x -v=5)
    add_test(shm_${prec}ormlq_hqr ${SHM_TEST_CMD_LIST} ./testing_${prec}ormlq_hqr -M 287 -N 383 -K 97 -t 56 -x -v=5)
  endif()

  # QR / LQ: systolic
  add_test(shm_${prec}geqrf_systolic ${SHM_TEST_CMD_LIST} ./testing_${prec}geqrf_systolic -M 487 -N 283 -K 97 -t 56 -x -v=5)
  add_test(shm_${prec}gelqf_systolic ${SHM_TEST_CMD_LIST} ./testing_${prec}gelqf_systolic -M 287 -N 383 -K 97 -t 56 -x -v=5)
  if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
    add_test(shm_${prec}unmqr_systolic ${SHM_TEST_CMD_LIST} ./testing_${prec}unmqr_systolic -M 487 -N 283 -K 97 -t 56 -x -v=5)
    add_test(shm_${prec}unmlq_systolic ${SHM_TEST_CMD_LIST} ./testing_${prec}unmlq_systolic -M 287 -N 383 -K 97 -t 56 -x -v=5)
  else()
    add_test(shm_${prec}ormqr_systolic ${SHM_TEST_CMD_LIST} ./testing_${prec}ormqr_systolic -M 487 -N 283 -K 97 -t 56 -x -v=5)
    add_test(shm_${prec}ormlq_systolic ${SHM_TEST_CMD_LIST} ./testing_${prec}ormlq_systolic -M 287 -N 383 -K 97 -t 56 -x -v=5)
  endif()

  # LU
  add_test(shm_${prec}getrf        ${SHM_TEST_CMD_LIST} ./testing_${prec}getrf        -N 378 -t 93       -x -v=5)
  add_test(shm_${prec}getrf_incpiv ${SHM_TEST_CMD_LIST} ./testing_${prec}getrf_incpiv -N 378 -t 93 -i 17 -x -v=5)
  add_test(shm_${prec}getrf_nopiv  ${SHM_TEST_CMD_LIST} ./testing_${prec}getrf_nopiv  -N 378 -t 93       -x -v=5)
  add_test(shm_${prec}getrf_qrf    ${SHM_TEST_CMD_LIST} ./testing_${prec}getrf_qrf    -N 378 -t 93 -i 17 -x -v=5)

  #add_test(shm_${prec}gesv         ${SHM_TEST_CMD_LIST} ./testing_${prec}gesv         -N 874 -K 367 -t 76       -x -v=5)
  add_test(shm_${prec}gesv_incpiv  ${SHM_TEST_CMD_LIST} ./testing_${prec}gesv_incpiv  -N 874 -K 367 -t 76 -i 23 -x -v=5)

  #   if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
  #     add_test(shm_${prec}heev         ${SHM_TEST_CMD_LIST} ./testing_${prec}heev  -N 4000 -x -v=5)
  #   else()
  #     add_test(shm_${prec}syev         ${SHM_TEST_CMD_LIST} ./testing_${prec}syev  -N 4000 -x -v=5)
  #   endif()
  #   add_test(shm_${prec}geqrf_p0     ${SHM_TEST_CMD_LIST} ./testing_${prec}geqrf_param -N 4000 -t 200 -i 32 -x --qr_a=2 --treel 0 --tsrr=0 -v=5)
  #   add_test(shm_${prec}geqrf_p1     ${SHM_TEST_CMD_LIST} ./testing_${prec}geqrf_param -N 4000 -t 200 -i 32 -x --qr_a=2 --treel 1 --tsrr=0 -v=5)
  #   add_test(shm_${prec}geqrf_p2     ${SHM_TEST_CMD_LIST} ./testing_${prec}geqrf_param -N 4000 -t 200 -i 32 -x --qr_a=2 --treel 2 --tsrr=0 -v=5)
  #   add_test(shm_${prec}geqrf_p3     ${SHM_TEST_CMD_LIST} ./testing_${prec}geqrf_param -N 4000 -t 200 -i 32 -x --qr_a=2 --treel 3 --tsrr=0 -v=5)
  #
endforeach()

#  # Specific cases
#  # Do we want to test them in all precisions ?
#  add_test(dpotrf_pbq ${SHM_TEST_CMD_LIST} ./testing_dpotrf -N 4000 -x -v=5 -o PBQ)
#  add_test(dgeqrf_pbq ${SHM_TEST_CMD_LIST} ./testing_dgeqrf -N 4000 -x -v=5 -o PBQ)
#
#  # The headnode lack GPUs so we need MPI in order to get the test to run on
#  # one of the nodes.
#  if (CUDA_FOUND AND MPI_C_FOUND)
#    add_test(dpotrf_g1  ${SHM_TEST_CMD_LIST} ./testing_dpotrf -N 8000 -x -v=5 -g 1)
#    add_test(dpotrf_g2  ${SHM_TEST_CMD_LIST} ./testing_dpotrf -N 8000 -x -v=5 -g 2)
#  endif (CUDA_FOUND AND MPI_C_FOUND)

#
# Distributed Memory Testings
#
if( MPI_C_FOUND )
  find_program(BINTRUE true)
  set(PROCS "-np;4")
  set(CORES "-c;1")
  # Check MPI
  add_test(mpi_test   ${MPI_TEST_CMD_LIST} ${PROCS} ${BINTRUE})

  foreach(prec ${DPLASMA_PRECISIONS})

    # check the control and test matrices generation (zplrnt, zplghe, zplgsy, zpltmg) in distributed memory
    add_test(mpi_${prec}print ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}print ${CORES} -p 2 -N 64 -t 7 -x -v=5)
    set_tests_properties(mpi_${prec}print PROPERTIES DEPENDS mpi_test)

    # check the norms that are used in all other testings
    add_test(mpi_${prec}lange ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}lange ${CORES} -P 4 -M 287 -N 283 -K 97 -t 19 -x -v=5)
    set_tests_properties(mpi_${prec}lange PROPERTIES DEPENDS mpi_${prec}print)

    # Need to add testings on zlacpy, zlaset, zgeadd, zlascal, zger, (zlaswp?)

    # BLAS Shared memory
    add_test(mpi_${prec}trmm  ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}trmm  ${CORES} -P 4 -M 106 -N 150 -K 97 -t 19 -x -v=5)
    set_tests_properties(mpi_${prec}trmm PROPERTIES DEPENDS mpi_${prec}lange)

    add_test(mpi_${prec}trsm  ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}trsm  ${CORES} -P 4 -M 106 -N 150 -K 97 -t 19 -x -v=5)
    set_tests_properties(mpi_${prec}trsm PROPERTIES DEPENDS mpi_${prec}trmm)

    add_test(mpi_${prec}gemm  ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}gemm  ${CORES} -P 4 -M 106 -N 283 -K 97 -t 19 -x -v=5)
    set_tests_properties(mpi_${prec}trmm PROPERTIES DEPENDS mpi_${prec}lange)

    add_test(mpi_${prec}symm  ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}symm  ${CORES} -P 4 -M 106 -N 283 -K 97 -t 19 -x -v=5)
    set_tests_properties(mpi_${prec}trmm PROPERTIES DEPENDS mpi_${prec}lange)

    add_test(mpi_${prec}syrk  ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}syrk  ${CORES} -P 4 -M 287 -N 283 -K 97 -t 19 -x -v=5)
    add_test(mpi_${prec}syr2k ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}syr2k ${CORES} -P 4 -M 287 -N 283 -K 97 -t 19 -x -v=5)

    if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
      add_test(mpi_${prec}hemm  ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}hemm  ${CORES} -P 4 -M 106 -N 283 -K 97 -t 19 -x -v=5)
      add_test(mpi_${prec}herk  ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}herk  ${CORES} -P 4 -M 287 -N 283 -K 97 -t 19 -x -v=5)
      add_test(mpi_${prec}her2k ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}her2k ${CORES} -P 4 -M 287 -N 283 -K 97 -t 19 -x -v=5)
    endif()

    # Cholesky
    add_test(mpi_${prec}potrf ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}potrf ${CORES} -N 378 -t 19        -x -v=5)
    add_test(mpi_${prec}posv  ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}posv  ${CORES} -N 457 -t 19 -K 367 -x -v=5)

    # QR / LQ
    add_test(mpi_${prec}geqrf ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}geqrf ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
    add_test(mpi_${prec}gelqf ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}gelqf ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
      add_test(mpi_${prec}unmqr ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}unmqr ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
      add_test(mpi_${prec}unmlq ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}unmlq ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    else()
      add_test(mpi_${prec}ormqr ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}ormqr ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
      add_test(mpi_${prec}ormlq ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}ormlq ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    endif()

    # QR / LQ: HQR
    add_test(mpi_${prec}geqrf_hqr ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}geqrf_hqr ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
    add_test(mpi_${prec}gelqf_hqr ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}gelqf_hqr ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
      add_test(mpi_${prec}unmqr_hqr ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}unmqr_hqr ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
      add_test(mpi_${prec}unmlq_hqr ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}unmlq_hqr ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    else()
      add_test(mpi_${prec}ormqr_hqr ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}ormqr_hqr ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
      add_test(mpi_${prec}ormlq_hqr ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}ormlq_hqr ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    endif()

    # QR / LQ: systolic
    add_test(mpi_${prec}geqrf_systolic ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}geqrf_systolic ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
    add_test(mpi_${prec}gelqf_systolic ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}gelqf_systolic ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
      add_test(mpi_${prec}unmqr_systolic ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}unmqr_systolic ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
      add_test(mpi_${prec}unmlq_systolic ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}unmlq_systolic ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    else()
      add_test(mpi_${prec}ormqr_systolic ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}ormqr_systolic ${CORES} -P 4 -M 487 -N 283 -K 97 -t 19 -x -v=5)
      add_test(mpi_${prec}ormlq_systolic ${MPI_TEST_CMD_LIST} ${PROCS}  ./testing_${prec}ormlq_systolic ${CORES} -P 4 -M 287 -N 383 -K 97 -t 19 -x -v=5)
    endif()

    # LU
    add_test(mpi_${prec}getrf        ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}getrf        ${CORES} -P 1 -N 378 -t 19      -x -v=5)
    add_test(mpi_${prec}getrf_incpiv ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}getrf_incpiv ${CORES} -P 2 -N 378 -t 19 -i 7 -x -v=5)
    add_test(mpi_${prec}getrf_nopiv  ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}getrf_nopiv  ${CORES} -P 2 -N 378 -t 19      -x -v=5)
    add_test(mpi_${prec}getrf_qrf    ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}getrf_qrf    ${CORES} -P 2 -N 378 -t 19 -i 7 -x -v=5)

    #add_test(mpi_${prec}gesv         ${MPI_TEST_CMD_LIST} ./testing_${prec}gesv        ${CORES}  -N 874 -K 367 -t 76       -x -v=5)
    add_test(mpi_${prec}gesv_incpiv  ${MPI_TEST_CMD_LIST} ./testing_${prec}gesv_incpiv  ${CORES} -N 874 -K 367 -t 17 -i 7 -x -v=5)

    # if (CUDA_FOUND)
    #   add_test(mpi_${prec}potrf_g1     ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}potrf       ${CORES}  -p 2 -N 8000 -x -v=5 -g 1)
    #   SET_TESTS_PROPERTIES(mpi_${prec}potrf_g1 PROPERTIES DEPENDS mpi_test)
    # endif (CUDA_FOUND)

    # add_test(mpi_${prec}potrf_pbq    ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}potrf       ${CORES}  -p 2 -N 4000 -x -v=5 -o PBQ)
    # SET_TESTS_PROPERTIES(mpi_${prec}potrf_pbq PROPERTIES DEPENDS mpi_test)

    # add_test(mpi_${prec}geqrf_pbq    ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}geqrf       ${CORES}  -p 4 -N 4000 -x -v=5 -o PBQ)
    # SET_TESTS_PROPERTIES(mpi_${prec}geqrf_pbq PROPERTIES DEPENDS mpi_test)

    # add_test(mpi_${prec}geqrf_p0     ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}geqrf_param  ${CORES} -p 4 -N 4000 -t 200 -i 32 -x --qr_p=4 --qr_a=2 --treel 0 --tsrr=0 -v=5)
    # SET_TESTS_PROPERTIES(mpi_${prec}geqrf_p0 PROPERTIES DEPENDS mpi_test)

    # add_test(mpi_${prec}geqrf_p1     ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}geqrf_param  ${CORES} -p 4 -N 4000 -t 200 -i 32 -x --qr_p=4 --qr_a=2 --treel 1 --tsrr=0 -v=5)
    # SET_TESTS_PROPERTIES(mpi_${prec}geqrf_p1 PROPERTIES DEPENDS mpi_test)

    # add_test(mpi_${prec}geqrf_p2     ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}geqrf_param  ${CORES} -p 4 -N 4000 -t 200 -i 32 -x --qr_p=4 --qr_a=2 --treel 2 --tsrr=0 -v=5)
    # SET_TESTS_PROPERTIES(mpi_${prec}geqrf_p2 PROPERTIES DEPENDS mpi_test)

    # add_test(mpi_${prec}geqrf_p3     ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}geqrf_param  ${CORES} -p 4 -N 4000 -t 200 -i 32 -x --qr_p=4 --qr_a=2 --treel 3 --tsrr=0 -v=5)

    # SET_TESTS_PROPERTIES(mpi_${prec}geqrf_p3 PROPERTIES DEPENDS mpi_test)
    # if ( "${prec}" STREQUAL "c" OR "${prec}" STREQUAL "z" )
    #   add_test(mpi_${prec}heev         ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}heev -p 2 ${CORES} -N 2000 -x -v=5)
    # else()
    #   add_test(mpi_${prec}syev         ${MPI_TEST_CMD_LIST} ${PROCS} ./testing_${prec}syev -p 2 ${CORES} -N 2000 -x -v=5)
    # endif()
  endforeach()

endif( MPI_C_FOUND )
