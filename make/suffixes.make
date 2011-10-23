# Prevent make from loading all those unwanted builtins
.SUFFIXES:

# Allow linking C++ code
tmwa-%: obj/%/main.o
	$(CXX) $(LDFLAGS) $(TARGET_ARCH) $^ $(LDLIBS) -o $@
%.cpp:: %.lpp
	$(LEX) -o $@ $<
%.cpp %.hpp:: %.ypp
	$(BISON) -d -o $*.cpp $<

obj/%.o: src/%.cpp warnings
	@+mkdir -p $(@D)
	$(COMPILE.cpp) -o $@ $<
%.hpp.gch: %.hpp
	$(COMPILE.cpp) -o $@ $<
