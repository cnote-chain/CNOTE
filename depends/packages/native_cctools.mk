package=native_cctools
$(package)_version=807d6fd1be5d2224872e381870c0a75387fe05e6
$(package)_download_path=https://github.com/theuni/cctools-port/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=a09c9ba4684670a0375e42d9d67e7f12c1f62581a27f28f7c825d6d7032ccc6a
$(package)_build_subdir=cctools
$(package)_clang_version=3.7.1
$(package)_clang_download_path=https://llvm.org/releases/$($(package)_clang_version)
$(package)_clang_download_file=clang+llvm-$($(package)_clang_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_clang_file_name=clang-llvm-$($(package)_clang_version)-x86_64-linux-gnu-ubuntu-14.04.tar.xz
$(package)_clang_sha256_hash=99b28a6b48e793705228a390471991386daa33a9717cd9ca007fcdde69608fd9
$(package)_extra_sources=$($(package)_clang_file_name)
$(package)_patches=sysctl.h

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_clang_download_path),$($(package)_clang_download_file),$($(package)_clang_file_name),$($(package)_clang_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_clang_sha256_hash)  $($(package)_source_dir)/$($(package)_clang_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir -p toolchain/bin toolchain/lib/clang/3.5/include && \
  tar --no-same-owner --strip-components=1 -C toolchain -xf $($(package)_source_dir)/$($(package)_clang_file_name) && \
  rm -f toolchain/lib/libc++abi.so* && \
  echo "#!/bin/sh" > toolchain/bin/$(host)-dsymutil && \
  echo "exit 0" >> toolchain/bin/$(host)-dsymutil && \
  chmod +x toolchain/bin/$(host)-dsymutil && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source)
endef

define $(package)_set_vars
$(package)_config_opts=--target=$(host) --disable-lto-support
$(package)_ldflags+=-Wl,-rpath=\\$$$$$$$$\$$$$$$$$ORIGIN/../lib
# Build the cctools host tools with the system clang. The bundled clang 3.7.1
# is too old for this host's libstdc++ headers (it lacks the __is_assignable
# builtin they use). The bundled clang is still copied into the staged
# toolchain by stage_cmds, so the actual macOS cross-compiler is unchanged.
$(package)_cc=clang
$(package)_cxx=clang++
# clang 11+ / gcc 10+ default to -fno-common, which turns the tentative
# definitions cctools shares via headers (e.g. dis_info) into "multiple
# definition" link errors. Restore the legacy -fcommon behaviour.
$(package)_cflags+=-fcommon
$(package)_cxxflags+=-fcommon
endef

