################################################################################
# AcquisitionMultipleCamerasWriteToFile Makefile
################################################################################
PROJECT_ROOT=/opt/spinnaker
OPT_INC = ${PROJECT_ROOT}/common/make/common_spin.mk
-include ${OPT_INC}

################################################################################
# Key paths and settings
################################################################################
CFLAGS += -std=c++11
ifeq ($(wildcard ${OPT_INC}),)
CXX = g++ ${CFLAGS}
ODIR  = .obj/build${D}
SDIR  = .
MKDIR = mkdir -p
PLATFORM = $(shell uname)
ifeq ($(PLATFORM),Darwin)
OS = mac
endif
endif
ifeq ($(OS), mac)
CFLAGS += -mmacosx-version-min=11.0
LDFLAGS += -mmacosx-version-min=11.0
else
LDFLAGS += 
endif

OUTPUTNAME = Synchronized${D}
OUTDIR = ./

################################################################################
# Dependencies
################################################################################
# Spinnaker deps
SPINNAKER_LIB = -L/opt/spinnaker/lib -lSpinnaker${D} ${SPIN_DEPS}

################################################################################
# Master inc/lib/obj/dep settings
################################################################################
_OBJ = Synchronized.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
INC = -I../../include
ifneq ($(OS),mac)
INC += -I/opt/spinnaker/include
LIB += -Wl,-Bdynamic ${SPINNAKER_LIB}
LIB += -Wl,-rpath-link=/opt/spinnaker/lib
else
INC += -I/usr/local/include/spinnaker
LIB += -rpath /opt/spinnaker/lib/
LIB += ${SPINNAKER_LIB}
LIB += -rpath /usr/local/lib/
endif

################################################################################
# Rules/recipes
################################################################################
# Final binary
${OUTPUTNAME}: ${OBJ}
	${CXX} ${LDFLAGS} -o ${OUTPUTNAME} ${OBJ} ${LIB}
	mv ${OUTPUTNAME} ${OUTDIR}

# Intermediate object files
${OBJ}: ${ODIR}/%.o : ${SDIR}/%.cpp
	@${MKDIR} ${ODIR}
	${CXX} ${CFLAGS} ${INC} -Wall -D LINUX -c $< -o $@

# Clean up intermediate objects
clean_obj:
	rm -f ${OBJ}
	@echo "intermediate objects cleaned up!"

# Clean up everything.
clean: clean_obj
	rm -f ${OUTDIR}/${OUTPUTNAME}
	@echo "all cleaned up!"
