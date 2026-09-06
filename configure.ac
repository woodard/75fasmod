AC_INIT([75fasmod], [0.1.0], [your.email@example.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_PROG_CXX
AM_PROG_AR

#Force C++ 20 for std::jthread and modern features
CXXFLAGS="$CXXFLAGS -std=c++20"

#Check for required libraries using pkg-config
PKG_CHECK_MODULES([HAMLIB], [hamlib])
PKG_CHECK_MODULES([GNURADIO], [gnuradio-runtime gnuradio-audio gnuradio-blocks gnuradio-digital gnuradio-filter gnuradio-trellis])
PKG_CHECK_MODULES([FMT], [fmt >= 8.0.0])
PKG_CHECK_MODULES([VOLK], [volk])

# Check for boost_program_options
BOOST_CFLAGS="-I/usr/include"
BOOST_LIBS="-L/usr/lib64 -lboost_program_options"
AC_SUBST([BOOST_CFLAGS])
AC_SUBST([BOOST_LIBS])

AC_CONFIG_FILES([Makefile Doxyfile])
AC_OUTPUT
