# Prevent make from loading all those unwanted builtins
.SUFFIXES:

# Allow linking C++ code
%: %.o
	@+mkdir -p $(@D)
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
