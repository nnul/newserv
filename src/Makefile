OBJECTS=FileContentsCache.o Menu.o PSOProtocol.o Client.o \
	Lobby.o ServerState.o Server.o License.o \
	PSOEncryption.o Player.o SendCommands.o ChatCommands.o \
	ReceiveSubcommands.o ReceiveCommands.o Version.o Items.o \
	LevelTable.o Compression.o Quest.o RareItemSet.o Map.o NetworkAddresses.o \
	Text.o DNSServer.o ProxyServer.o Shell.o ServerShell.o ProxyShell.o Main.o
PHOSG_OBJECTS=phosg/Concurrency.o phosg/ConsistentHashRing.o phosg/FileCache.o \
	phosg/Filesystem.o phosg/Hash.o phosg/Image.o phosg/JSONPickle.o \
	phosg/JSON.o phosg/Network.o phosg/Process.o phosg/Random.o \
	phosg/Strings.o phosg/Time.o phosg/UnitTest.o
CXX=g++ -fPIC
CXXFLAGS=-I/opt/local/include -I/usr/local/include -I./ -g -std=c++14 -DLINUX -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H -Wall -Werror
LDFLAGS=-L/opt/local/lib -L/usr/local/lib -std=c++14 -lstdc++ -levent -levent_pthreads -lpthread
NEWSERV=newserv

all: build

$(NEWSERV): $(OBJECTS) $(PHOSG_OBJECTS)
	$(CXX) $(PHOSG_OBJECTS) $(OBJECTS) $(LDFLAGS) -o $(NEWSERV)

.PHONY: build
build: $(NEWSERV)
