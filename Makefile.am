bin_PROGRAMS = 75fasmod

#Tell the C++ preprocessor where to find the header files
    AM_CPPFLAGS =
        -I$(top_srcdir) / include

#List all source files from the src / directory
        75fasmod_SOURCES =
            src / main.cpp src / DataSocket.cpp src / cobs.cpp src /
            Frame.cpp src / RadioController.cpp src / ModemDSP.cpp src /
            TcmConfig.cpp

#Feed compiler and linker flags from pkg - config
            75fasmod_CXXFLAGS = $(HAMLIB_CFLAGS) $(GNURADIO_CFLAGS)
                                    $(FMT_CFLAGS) $(VOLK_CFLAGS) -
                                Wall - Wextra - pthread 75fasmod_LDADD =
                                    $(HAMLIB_LIBS) $(GNURADIO_LIBS) $(FMT_LIBS)
                                        $(VOLK_LIBS) -
                                    lpthread

#Formatting target using clang - format on both directories
                                        .PHONY : format format
    : clang - format - i $(srcdir) / src/*.cpp $(srcdir)/include/*.hpp
	@echo "C++ source and header files formatted successfully."

# Documentation targets
.PHONY: docs
docs: Doxyfile
	doxygen Doxyfile
	@echo "Documentation generated in docs/html/"

# Clean documentation
.PHONY: clean-docs
clean-docs:
	rm -rf docs/html docs/latex
	@echo "Documentation cleaned"

# Install documentation
.PHONY: install-docs
install-docs: docs
	@echo "Documentation generated in $(DESTDIR)$(docdir)/html/"

# Check if Doxygen is available
check-doxygen:
	@which doxygen > /dev/null || (echo "Doxygen not found, please install it" && exit 1)
