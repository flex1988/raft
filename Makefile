all: raft_server

raft_server:
	protoc ./proto/raft.proto --cpp_out=./

clean:
	rm -f ./proto/*.cc
	rm -f ./proto/*.h
