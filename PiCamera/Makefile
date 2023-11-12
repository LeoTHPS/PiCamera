RM              ?= rm -f
CXX             ?= g++

CXXFLAGS        += -Wall -Wfatal-errors -std=c++20 -I. -I"$(AL_INCLUDE)"

SOURCE_FILES     = main.cpp pi_camera.cpp
OBJECT_FILES     = $(SOURCE_FILES:.cpp=.o)

SOURCE_FILES_API = pi_camera.cpp
OBJECT_FILES_API = $(SOURCE_FILES_API:.cpp=.o)

ifdef COMPILER
	ifeq ($(COMPILER), GNU)
		CXX = g++
	else ifeq ($(COMPILER), CLANG)
		CXX = clang++
	else
		CXX = $(COMPILER)
	endif
endif

ifdef PLATFORM
	ifeq ($(PLATFORM), LINUX)
		LDLIBS += -ldl -lpthread

		API_FILE_EXTENSION = .so
	endif

	ifeq ($(PLATFORM), WINDOWS)
		LDLIBS += -luser32 -lntdll -lkernel32 -lAdvapi32
		LDLIBS += -lwsock32 -lws2_32 -lIphlpapi
		LDLIBS += -lshlwapi

		API_FILE_EXTENSION = .dll
	endif
endif

.PHONY: clean

PiCamera: $(OBJECT_FILES)
	$(CXX) $(CPPFLAGS) -DPI_CAMERA_BIN $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

PiCamera.API: $(OBJECT_FILES_API)
	$(CXX) $(CPPFLAGS) -DPI_CAMERA_API $(CXXFLAGS) -fPIC -shared $^ -o $@$(API_FILE_EXTENSION) $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(OBJECT_FILES)
	$(RM) $(OBJECT_FILES_API)
