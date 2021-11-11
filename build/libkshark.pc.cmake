prefix=@_INSTALL_PREFIX@
libdir=@_LIBDIR@
includedir=${prefix}/include/@KS_APP_NAME@

Name: libkshark
URL: https://git.kernel.org/pub/scm/utils/trace-cmd/kernel-shark.git/
Description: Library for accessing ftrace file system
Version: @KS_VERSION_STRING@
Requires: json-c, libtracecmd >= @LIBTRACECMD_MIN_VERSION@
Cflags: -I${includedir}
Libs: -L${libdir} -lkshark
