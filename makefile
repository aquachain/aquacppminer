source_files += $(wildcard src/*.cpp)
source_files += $(wildcard src/*.h)



bin/aquacppminer: $(source_files)
	$(MAKE) -C prj/ config=rel_x64 aquacppminer

.PHONY += all
all: bin/aquacppminer bin/aquacppminer_avx bin/aquacppminer_avx2

bin/aquacppminer_avx: $(source_files)
	$(MAKE) -C prj/ config=relavx_x64 aquacppminer

bin/aquacppminer_avx2: $(source_files)
	$(MAKE) -C prj/ config=relavx2_x64 aquacppminer

clean:
	$(MAKE) -C prj config=rel_x64 clean
	$(MAKE) -C prj config=relavx_x64 clean
	$(MAKE) -C prj config=relavx2_x64 clean

.PHONY += clean
