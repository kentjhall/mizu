CXX := g++
CC := g++
INCLUDES := -I ~/mizu-services -I ~/mizu-services/glad/include -I ~/yuzu/externals/dynarmic/externals/fmt/include -I ~/yuzu/src/externals/opus/opus/include -I ~/yuzu/externals/cubeb/include -I ~/yuzu/externals/soundtouch/include -I ~/yuzu/src/exports -I ~/yuzu/externals/SDL/include -I ~/yuzu/externals/microprofile -I ~/yuzu/externals/opus/opus/include -I ~/yuzu/externals/ffmpeg -I ~/yuzu/build/externals/ffmpeg -I ~/yuzu/externals/Vulkan-Headers/include -I ~/yuzu/externals/SDL/include -I ~/yuzu/externals/sirit/externals/SPIRV-Headers/include -I ~/yuzu/externals/sirit/include
DEBUG-y := -O0 -ggdb -D_DEBUG -fsanitize=address -static-libasan
CXXFLAGS := -DYUZU_UNIX -DHAS_OPENGL -DFMT_HEADER_ONLY -DMBEDTLS_CMAC_C -DSDL_VIDEO_DRIVER_X11 -DHAVE_SDL2 -std=gnu++2a $(shell pkg-config --cflags Qt5Gui Qt5Widgets libusb-1.0 glfw3 libavutil libavcodec libswscale INIReader liblz4 opus) $(INCLUDES) -I /usr/include/aarch64-linux-gnu/qt5/QtGui/5.15.2/QtGui -I ~/soundtouch/include $(DEBUG-$(DEBUG))
CFLAGS := $(CXXFLAGS)
LDFLAGS := -L ~/sirit/build/src -L ~/soundtouch/build $(DEBUG-$(DEBUG))
LDLIBS := -lmbedcrypto -lsirit -lrt -ldl -lcubeb -lSoundTouch -pthread $(shell pkg-config --libs Qt5Gui Qt5Widgets libusb-1.0 glfw3 sdl2 libavutil libavcodec libswscale INIReader liblz4 opus)

services := sm set apm am acc bcat glue hid ns filesystem nvflinger vi nvdrv time lm aoc pctl audio ptm friend

shader_headers := astc_decoder_comp.h \
                  block_linear_unswizzle_2d_comp.h \
                  block_linear_unswizzle_3d_comp.h \
                  convert_depth_to_float_frag.h \
                  convert_float_to_depth_frag.h \
                  full_screen_triangle_vert.h \
                  opengl_copy_bc4_comp.h \
                  opengl_present_frag.h \
                  opengl_present_vert.h \
                  pitch_unswizzle_comp.h \
                  astc_decoder_comp_spv.h \
                  block_linear_unswizzle_2d_comp_spv.h \
                  block_linear_unswizzle_3d_comp_spv.h \
                  convert_depth_to_float_frag_spv.h \
                  convert_float_to_depth_frag_spv.h \
                  full_screen_triangle_vert_spv.h \
                  pitch_unswizzle_comp_spv.h \
                  vulkan_blit_color_float_frag_spv.h \
                  vulkan_blit_depth_stencil_frag_spv.h \
                  vulkan_present_frag_spv.h \
                  vulkan_present_vert_spv.h \
                  vulkan_quad_indexed_comp_spv.h \
                  vulkan_uint8_comp_spv.h

subdirs := common common/fs common/logging configuration \
	   core core/file_sys core/file_sys/system_archive core/file_sys/system_archive/data core/crypto \
	   core/frontend core/loader core/frontend/applets core/hle core/hle/kernel \
	   $(shell find video_core -type d) $(shell find audio_core -type d) $(shell find shader_recompiler -type d) \
	   input_common $(shell find input_common -type d)  \
	   core/hle/service $(shell find $(addprefix core/hle/service/,$(services)) -type d)
sources := $(wildcard *.cpp) $(wildcard $(addsuffix /*.cpp,$(subdirs))) video_core/bootmanager.moc.cpp
headers := $(wildcard $(patsubst %.cpp,%.h,$(sources))) $(addprefix video_core/host_shaders/,$(shader_headers)) \
	   glad/include/glad/glad.h mizu_servctl.h
objects := $(patsubst %.cpp,%.o,$(sources)) glad/src/glad.o

.PHONY: default
default: horizon-services hlaunch

horizon-services: $(objects)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

hlaunch: hlaunch.c
	gcc $(DEBUG-$(DEBUG)) -Wall -o $@ $^ -lrt

$(objects): $(headers)

%.moc.cpp: %.h
	moc -o $@ $^

video_core/host_shaders/%_comp.h: video_core/host_shaders/%.comp video_core/host_shaders/source_shader.h.in
	cmake -P video_core/host_shaders/StringShaderHeader.cmake $< $@ $(word 2,$^)
video_core/host_shaders/%_frag.h: video_core/host_shaders/%.frag video_core/host_shaders/source_shader.h.in
	cmake -P video_core/host_shaders/StringShaderHeader.cmake $< $@ $(word 2,$^)
video_core/host_shaders/%_vert.h: video_core/host_shaders/%.vert video_core/host_shaders/source_shader.h.in
	cmake -P video_core/host_shaders/StringShaderHeader.cmake $< $@ $(word 2,$^)
video_core/host_shaders/%_comp_spv.h: video_core/host_shaders/%.comp
	glslangValidator -V --quiet --variable-name $(shell echo $(shell basename -s _spv.h $@) | tr '[:lower:]' '[:upper:]')_SPV -o $@ $<
video_core/host_shaders/%_frag_spv.h: video_core/host_shaders/%.frag video_core/host_shaders/source_shader.h.in
	glslangValidator -V --quiet --variable-name $(shell echo $(shell basename -s _spv.h $@) | tr '[:lower:]' '[:upper:]')_SPV -o $@ $<
video_core/host_shaders/%_vert_spv.h: video_core/host_shaders/%.vert video_core/host_shaders/source_shader.h.in
	glslangValidator -V --quiet --variable-name $(shell echo $(shell basename -s _spv.h $@) | tr '[:lower:]' '[:upper:]')_SPV -o $@ $<

.PHONY: missing
missing: missing_syms.txt
	@cat missing_syms.txt
missing_syms.txt: ld_err.txt
	@grep 'undefined reference to `' ld_err.txt | cut -d '`' -f 2 | cut -d "'" -f 1 | sort | uniq > missing_syms.txt
ld_err.txt: $(objects)
	@$(CXX) $(LDFLAGS) -o /dev/null $^ $(LDLIBS) 2> ld_err.txt || true

.PHONY: clean
clean:
	rm -f horizon-services
	find . -name '*.o' -exec rm {} \;
	find . -name '*.moc.cpp' -exec rm {} \;
	find video_core/host_shaders -name '*_comp.h' -exec rm {} \;
	find video_core/host_shaders -name '*_frag.h' -exec rm {} \;
	find video_core/host_shaders -name '*_vert.h' -exec rm {} \;
	find video_core/host_shaders -name '*_spv.h' -exec rm {} \;
