CC = dpu-upmem-dpurte-clang
CXX = g++ 
INCLUDE_PATH = -I/usr/include/dpu -I./include
DINCLUDE_PATH = -I/usr/share/upmem/include
CXXFLAGS = --std=c++17 $(INCLUDE_PATH) -ldpu -g
TASKLETS = -DNR_TASKLETS=3 -DSTACK_SIZE_DEFAULT=256 -DSTACK_SIZE_TASKLET_1=2048
BDIR = ./build


TARGETS = dpuprog hostprog
DPU_SOURCES = dpup.c
HOST_SCOURCES = hostp.cc

all: $(TARGETS)


dpuprog: $(DPU_SOURCES)
	$(CC) $(DPU_SOURCES) -O2 -g -o $(BDIR)/dpuprog $(DINCLUDE_PATH)

hostprog: $(HOST_SCOURCES)
	$(CXX) $(HOST_SCOURCES) -o $(BDIR)/hostprog  $(CXXFLAGS)

clean:
	rm -rf $(BDIR)/* 
