CC  = gcc
PROTOBUF = ../../plib/outerlib/protobuf-2.6.1/
CXX := g++ -g -O0 -Wall -fPIC -D_GNU_SOURCE -D_THREAD_SAFE -D ENABLE_LOG -DLinux  -DTIXML_USE_TICPP -std=c++11 -I ./src  -I ./include 

SRC_ENET_DIR=./src/

RTCHECKS := -fmudflap -fstack-check -gnato

INC  = -I ./src -I ./include 
LINK += -lpthread -lrt

OBJS = $(patsubst %.c, %.o,  $(wildcard $(SRC_ENET_DIR)*.c))
OBJS += $(patsubst %.cpp, %.o,  $(wildcard $(SRC_ENET_DIR)*.cpp))



MAIN = ./enet

all: $(MAIN)


$(MAIN): $(OBJS)
	@echo -e  $(OBJS)
	ls
	$(CXX) $(CFLAGS) -o $@ $^  $(LINK)

%.o: $(SRC_ENET_DIR)%.c
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

%.o: $(SRC_ENET_DIR)%.cpp
	$(CXX) $(CFLAGS) $(INC) -c -o $@ $<     
	
clean:
	rm -rf $(MAIN) $(OBJS)

