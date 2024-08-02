PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

REQUIRES = fmt gdal geos

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

DEFINES = -DUSE_UNSTABLE_GEOS_CPP_API
FLAGS = -std=$(CXX_STD) -Wall -W -fdiagnostics-color=$(GCC_DIAG_COLOR) -Wno-unused-parameter

ifeq ($(TSAN), yes)
  FLAGS += -fsanitize=thread
endif
ifeq ($(ASAN), yes)
  FLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=undefined -fsanitize-address-use-after-scope
endif

CFLAGS = -DUNIX -O0 -g $(FLAGS)

INCLUDES += -I../trax

LIBS += ../libsmartmet-trax.so \
	$(REQUIRED_LIBS) \
	$(PREFIX_LDFLAGS)

all: $(PROG)
clean:
	rm -f $(PROG) *~

test: $(PROG)
	@echo Running tests:
	@ok=true; \
	for prog in $(PROG); do \
	./$$prog || ok=false; \
	done; \
	$$ok

$(PROG) : % : %.cpp Makefile ../libsmartmet-trax.so
	$(CXX) $(CFLAGS) $(DEFINES) -o $@ $@.cpp $(INCLUDES) $(LIBS)
