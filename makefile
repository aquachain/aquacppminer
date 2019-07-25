source_files += $(wildcard src/*.cpp)
source_files += $(wildcard src/*.h)
projectdir := prj

.PHONY += release
.PHONY += all

release: bin/aquacppminer bin/aquacppminer_avx bin/aquacppminer_avx2
all: release debug

bin/aquacppminer: $(projectdir) $(source_files)
	$(MAKE) -C $(projectdir) config=rel_x64 aquacppminer


bin/aquacppminer_avx: $(projectdir) $(source_files)
	$(MAKE) -C $(projectdir) config=relavx_x64 aquacppminer

bin/aquacppminer_avx2: $(projectdir) $(source_files)
	$(MAKE) -C $(projectdir) config=relavx2_x64 aquacppminer

bin/aquacppminer_d: $(projectdir) $(source_files)
	$(MAKE) -C $(projectdir) aquacppminer


debug: bin/aquacppminer_d
.PHONY += debug
clean:
	$(MAKE) -C prj config=rel_x64 clean
	$(MAKE) -C prj config=relavx_x64 clean
	$(MAKE) -C prj config=relavx2_x64 clean

.PHONY += clean

$(projectdir):
	bash ./scripts/setup_linux.sh
