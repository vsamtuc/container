
AUTOMAKE_OPTIONS = foreign

CXXFLAGS=
if DEBUG
#AM_CXXFLAGS= -Wall -std=gnu++17 -g3
AM_CXXFLAGS= -Wall -std=c++17 -g3
else
#AM_CXXFLAGS= -Wall -std=gnu++17 -Ofast -DNDEBUG 
AM_CXXFLAGS= -Wall -std=c++17 -Ofast -DNDEBUG 
endif

AM_CXXFLAGS+= $(HDF5_CPPFLAGS) $(JSONCPP_CPPFLAGS)

lib_LIBRARIES= 

include_HEADERS= cdi.hh utilities.hh exceptions.hh qualifiers.hh \
	 resource.hh contextual.hh scope.hh  container.hh

EXTRA_DIST= $(include_HEADERS)

#
# Testing: 
# 1. add to TESTS
# 2. add SOURCES and LDADD
# 
# 

TESTS = unit_tests
check_PROGRAMS= $(TESTS)

unit_tests_SOURCES= unit_tests.cc provider_tests.cc resource_tests.cc qualifiers_tests.cc \
	utilities_tests.cc scope_tests.cc container_tests.cc
#unit_tests_LDADD= $(JSONCPP_LIBS) 

unit_tests.cc:
	cxxtestgen --root --runner=ErrorPrinter -o $@ $<

%_tests.cc: %_tests.hh
	cxxtestgen --part --runner=ErrorPrinter -o $@ $^

BUILT_SOURCES = provider_tests.cc resource_tests.cc qualifiers_tests.cc utilities_tests.cc  unit_tests.cc
MAINTAINERCLEANFILES = $(BUILT_SOURCES)

# documentation
@DX_RULES@
