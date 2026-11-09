# Offline tests. Works with g++ or clang++ on Linux (the Mac build is in
# scripts/build_vst_mac.sh). Everything is compiled as C++98 to prove it builds
# on old toolchains.
#   make run        DSP test suite, writes demo WAVs to ./renders
#   make vst-run    builds the plugin as a shared library and loads it in a mini VST host

CXX      ?= c++
CXXFLAGS ?= -O2 -std=c++98 -Wall -Wextra -pedantic -Wno-long-long
DSP_SRC   = ../Source/DSP/Netlist.cpp ../Source/DSP/SpringTank.cpp ../Source/DSP/MusicianAmp.cpp
HEADERS   = ../Source/DSP/*.h ../Source/VST2/*.h

musician_test: $(DSP_SRC) musician_test.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(DSP_SRC) musician_test.cpp -o $@

TBVSMusician.so: $(DSP_SRC) ../Source/VST2/VSMusicianVST.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -fPIC -fvisibility=hidden -shared ../Source/VST2/VSMusicianVST.cpp $(DSP_SRC) -o $@

vst_host_test: vst_host_test.cpp $(DSP_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) vst_host_test.cpp $(DSP_SRC) -ldl -o $@

run: musician_test
	mkdir -p renders
	./musician_test renders

vst-run: TBVSMusician.so vst_host_test
	./vst_host_test ./TBVSMusician.so

clean:
	rm -rf musician_test vst_host_test TBVSMusician.so renders

.PHONY: run vst-run clean
