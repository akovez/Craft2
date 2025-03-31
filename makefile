# CC=tcc #	tcc does not seem to work- lacking immintrin support
CC=gcc
SOURCEDIR=src
DEPENDENCIES=deps/*
#export PATH :=/usr/include:$(PATH)
FLAGS = -mavx2 -gdwarf-2 -lglfw -lGLU -lGL -lXrandr -lXxf86vm -lXi -lXinerama -lX11 -lrt -ldl -lm -lGLEW -lEGL -lGL -lGLU -lOpenGL -lcurl
FILES += $(wildcard $(SOURCEDIR)/*.h)
FILES += $(wildcard $(SOURCEDIR)/*.c)
FILES += $(wildcard $(DEPENDENCIES)*/*.h)
FILES += $(wildcard $(DEPENDENCIES)*/*.c)

craft2: $(FILES)
	$(info  FILES is $(FILES))
	$(info 	PATH is $(PATH))
	$(CC) $(FLAGS) $(FILES)  -o craft2
