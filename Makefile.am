bin_PROGRAMS = 75fasmod

75fasmod_SOURCES = main.cpp DataSocket.cpp

# Feed compiler and linker flags from pkg-config
75fasmod_CXXFLAGS = $(HAMLIB_CFLAGS) $(GNURADIO_CFLAGS) -Wall -Wextra -pthread
75fasmod_LDADD = $(HAMLIB_LIBS) $(GNURADIO_LIBS) -lpthread