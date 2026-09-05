bin_PROGRAMS = 75fasmod

# Test programs
check_PROGRAMS = test_frequency

test_frequency_SOURCES = test/test_frequency.cpp src/RadioController.cpp
test_frequency_CPPFLAGS = -I$(top_srcdir)/include $(HAMLIB_CFLAGS) -std=c++20
test_frequency_LDADD = $(HAMLIB_LIBS)

# Tell the C++ preprocessor where to find the header files
AM_CPPFLAGS = -I$(top_srcdir)/include

# List all source files from the src/ directory
75fasmod_SOURCES = \
	src/main.cpp \
	src/DataSocket.cpp \
	src/cobs.cpp \
	src/Frame.cpp \
	src/RadioController.cpp \
	src/ModemDSP.cpp \
	src/TcmConfig.cpp

# Feed compiler and linker flags from pkg-config
75fasmod_CXXFLAGS = $(HAMLIB_CFLAGS) $(GNURADIO_CFLAGS) $(FMT_CFLAGS) $(VOLK_CFLAGS) -Wall -Wextra -pthread
75fasmod_LDADD = $(HAMLIB_LIBS) $(GNURADIO_LIBS) $(FMT_LIBS) $(VOLK_LIBS) -lpthread

# Test runner
TESTS = test/run_frequency_test.sh
EXTRA_DIST = test/run_frequency_test.sh

# Documentation targets
.PHONY: docs
docs: Doxyfile
	doxygen Doxyfile

.PHONY: clean-docs
clean-docs:
	rm -rf docs/html docs/latex

.PHONY: install-docs
install-docs: docs