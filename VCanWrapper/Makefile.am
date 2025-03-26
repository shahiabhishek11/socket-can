AM_CFLAGS = \
     -I./ \
     -D__func__=__PRETTY_FUNCTION__ \
     -DLINUX_AUTO_MAKE

AM_CPPFLAGS = -std=c++11

ACLOCAL_AMFLAGS = -I m4

libVCanWrapper_la_SOURCES = \
    VCanWrapper.cpp \
    VCwBase.cpp     \
    VCwBlockingQueue.cpp \
    VCwEar.cpp      \
    VCwEarSet.cpp   \
    VCwFrame.cpp    \
    VCwFilterManager.cpp \
    VCwLock.cpp     \
    VCwNodeSet.cpp  \
    VCwSim.cpp      \
    VCwSock.cpp     \
    VCwSockCan.cpp  \
    VCwCanSimulation.cpp \
    VCwWorker.cpp \
    VCwDropFrameLog.cpp \
    VCwSettings.cpp

if USE_GLIB
libVCanWrapper_la_CFLAGS = -DUSE_GLIB $(AM_CFLAGS) @GLIB_CFLAGS@
libVCanWrapper_la_LDFLAGS = -lrt -lstdc++ -lpthread @GLIB_LIBS@ -shared -version-info 1:0:0
libVCanWrapper_la_CPPFLAGS = -DUSE_GLIB $(AM_CFLAGS) $(AM_CPPFLAGS) @GLIB_CFLAGS@
else
libVCanWrapper_la_CFLAGS = $(AM_CFLAGS)
libVCanWrapper_la_LDFLAGS = -lrt -lpthread -shared -version-info 1:0:0
libVCanWrapper_la_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS)
endif

libVCanWrapper_la_LIBADD = -lcutils


library_includedir = $(pkgincludedir)

#Create and Install libraries
lib_LTLIBRARIES = libVCanWrapper.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libVCanWrapper.pc
EXTRA_DIST = $(pkgconfig_DATA)