define $(package)_preprocess_cmds
  cd $($(package)_build_subdir); ./autogen.sh && \
  cp -f $($(package)_patch_dir)/sysctl.h include/foreign/sys/sysctl.h && \
  sed -i.old "/define HAVE_PTHREADS/d" ld64/src/ld/InputFiles.h && \
  cd .. && \
  python3 -c "p='cctools/ld64/src/ld/ld.hpp'; s=open(p).read(); o1='binding(Fixup::bindingByNameUnbound),  \n\t\tcontentAddendOnly(false), contentDetlaToAddendOnly(false), contentIgnoresAddend(false) \n\t\t\t{ assert(name != NULL); u.name = name; }'; n1='binding(name != NULL ? Fixup::bindingByNameUnbound : Fixup::bindingNone),  \n\t\tcontentAddendOnly(false), contentDetlaToAddendOnly(false), contentIgnoresAddend(false) \n\t\t\t{ if (name != NULL) u.name = name; else u.addend = 0; }'; s=s.replace(o1,n1); o2='binding(b),  \n\t\tcontentAddendOnly(false), contentDetlaToAddendOnly(false), contentIgnoresAddend(false) \n\t\t\t{ assert(name != NULL); u.name = name; }'; n2='binding(name != NULL ? b : Fixup::bindingNone),  \n\t\tcontentAddendOnly(false), contentDetlaToAddendOnly(false), contentIgnoresAddend(false) \n\t\t\t{ if (name != NULL) u.name = name; else u.addend = 0; }'; open(p,'w').write(s.replace(o2,n2))" && \
  python3 -c "p='cctools/ld64/src/ld/Resolver.cpp'; s=open(p).read(); o='\t\t\t\t\tdefault:\n\t\t\t\t\t\tassert(0 && \"bad binding during dead stripping\");'; n='\t\t\t\t\tcase ld::Fixup::bindingNone:\n\t\t\t\t\t\tbreak;\n\t\t\t\t\tdefault:\n\t\t\t\t\t\tassert(0 && \"bad binding during dead stripping\");'; open(p,'w').write(s.replace(o,n,1))" && \
  python3 -c "p='cctools/ld64/src/ld/OutputFile.cpp'; s=open(p).read(); o='\t\t\t\t\t}\n\t\t\t\t\tassert(target != NULL);\n\t\t\t\t}'; n='\t\t\t\t\t}\n\t\t\t\t\tif (fit->binding != ld::Fixup::bindingNone) assert(target != NULL);\n\t\t\t\t}'; s=s.replace(o,n,1); s=s.replace('case ld::Fixup::bindingNone:\n\t\t\tthrow \"unexpected bindingNone\";', 'case ld::Fixup::bindingNone:\n\t\t\treturn 0;'); open(p,'w').write(s)" && \
  sed -i 's|throw "unexpected bindingNone";|return;|' cctools/ld64/src/ld/passes/branch_shim.cpp && \
  python3 -c "p='toolchain/include/c++/v1/string'; s=open(p).read(); o='basic_string<_CharT, _Traits, _Allocator>::basic_string(const allocator_type& __a)\n    : __r_(__a)'; n='basic_string<_CharT, _Traits, _Allocator>::basic_string(const allocator_type& __a)\n#if _LIBCPP_STD_VER <= 14\n        _NOEXCEPT_(is_nothrow_copy_constructible<allocator_type>::value)\n#else\n        _NOEXCEPT\n#endif\n    : __r_(__a)'; open(p,'w').write(s.replace(o,n))" && \
  python3 -c "p='toolchain/include/c++/v1/__hash_table'; s=open(p).read(); o='is_nothrow_default_constructible<__first_node>::value &&\n        is_nothrow_default_constructible<hasher>::value'; n='is_nothrow_default_constructible<__first_node>::value &&\n        is_nothrow_default_constructible<__node_allocator>::value &&\n        is_nothrow_default_constructible<hasher>::value'; open(p,'w').write(s.replace(o,n))" && \
  python3 -c "p='toolchain/include/c++/v1/__hash_table'; s=open(p).read(); o='is_nothrow_move_constructible<__first_node>::value &&\n            is_nothrow_move_constructible<hasher>::value'; n='is_nothrow_move_constructible<__first_node>::value &&\n            is_nothrow_move_constructible<__node_allocator>::value &&\n            is_nothrow_move_constructible<hasher>::value'; open(p,'w').write(s.replace(o,n))" && \
  python3 -c "p='cctools/ld64/src/ld/OutputFile.cpp'; s=open(p).read(); o='->isThumb();\n\t\tdefault:\n\t\t\tbreak;\n\t}\n\tthrow \"unexpected binding\";'; n='->isThumb();\n\t\tdefault:\n\t\t\tbreak;\n\t}\n\treturn false;'; open(p,'w').write(s.replace(o,n,1))"
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install && \
  cd $($(package)_extract_dir)/toolchain && \
  mkdir -p $($(package)_staging_prefix_dir)/lib/clang/$($(package)_clang_version)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/bin $($(package)_staging_prefix_dir)/include && \
  printf '#!/bin/sh\nexec /usr/bin/clang -fuse-ld=$(host_prefix)/native/bin/$(host)-ld "\044@"\n' > $($(package)_staging_prefix_dir)/bin/clang && \
  printf '#!/bin/sh\nexec /usr/bin/clang++ -fuse-ld=$(host_prefix)/native/bin/$(host)-ld "\044@"\n' > $($(package)_staging_prefix_dir)/bin/clang++ && \
  chmod +x $($(package)_staging_prefix_dir)/bin/clang $($(package)_staging_prefix_dir)/bin/clang++ && \
  cp lib/libLTO.so $($(package)_staging_prefix_dir)/lib/ && \
  cp -rf lib/clang/$($(package)_clang_version)/include/* $($(package)_staging_prefix_dir)/lib/clang/$($(package)_clang_version)/include/ && \
  cp bin/llvm-dsymutil $($(package)_staging_prefix_dir)/bin/$(host)-dsymutil && \
  if `test -d include/c++/`; then cp -rf include/c++/ $($(package)_staging_prefix_dir)/include/; fi && \
  if `test -d lib/c++/`; then cp -rf lib/c++/ $($(package)_staging_prefix_dir)/lib/; fi
endef
