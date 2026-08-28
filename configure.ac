AC_INIT([75fasmod], [0.1.0], [your.email@example.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CXX

# Force C++20 for std::jthread and modern features
CXXFLAGS="$CXXFLAGS -std=c++20"

# Check for required libraries using pkg-config
PKG_CHECK_MODULES([HAMLIB], [hamlib])
PKG_CHECK_MODULES([GNURADIO], [gnuradio-runtime])
PKG_CHECK_MODULES([GNURADIO], [gnuradio-runtime gnuradio-blocks gnuradio-audio gnuradio-digital])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
