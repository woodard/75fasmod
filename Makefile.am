bin_PROGRAMS = 75fasmod

# Test programs
check_PROGRAMS = test_frequency test_power test_vfo test_tnc test_single_dual test_other_mode test_other_frequency

test_frequency_SOURCES = test/test_frequency.cpp src/RadioController.cpp src/THD75.cpp
test_frequency_CPPFLAGS = -I$(top_srcdir)/include $(HAMLIB_CFLAGS) -std=c++20
test_frequency_LDADD = $(HAMLIB_LIBS)

test_power_SOURCES = test/test_power.cpp src/RadioController.cpp src/THD75.cpp
test_power_CPPFLAGS = -I$(top_srcdir)/include $(HAMLIB_CFLAGS) -std=c++20
test_power_LDADD = $(HAMLIB_LIBS)

test_vfo_SOURCES = test/test_vfo.cpp src/RadioController.cpp src/THD75.cpp
test_vfo_CPPFLAGS = -I$(top_srcdir)/include $(HAMLIB_CFLAGS) -std=c++20
test_vfo_LDADD = $(HAMLIB_LIBS)

test_tnc_SOURCES = test/test_tnc.cpp src/RadioController.cpp src/THD75.cpp
test_tnc_CPPFLAGS = -I$(top_srcdir)/include $(HAMLIB_CFLAGS) -std=c++20
test_tnc_LDADD = $(HAMLIB_LIBS)

test_single_dual_SOURCES = test/test_single_dual.cpp src/RadioController.cpp src/THD75.cpp
test_single_dual_CPPFLAGS = -I$(top_srcdir)/include $(HAMLIB_CFLAGS) -std=c++20
test_single_dual_LDADD = $(HAMLIB_LIBS)

test_other_mode_SOURCES = test/test_other_mode.cpp src/RadioController.cpp src/THD75.cpp
test_other_mode_CPPFLAGS = -I$(top_srcdir)/include $(HAMLIB_CFLAGS) -std=c++20
test_other_mode_LDADD = $(HAMLIB_LIBS)

test_other_frequency_SOURCES = test/test_other_frequency.cpp src/RadioController.cpp src/THD75.cpp
test_other_frequency_CPPFLAGS = -I$(top_srcdir)/include $(HAMLIB_CFLAGS) -std=c++20
test_other_frequency_LDADD = $(HAMLIB_LIBS)

# Tell the C++ preprocessor where to find the header files
AM_CPPFLAGS = -I$(top_srcdir)/include

# List all source files from the src/ directory
75fasmod_SOURCES = \
	src/main.cpp \
	src/DataSocket.cpp \
	src/cobs.cpp \
	src/Frame.cpp \
	src/RadioController.cpp \
	src/THD75.cpp \
	src/ModemDSP.cpp \
	src/TcmConfig.cpp

# Feed compiler and linker flags from pkg-config
75fasmod_CXXFLAGS = $(HAMLIB_CFLAGS) $(GNURADIO_CFLAGS) $(FMT_CFLAGS) $(VOLK_CFLAGS) -Wall -Wextra -pthread
75fasmod_LDADD = $(HAMLIB_LIBS) $(GNURADIO_LIBS) $(FMT_LIBS) $(VOLK_LIBS) -lpthread

# Test runner
TESTS = test/run_frequency_test.sh test/run_power_test.sh \
        test/run_vfo_test.sh test/run_tnc_test.sh test/run_single_dual_test.sh \
        test/run_other_mode_test.sh \
        test/run_other_frequency_test.sh
EXTRA_DIST = test/run_frequency_test.sh test/run_power_test.sh \
             test/run_vfo_test.sh test/run_tnc_test.sh \
             test/run_single_dual_test.sh \
             test/run_other_mode_test.sh \
             test/run_other_frequency_test.sh

# Documentation targets
.PHONY: docs
docs: Doxyfile
	doxygen Doxyfile

.PHONY: clean-docs
clean-docs:
	rm -rf docs/html docs/latex

.PHONY: install-docs
install-docs: docs
