INCLUDES=	-I/usr/local/opencv/include

CC=	g++

CFLAGS=	-O0	-g	-std=c++11	

CPPLIBS= -L/usr/lib -lopencv_core -lopencv_flann -lopencv_video -lpthread -lrt 
 
CPPFILES= TL_Image_Acqusition.cpp 

CPPOBJS= ${CPPFILES:.cpp=.o}

build:	TL_Image_Acqusition 

clean:
	-rm -f *.o *.d *.jpg *.ppm
	-rm -f TL_Image_Acqusition

TL_Image_Acqusition:	TL_Image_Acqusition.o	
	$(CC)	$(LDFLAGS)	$(INCLUDES)	$(CFLAGS)	-o	TL_Image_Acqusition	TL_Image_Acqusition.o	`pkg-config --libs opencv`	$(CPPLIBS)
