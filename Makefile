CXX        = g++
CXXFLAGS   = -g3 -Wall -Werror -std=c++11
CXXPATHS   = -I.
LDFLAGS    =
RM         = rm -f
AR         = ar rsc
GILFLAGS   = -w 32

LIBSRCS    = $(patsubst %,uwt/trans/%, cfg.cc cfg_testbench.cc functionlab.cc ibloc.cc statement.cc insnstmt.cc ctrlstmt.cc core.cc)
LIBSRCS   += $(patsubst %,uwt/win/%, memmgr.cc module.cc pef.cc winnt.cc filesystem.cc)
LIBSRCS   += $(patsubst %,uwt/x86/%, context.cc flags.cc memory.cc calldb.cc)
LIBSRCS   += $(patsubst %,uwt/utils/%, logchannels.cc misc.cc bytefield.cc)
LIBSRCS   += $(patsubst %,uwt/%, iterbox.cc launchbox.cc)
LIBSRCS   += $(patsubst %,unisim/component/cxx/processor/intel/%, disasm.cc)
LIBSRCS   += $(patsubst %,unisim/util/symbolic/%, symbolic.cc ccode/ccode.cc)
EXESRCS   += $(patsubst %,uwt/%, unwintel.cc) 

BUILD      = build
LIBOBJS    = $(patsubst %.cc,$(BUILD)/%.o,$(LIBSRCS))
EXEOBJS    = $(patsubst %.cc,$(BUILD)/%.o,$(EXESRCS))
ALLOBJS    = $(LIBOBJS) $(EXEOBJS)
ALLDEPS    = $(patsubst %.o,%.d,$(ALLOBJS))
EXE        = unwintel
LIB        = libuwt.a

.PHONY: all
all: $(EXE) $(LIB)

$(EXE): $(EXEOBJS) $(LIB)
	$(CXX) $(LDFLAGS) -o $@ $(EXEOBJS) $(LIB)

$(LIB): $(LIBOBJS)
	$(AR) $@ $(LIBOBJS)

$(ALLOBJS): $(BUILD)/%.o: %.cc
	@mkdir -p `dirname $@`
	$(CXX) $(CXXFLAGS) $(CXXPATHS) -MMD -MP -o $@ -c $<

.PHONY: clean
clean:
	$(RM) -R $(BUILD)
	$(RM) $(EXE) $(LIB)

-include $(ALLDEPS)
