#
# Makefile
#
# BarePD - Bare metal Pure Data for Raspberry Pi
#

# Circle configuration
CIRCLEHOME = ../circle

# libpd configuration
LIBPD_HOME = ../libpd
PD_HOME = $(LIBPD_HOME)/pure-data

# Application name
PROG = barepd

# libpd wrapper files
LIBPD_WRAPPER_OBJS = \
	libpd_z_libpd.o \
	libpd_z_hooks.o \
	libpd_s_libpdmidi.o \
	libpd_x_libpdreceive.o

# Pure Data core - minimal set for audio synthesis
PD_CORE_OBJS = \
	pd_d_arithmetic.o \
	pd_d_array.o \
	pd_d_ctl.o \
	pd_d_dac.o \
	pd_d_delay.o \
	pd_d_fft.o \
	pd_d_fft_fftsg.o \
	pd_d_filter.o \
	pd_d_global.o \
	pd_d_math.o \
	pd_d_misc.o \
	pd_d_osc.o \
	pd_d_resample.o \
	pd_d_soundfile.o \
	pd_d_soundfile_aiff.o \
	pd_d_soundfile_caf.o \
	pd_d_soundfile_next.o \
	pd_d_soundfile_wave.o \
	pd_d_ugen.o \
	pd_g_all_guis.o \
	pd_g_array.o \
	pd_g_bang.o \
	pd_g_canvas.o \
	pd_g_clone.o \
	pd_g_editor.o \
	pd_g_editor_extras.o \
	pd_g_graph.o \
	pd_g_guiconnect.o \
	pd_g_io.o \
	pd_g_mycanvas.o \
	pd_g_numbox.o \
	pd_g_radio.o \
	pd_g_readwrite.o \
	pd_g_rtext.o \
	pd_g_scalar.o \
	pd_g_slider.o \
	pd_g_template.o \
	pd_g_text.o \
	pd_g_toggle.o \
	pd_g_traversal.o \
	pd_g_undo.o \
	pd_g_vumeter.o \
	pd_m_atom.o \
	pd_m_binbuf.o \
	pd_m_class.o \
	pd_m_conf.o \
	pd_m_glob.o \
	pd_m_memory.o \
	pd_m_obj.o \
	pd_m_pd.o \
	pd_m_sched.o \
	pd_s_audio.o \
	pd_s_audio_dummy.o \
	pd_s_inter.o \
	pd_s_inter_gui.o \
	pd_s_loader.o \
	pd_s_main.o \
	pd_s_path.o \
	pd_s_print.o \
	pd_s_utf8.o \
	pd_x_acoustics.o \
	pd_x_arithmetic.o \
	pd_x_array.o \
	pd_x_connective.o \
	pd_x_gui.o \
	pd_x_interface.o \
	pd_x_list.o \
	pd_x_midi.o \
	pd_x_misc.o \
	pd_x_scalar.o \
	pd_x_text.o \
	pd_x_time.o \
	pd_x_vexp.o \
	pd_x_vexp_if.o \
	pd_x_vexp_fun.o

# All object files - OBJS is used by Circle's Rules.mk
OBJS = main.o kernel.o pdsounddevice.o pd_compat.o pd_fileio.o \
       $(LIBPD_WRAPPER_OBJS) $(PD_CORE_OBJS)

# Include paths
INCLUDE += \
	-I$(LIBPD_HOME)/libpd_wrapper \
	-I$(PD_HOME)/src \
	-I.

# C/C++ flags for libpd
# -fno-short-enums: Force enums to be int-sized (required for variadic functions)
#                   ARM EABI defaults to short enums, but variadic args expect int
# -mthumb/-mthumb-interwork: Match newlib's Thumb compilation
CFLAGS_LIBPD = \
	-DPD \
	-DUSEAPI_DUMMY \
	-DPD_INTERNAL \
	-DLIBPD_NO_NUMERIC \
	-DLITTLE_ENDIAN=1234 \
	-DBYTE_ORDER=1234 \
	-DBAREPD \
	-fno-short-enums \
	-mthumb \
	-mthumb-interwork \
	-Wno-unused-parameter \
	-Wno-sign-compare \
	-Wno-unused-variable \
	-Wno-maybe-uninitialized \
	-Wno-unused-function \
	-fno-exceptions

# Circle libraries to link (using Circle's native FAT fs)
LIBS = $(CIRCLEHOME)/addon/SDCard/libsdcard.a \
       $(CIRCLEHOME)/lib/usb/libusb.a \
       $(CIRCLEHOME)/lib/input/libinput.a \
       $(CIRCLEHOME)/lib/fs/fat/libfatfs.a \
       $(CIRCLEHOME)/lib/fs/libfs.a \
       $(CIRCLEHOME)/lib/sound/libsound.a \
       $(CIRCLEHOME)/lib/sched/libsched.a \
       $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

# Add newlib's libc and libm for standard library support
LIBC = "$(shell $(CC) $(ARCH) -print-file-name=libc.a)"
LIBM = "$(shell $(CC) $(ARCH) -print-file-name=libm.a)"
EXTRALIBS += $(LIBC) $(LIBM)

# Custom rules for libpd wrapper files
libpd_z_libpd.o: $(LIBPD_HOME)/libpd_wrapper/z_libpd.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

libpd_z_hooks.o: $(LIBPD_HOME)/libpd_wrapper/z_hooks.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

libpd_s_libpdmidi.o: $(LIBPD_HOME)/libpd_wrapper/s_libpdmidi.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

libpd_x_libpdreceive.o: $(LIBPD_HOME)/libpd_wrapper/x_libpdreceive.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

# Custom rules for Pure Data source files
pd_d_%.o: $(PD_HOME)/src/d_%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

pd_g_%.o: $(PD_HOME)/src/g_%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

pd_m_%.o: $(PD_HOME)/src/m_%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

pd_s_%.o: $(PD_HOME)/src/s_%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

pd_x_%.o: $(PD_HOME)/src/x_%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(CFLAGS_LIBPD) $(INCLUDE) -c -o $@ $<

# Clean all
clean: clean-libpd

clean-libpd:
	rm -f $(LIBPD_WRAPPER_OBJS) $(PD_CORE_OBJS)

.PHONY: clean-libpd
