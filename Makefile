CC	= g++
TARGET	= MbsBackend
SRC	= MbsBackend.cpp Util.cpp
OBJ 	= ${SRC:.cpp=.o}

all: $(TARGET)

${TARGET}: $(OBJ)
	$(CC) ${CPPFLAGS} ${CFLAGS} -o $@ -lsqlite3 $(OBJ)

clean:
	rm -f $(TARGET) $(OBJ)
