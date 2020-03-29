OBJECTS=FileContentsCache.o Menu.o PSOProtocol.o Client.o \
	Lobby.o ServerState.o Server.o License.o \
	PSOEncryption.o Player.o SendCommands.o ChatCommands.o \
	ReceiveSubcommands.o ReceiveCommands.o Version.o Items.o \
	LevelTable.o Compression.o Quest.o RareItemSet.o Map.o NetworkAddresses.o \
	Text.o DNSServer.o ProxyServer.o Shell.o ServerShell.o ProxyShell.o Main.o
CXX=g++
CXXFLAGS=-I/opt/local/include -I/usr/local/include -Ilibphosg/include -std=c++14 -g -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H -Wall -Werror
LDFLAGS=-L/opt/local/lib -L/usr/local/lib -Llibphosg/lib -std=c++14 -levent -levent_pthreads -lphosg -lpthread
NEWSERV=newserv

all: build

$(NEWSERV): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $(NEWSERV)

.PHONY: build
build: $(NEWSERV)
