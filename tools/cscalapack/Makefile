MPICC=mpicc
MPIF77=mpif77
CFLAGS="-Wall"
CC=$(MPICC)
LD=gfortran
SMKL=-L/opt/mkl/lib/em64t -lmkl_scalapack_lp64 -lmkl_lapack -lmkl_blacs_openmpi_lp64 -lmkl_sequential -lmkl_gf_lp64 -lmkl_core 
TMKL=-L/opt/mkl/lib/em64t -lmkl_scalapack_lp64 -lmkl_lapack -lmkl_blacs_openmpi_lp64 -lmkl -lmkl_gf_lp64 -lmkl_core -lguide -lpthread
TLIBS=$(TMKL)
SLIBS=$(SMKL)
TARGETS=scadpotrf scadgetrf scadgeqrf tscadpotrf tscadgetrf tscadgeqrf

all: $(TARGETS)

scadpotrf: scalapackPOTRF.o
	$(LD) -o $@ $^ $(SLIBS)

scadgetrf: scalapackLU.o
	$(LD) -o $@ $^ $(SLIBS)

scadgeqrf: scalapackQR.o
	$(LD) -o $@ $^ $(SLIBS)

tscadpotrf: scalapackPOTRF.o
	$(LD) -o $@ $^ $(TLIBS)

tscadgetrf: scalapackLU.o
	$(LD) -o $@ $^ $(TLIBS)

tscadgeqrf: scalapackQR.o
	$(LD) -o $@ $^ $(TLIBS)

clean:
	rm *.o $(TARGETS)

