TARGET	= MbsBackend
SRC 	= $(wildcard *.cpp)
OBJ 	= $(SRC:.cpp=.o)

CFLAGS += -pipe
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -march=native -Ofast

all: $(TARGET)

# Link objects into target binary
$(TARGET): $(OBJ)
	$(CXX) -o $(TARGET) $(CFLAGS) $(OBJ) -lsqlite3

# Generate Objects
%.o : %.cpp 
	$(CXX) $< $(LIB) $(CFLAGS) -c -o $@ 

clean:
	rm -f $(TARGET) $(OBJ)

hard-clean:
	rm -f $(TARGET) $(OBJ) movie_ticket_system.db
