#!make
include ./Make.def
P_ROOT = ./..
P_OUTDIR = .

OBJDIR=./objs/$(LIBTAG)
SRCDIR=.


SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))
DEPS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.d, $(SOURCES))



BASELIB_INCS=	\
	-I/usr/local/ffmpeg/include


BASELIB_LIBS=	\
	

COMMON_INCS=	\


####################################################################
# Compile and link options

PLUGIN_INCLUDES =               \
	$(COMMON_INCS) $(BASELIB_INCS)

PLUGIN_LIBS =	\
	$(BASELIB_LIBS)


CXXFLAGS +=  $(PLUGIN_INCLUDES) \
      -DUNIX -DLINUX -Dlinux -Wall	$(PG)	\
	  -DSQLITE_TEMP_STORE=3	\

#CXXFLAGS += $(shell pkg-config --cflags gconf-2.0 glib-2.0 gobject-2.0)

#     -g -DUNIX -DLINUX -Dlinux -Wall -O2 -Os\

CXXLIBS =	\
	-lrt -ldl -lpthread  $(PG)
	
#CXXLIBS  += $(shell pkg-config --libs gconf-2.0 glib-2.0 gobject-2.0)

LFLAGS	=	-Wl	

CRYPTO= -L/usr/lib -lcrypto

FFMPEGLIBS = -L/usr/local/ffmpeg/lib -lavdevice -lavfilter -lavformat -lavcodec -lswresample -lswscale -lavutil
#FFMPEGLIBS = -L/usr/local/ffmpeg/lib -lavutil -lswscale -lswresample -lavcodec -lavformat -lavfilter -lavdevice
#FFMPEGLIBS = -L/usr/local/ffmpeg/lib -lavutil

####################################################################
TARGET = $(P_OUTDIR)/test.$(LIBTAG)
####################################################################
# make all
# client:all

all: checkoutdir $(TARGET) $(TARGET_FULL)

$(TARGET): $(OBJS) $(PLUGIN_LIBS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(PLUGIN_LIBS) $(CXXLIBS) $(FFMPEGLIBS)

$(OBJDIR)/%.o:$(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ -MD $<

$(SRCDIR)/%.d:$(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $(basename $@).o -MD $<
	

install: checkinstdir $(TARGET)
	install -m 755 $(TARGET) $(P_INST_BINDIR)

checkoutdir: 
	@if test ! -d $(P_OUTDIR) ; \
	then \
		mkdir -p $(P_OUTDIR) ; \
	fi
	@if test ! -d $(OBJDIR) ; \
	then \
		mkdir -p $(OBJDIR) ; \
	fi


-include $(DEPS)
####################################################################
# clean
cleancli:clean
clean:
	$(RM) $(OBJS) $(TARGET)  $(DEPS)
