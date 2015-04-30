#CXX=g++-4.9
CXX=g++
CC=gcc
BIN=./bin

CXXFLAGS= -O3 -g -pipe  \
  -lpthread -std=c++11 -pthread -DHAVE_PCAP -lgtest -lgtest_main  -lzmq -lglog  -lz

# path to SwiftSnails src
SwiftSnails_src=/Users/apple/project/SwiftSnails/src

# path to SwiftSnails-third/local
LOCAL_ROOT=/Users/apple/project/SwiftSnails/third/local

GTEST_HOME=$(LOCAL_ROOT)/gtest

THIRD_INCPATH=-I$(LOCAL_ROOT)/glog/include \
	-I$(LOCAL_ROOT) \
	-I$(LOCAL_ROOT)/zeromq/include \
	-I$(LOCAL_ROOT)/zlib/include \
	-I/usr/local/Cellar/google-sparsehash/2.0.2/include \
	-I/usr/include/pthread \
	-I$(SwiftSnails_src) \
	-I$(GTEST_HOME)/include

THIRD_LIB=-L$(LOCAL_ROOT)/glog/lib/  -L$(LOCAL_ROOT)/zeromq/lib/ -L$(LOCAL_ROOT)/zlib/lib/

.PHONY:all clean test cur

ALL: master worker server lworker

master: master.cpp
	$(CXX) master.cpp $(THIRD_INCPATH) -L$(GTEST_HOME)/lib/.libs -Xlinker $(THIRD_LIB)  $(CXXFLAGS) -o $(BIN)/swift_master

worker: worker.cpp
	$(CXX) worker.cpp $(THIRD_INCPATH) -L$(GTEST_HOME)/lib/.libs -Xlinker $(THIRD_LIB)  $(CXXFLAGS) -o $(BIN)/swift_worker

lworker: lworker.cpp
	$(CXX) lworker.cpp $(THIRD_INCPATH) -L$(GTEST_HOME)/lib/.libs -Xlinker $(THIRD_LIB)  $(CXXFLAGS) -o $(BIN)/swift_lworker

server: server.cpp
	$(CXX) server.cpp $(THIRD_INCPATH) -L$(GTEST_HOME)/lib/.libs -Xlinker $(THIRD_LIB)  $(CXXFLAGS) -o $(BIN)/swift_server


clean:
	rm -f $(BIN)/*
