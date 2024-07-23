SUBNAME = trax
LIB = smartmet-$(SUBNAME)
SPEC = smartmet-library-$(SUBNAME)
INCDIR = smartmet/$(SUBNAME)

REQUIRES = gdal geos fmt

OPTIMIZE = -O3
include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc
CFLAGS += -Wno-maybe-uninitialized

# maybe-uninitialized: GCC analyzes incorrectly that a vertex may be uninitialized since all data members have defaults

DEFINES = -DUNIX -D_REENTRANT -DUSE_UNSTABLE_GEOS_CPP_API

# Common library compiling template

LIBS +=	\
	-lsmartmet-macgyver \
	$(REQUIRED_LIBS) \
	$(PREFIX_LDFLAGS)

# What to install

LIBFILE = lib$(LIB).so

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)
vpath %.o $(objdir)

# The files to be compiled

SRCS = $(wildcard $(SUBNAME)/*.cpp)
OBJS = $(patsubst $(SUBNAME)/%.cpp,obj/%.o,$(SRCS))

# Headers to install for development

HDRS =	$(SUBNAME)/BBox.h \
	$(SUBNAME)/Contour.h \
	$(SUBNAME)/Cell.h \
	$(SUBNAME)/GeometryCollection.h \
	$(SUBNAME)/Geos.h \
	$(SUBNAME)/Grid.h \
	$(SUBNAME)/GridPoint.h \
	$(SUBNAME)/InterpolationType.h \
	$(SUBNAME)/IsobandLimits.h \
	$(SUBNAME)/IsolineValues.h \
	$(SUBNAME)/MirrorMatrix.h \
	$(SUBNAME)/OGR.h \
	$(SUBNAME)/Point.h \
	$(SUBNAME)/Polygon.h \
	$(SUBNAME)/Polyline.h \
	$(SUBNAME)/Range.h

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE)

debug: all
release: all
profile: all

$(LIBFILE): $(OBJS)
	$(CXX) $(CFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol |\
			grep -Pv ':\ __(?:(?:a|t|ub)san_|sanitizer_)'; \
	then \
		rm -v $(LIBFILE); \
		exit 1; \
	fi

clean:
	rm -f $(LIBFILE) $(OBJS) $(patsubst obj/%.o,obj/%.d,$(OBJS)) *~ $(SUBNAME)/*~
	rm -f test/*Test

format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp test/*.cpp

install:
	@mkdir -p $(includedir)/$(INCDIR)
	@list='$(HDRS)'; \
	for hdr in $$list; do \
	  HDR=$$(basename $$hdr); \
	  echo $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$HDR; \
	  $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$HDR; \
	done
	@mkdir -p $(libdir)
	echo $(INSTALL_PROG) $(LIBFILE) $(libdir)/$(LIBFILE)
	$(INSTALL_PROG) $(LIBFILE) $(libdir)/$(LIBFILE)

test:
	$(MAKE) -C test $@

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o : %.cpp
	@mkdir -p obj
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
