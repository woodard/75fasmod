bin_PROGRAMS = 75fasmod

# Tell the C++ preprocessor where to find the header files
AM_CPPFLAGS = -I$(top_srcdir)/include

# List all source files from the src/ directory
75fasmod_SOURCES = \
    src/main.cpp \
    src/DataSocket.cpp \
    src/cobs.cpp \
    src/crc32.cpp \
    src/RadioController.cpp \
    src/ModemDSP.cpp

# Feed compiler and linker flags from pkg-config
75fasmod_CXXFLAGS = $(HAMLIB_CFLAGS) $(GNURADIO_CFLAGS) -Wall -Wextra -pthread
75fasmod_LDADD = $(HAMLIB_LIBS) $(GNURADIO_LIBS) -lpthread

# Formatting target using clang-format on both directories
.PHONY: format
format:
	clang-format -i $(srcdir)/src/*.cpp $(srcdir)/include/*.hpp
	@echo "C++ source and header files formatted successfully."