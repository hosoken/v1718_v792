CXX        = g++
RM         = rm -f
CXXFLAGS   = -O2 -Wall

RTINCLUDES = $(shell root-config --cflags)
RTLIBS     = $(shell root-config --glibs)

FLAGS      = $(CXXFLAGS) $(RTINCLUDES) -DLINUX
LIBS       = $(RTLIBS) -lCAENVME

v792: v792.o
	@echo Linking $@ ...
	@$(CXX) -o $@ $^ $(LIBS)

clean:
	@echo Cleaning up ...
	@$(RM) *.o *~
	@$(RM) v792

%.o: %.cc
	@echo Compiling $< ...
	@$(CXX) $(FLAGS) -c $< -o $@
