ODIR=obj
SDIR=src
BDIR=bin
TDIR=t
CXX=g++
CC=gcc
CFLAGS=-Wall -fexceptions -fPIC -DLIQUID_INCLUDE_WEB_DIALECT -DLIQUID_INCLUDE_RAPIDJSON_VARIABLE
CXXFLAGS=$(CFLAGS) -std=c++17
LDFLAGS=-lcrypto -lssl
AR=ar
RUBY_DIR=ruby
PERL_DIR=perl
SOURCES=$(wildcard $(SDIR)/*.cpp) $(wildcard $(SDIR)/*.c) $(wildcard $(TDIR)/*.cpp)
TESTSOURCES=$(wildcard $(TDIR)/*.cpp)
OBJECTS=$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(patsubst $(SDIR)/%,$(ODIR)/%,$(SOURCES))))
NONLIBRARYSOURCES=$(TESTSOURCES)
LIBRARYSOURCES=$(filter-out $(NONLIBRARYSOURCES),$(SOURCES))
LIBRARYOBJECTS=$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(patsubst $(SDIR)/%,$(ODIR)/%,$(LIBRARYSOURCES))))
LIBRARY = $(BDIR)/libliquid.a
DEPENDS=$(wildcard $(ODIR)/*.d)
DEBUGFLAGS=-g3 -pthread -gdwarf-4 -Wno-deprecated -fno-elide-type -fdiagnostics-show-template-tree -Wall -Werror -Wextra -Wvla -Wextra-semi -Wnull-dereference -fvar-tracking-assignments -Wduplicated-cond -Wduplicated-branches -Wsuggest-override -Wno-unused-parameter

$(BDIR)/01sanity: $(LIBRARY) $(ODIR)/01sanity.o
	$(CXX) $(ODIR)/01sanity.o -L$(BDIR) -lliquid -lgtest -lpthread -o $(BDIR)/01sanity $(LDFLAGS)

test: CFLAGS := $(CFLAGS) $(DEBUGFLAGS)
test: $(BDIR)/01sanity

$(LIBRARY): $(LIBRARYOBJECTS)
	$(AR) -r -s $(LIBRARY) $(LIBRARYOBJECTS)

$(ODIR)/%.d: $(SDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -MM $< -MMD -MP -MT '$(patsubst %.d,%.o,$@)' -MF $@

$(ODIR)/%.d: $(TDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -MM $< -MMD -MP -MT '$(patsubst %.d,%.o,$@)' -MF $@

$(ODIR)/%.o: $(SDIR)/%.cpp $(ODIR)/%.d
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ODIR)/%.o: $(TDIR)/%.cpp $(ODIR)/%.d
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ODIR)/%.o: $(SDIR)/%.d
	$(CC) $(CFLAGS) -M $< -MMD -MP -MF $@

$(ODIR)/%.o: $(SDIR)/%.c $(ODIR)/%.d
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(ODIR)/%.d

perl : $(LIBRARY)
	cd $(PERL_DIR) && perl Makefile.PL && ln -f -s ../$(LIBRARY)
	$(MAKE) -C $(PERL_DIR) clean
	$(MAKE) -C $(PERL_DIR)

library: $(LIBRARY)

cleanlibrary: clean

directories: $(ODIR) $(BDIR) $(TDIR)

cleantest: clean

$(ODIR):
	mkdir -p $(ODIR)
$(BDIR):
	mkdir -p $(BDIR)
$(TDIR):
	mkdir -p $(TDIR)

-include $(DEPENDS)

libraryRelease: CFLAGS := $(CFLAGS) -O3 -s
libraryRelease: library

libraryDebug: CFLAGS := $(CFLAGS) $(DEBUGFLAGS)
libraryDebug: library

clean:
	rm -f $(ODIR)/*.o $(ODIR)/*.d $(LIBRARY)

