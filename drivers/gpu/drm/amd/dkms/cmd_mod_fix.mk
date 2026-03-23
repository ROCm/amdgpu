# Fix E2BIG/ENAMETOOLONG for amdgpu on kernels < 5.19: use $(file) instead
# of shell echo in cmd_mod, and write one .o per line in .mod files.
#
# $(subst $(space),$(newline),...) inside $(file) silently drops newlines,
# so we use $(file >) for the first entry and $(file >>) for the rest.

ifndef _amdgpu_cmd_mod_fix_included
_amdgpu_cmd_mod_fix_included := 1

_kbuild_makefile_build := $(srctree)/scripts/Makefile.build
_cmd_mod_needs_fix := $(shell grep -q 'cmd_mod = printf' $(_kbuild_makefile_build) 2>/dev/null || echo 1)

ifeq ($(_cmd_mod_needs_fix),1)

_amdgpu_mod_objs = $(sort $(strip $(if $($*-objs)$($*-y)$($*-m),$(addprefix $(obj)/,$($*-objs) $($*-y) $($*-m)),$(@:.mod=.o))))

override quiet_cmd_mod = MOD     $@
override cmd_mod = $(file >$@,$(firstword $(_amdgpu_mod_objs))) \
                   $(foreach o,$(wordlist 2,$(words $(_amdgpu_mod_objs)),$(_amdgpu_mod_objs)),$(file >>$@,$(o))) \
                   true $(if $(cmd_undef_syms),; $(cmd_undef_syms))

override quiet_cmd_link_multi-m = LD [M]  $@
override cmd_link_multi-m = $(file >$@.args, $(filter %.o,$^)) \
        $(LD) $(ld_flags) -r -o $@ @$@.args

endif
endif
