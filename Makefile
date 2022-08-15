CXX := g++
CC := g++
INCLUDES := -I . -I ./glad/include -I ./externals/fmt/include -I ./externals/cubeb/include -I ./externals/cubeb/build/exports -I ./externals/soundtouch/include -I ./externals/microprofile -I ./externals/Vulkan-Headers/include -I ./externals/SDL/include -I ./externals/sirit/externals/SPIRV-Headers/include -I ./externals/sirit/include -I ./externals/mbedtls/include $(shell bash -c 'PATH=/usr/lib/qt5/bin/:/usr/lib64/qt5/bin/:$$PATH qmake -o - <(echo "QT += gui-private") 2> /dev/null | grep INCPATH | cut -d"=" -f2')
DEBUG-y := -O0 -ggdb -D_DEBUG -fsanitize=address -static-libasan
CXXFLAGS := -DHAS_OPENGL -DFMT_HEADER_ONLY -DHAVE_SDL2 -DMIZU_UNIX -fno-new-ttp-matching -std=gnu++2a $(shell pkg-config --cflags Qt5Gui Qt5Widgets libusb-1.0 glfw3 libavutil libavcodec libswscale liblz4 opus) $(INCLUDES) $(DEBUG-$(DEBUG))
CFLAGS := $(CXXFLAGS)
LDFLAGS := -L ./externals/sirit/build/src -L ./externals/soundtouch/build -L ./externals/cubeb/build -L ./externals/mbedtls/library -L ./externals/SDL/build $(DEBUG-$(DEBUG))
LDLIBS := -pthread -lrt -ldl -lmbedcrypto -lsirit -lcubeb -lSoundTouch -lSDL2 $(shell pkg-config --libs Qt5Gui Qt5Widgets libusb-1.0 glfw3 libavutil libavcodec libswscale liblz4 opus)

services := sm set apm am acc bcat glue hid ns filesystem nvflinger vi nvdrv time lm aoc pctl audio ptm friend nifm sockets ssl

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
	   core/frontend core/loader core/frontend/applets core/hle core/hle/kernel core/network \
	   $(shell find video_core -type d) $(shell find audio_core -type d) $(shell find shader_recompiler -type d) \
	   input_common $(shell find input_common -type d)  \
	   core/hle/service $(shell find $(addprefix core/hle/service/,$(services)) -type d)
sources := $(wildcard *.cpp) $(wildcard $(addsuffix /*.cpp,$(subdirs))) video_core/bootmanager.moc.cpp
headers := $(wildcard $(patsubst %.cpp,%.h,$(sources))) $(addprefix video_core/host_shaders/,$(shader_headers)) \
	   glad/include/glad/glad.h horizon_servctl.h
objects := $(patsubst %.cpp,%.o,$(sources)) glad/src/glad.o
externals = externals/cubeb/build/CMakeFiles externals/soundtouch/build/CMakeFiles \
	    externals/sirit/build/CMakeFiles externals/SDL/build/CMakeFiles \
	    externals/mbedtls/library/libmbedtls.a

.PHONY: default
default: mizu hlaunch

mizu: $(objects)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

hlaunch: hlaunch.c
	gcc $(DEBUG-$(DEBUG)) -Wall -o $@ $^ -lrt

$(objects): $(externals) $(headers)

%.moc.cpp: %.h
	PATH=/usr/lib/qt5/bin/:/usr/lib64/qt5/bin/:$$PATH moc -o $@ $^

video_core/host_shaders/%_comp.h: video_core/host_shaders/%.comp video_core/host_shaders/source_shader.h.in
	cmake -P video_core/host_shaders/StringShaderHeader.cmake $< $@ $(word 2,$^)
video_core/host_shaders/%_frag.h: video_core/host_shaders/%.frag video_core/host_shaders/source_shader.h.in
	cmake -P video_core/host_shaders/StringShaderHeader.cmake $< $@ $(word 2,$^)
video_core/host_shaders/%_vert.h: video_core/host_shaders/%.vert video_core/host_shaders/source_shader.h.in
	cmake -P video_core/host_shaders/StringShaderHeader.cmake $< $@ $(word 2,$^)
video_core/host_shaders/%_comp_spv.h: video_core/host_shaders/%.comp
	glslangValidator -V --variable-name $(shell echo $(shell basename -s _spv.h $@) | tr '[:lower:]' '[:upper:]')_SPV -o $@ $<
	sed -i '1 i\#include <cstdint>' $@
video_core/host_shaders/%_frag_spv.h: video_core/host_shaders/%.frag video_core/host_shaders/source_shader.h.in
	glslangValidator -V --variable-name $(shell echo $(shell basename -s _spv.h $@) | tr '[:lower:]' '[:upper:]')_SPV -o $@ $<
	sed -i '1 i\#include <cstdint>' $@
video_core/host_shaders/%_vert_spv.h: video_core/host_shaders/%.vert video_core/host_shaders/source_shader.h.in
	glslangValidator -V --variable-name $(shell echo $(shell basename -s _spv.h $@) | tr '[:lower:]' '[:upper:]')_SPV -o $@ $<
	sed -i '1 i\#include <cstdint>' $@

externals/%/build/CMakeFiles: $(filter %/CMakeFiles, $(externals)) # hack to ensure cmake doesn't run concurrently (seems to cause problems)
	mkdir -p $(dir $@)
	cd $(dir $@) ; cmake .. || ( rm -rf $(dir $@) ; false )
	make -C $(dir $@) -j$(shell nproc) || ( rm -rf $(dir $@) ; false )

externals/mbedtls/library/libmbedtls.a:
	make -C $(dir $@) -j$(shell nproc)

.PHONY: install
install: mizu hlaunch
	cp mizu hlaunch /usr/bin
	cp mizu.service /usr/lib/systemd/user
	mkdir -p /etc/sysconfig
	touch /etc/sysconfig/mizu
	chmod 666 /etc/sysconfig/mizu
	echo 'enable mizu.service' > /usr/lib/systemd/user-preset/90-mizu.preset
	systemctl --global enable mizu

.PHONY: uninstall
uninstall:
	systemctl --global disable mizu
	rm -f /usr/lib/systemd/user-preset/90-mizu.preset
	rm -f /etc/sysconfig/mizu
	rm -f /etc/systemd/user/mizu.service
	rm -f /usr/bin/hlaunch
	rm -f /dev/mqueue/mizu_loader

.PHONY: clean
clean:
	rm -f mizu hlaunch
	find . -name '*.o' -not -path "./externals/*" -exec rm {} \;
	find . -name '*.moc.cpp' -not -path "./externals/*" -exec rm {} \;
	find video_core/host_shaders -name '*_comp.h' -exec rm {} \;
	find video_core/host_shaders -name '*_frag.h' -exec rm {} \;
	find video_core/host_shaders -name '*_vert.h' -exec rm {} \;
	find video_core/host_shaders -name '*_spv.h' -exec rm {} \;

.PHONY: distclean
distclean: clean
	rm -rf externals/cubeb/build externals/soundtouch/build externals/sirit/build externals/SDL/build
	make -C externals/mbedtls/library clean
