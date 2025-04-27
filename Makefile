CC	= g++
TARGET	= MbsBackend
SRC	= MbsBackend.cpp Util.cpp
OBJ 	= ${SRC:.cpp=.o}

all: $(TARGET)

${TARGET}: $(OBJ)
	$(CC) ${CPPFLAGS} ${CFLAGS} -o $@ $(OBJ)

clean:
	rm -f $(TARGET) $(OBJ)
