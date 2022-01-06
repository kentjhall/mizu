CXX := g++
INCLUDES := -I ~/mizu-services -I ~/yuzu/src -I ~/yuzu/externals/dynarmic/externals/fmt/include -I ~/yuzu/src/externals/opus/opus/include -I ~/yuzu/externals/cubeb/include -I ~/yuzu/externals/soundtouch/include -I ~/yuzu/src/exports -I ~/yuzu/externals/SDL/include -I ~/yuzu/externals/microprofile -I ~/yuzu/externals/opus/opus/include
CXXFLAGS := -DYUZU_UNIX -DFMT_HEADER_ONLY -std=gnu++2a $(INCLUDES)
LDFLAGS := -L ~/mbedtls/library/
LDLIBS := -lmbedcrypto -lrt -pthread

services := sm set apm am acc bcat glue hid ns filesystem

subdirs := common common/fs common/logging \
	   core core/file_sys core/file_sys/system_archive core/file_sys/system_archive/data core/crypto \
	   core/frontend/applets core/hle core/hle/kernel \
	   core/hle/service $(shell find $(addprefix core/hle/service/,$(services)) -type d)
sources := $(wildcard *.cpp) $(wildcard $(addsuffix /*.cpp,$(subdirs)))
headers := $(wildcard $(patsubst %.cpp,%.h,$(sources))) mizu_servctl.h
objects := $(patsubst %.cpp,%.o,$(sources))

mizu_service_pack: $(objects)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(objects): $(headers)

.PHONY: missing
missing: missing_syms.txt
	@cat missing_syms.txt
missing_syms.txt: ld_err.txt
	@grep 'undefined reference to `' ld_err.txt | cut -d '`' -f 2 | cut -d "'" -f 1 | sort | uniq > missing_syms.txt
ld_err.txt: $(objects)
	@$(CXX) $(LDFLAGS) -o /dev/null $^ $(LDLIBS) 2> ld_err.txt || true

.PHONY: clean
clean:
	rm -f mizu_service_pack
	find . -name '*.o' -exec rm {} \;
