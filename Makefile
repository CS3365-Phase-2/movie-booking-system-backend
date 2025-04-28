CC	= g++
TARGET	= MbsBackend
SRC	= MbsBackend.cpp Util.cpp
OBJ 	= ${SRC:.cpp=.o}

all: $(TARGET)

${TARGET}: $(OBJ)
	$(CC) -c $(SRC)
        $(CC) -o $(TARGET) $(SRC) -lsqlite3

clean:
	rm -f $(TARGET) $(OBJ)
