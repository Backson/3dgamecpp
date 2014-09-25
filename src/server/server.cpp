#include "game/world.hpp"
#include "io/logging.hpp"
#include "std_types.hpp"
#include "util.hpp"
#include "constants.hpp"
#include "time.hpp"
#include "net/net.hpp"
#include "net/socket.hpp"
#include "net/buffer.hpp"

#include <boost/asio.hpp>
#include <thread>
#include <future>

using namespace std;
using namespace boost;
using namespace boost::asio::ip;

using namespace my::time;
using namespace my::net;

#undef DEFAULT_LOGGER
#define DEFAULT_LOGGER NAMED_LOGGER("server")

static const uint8 MAGIC[4] = {0xaa, 0x0d, 0xbe, 0x15};

struct Client {
	bool connected;
	udp::endpoint endpoint;
	uint32 token;
	time_t timeOfLastPacket;
};

class Server {
private:
	bool closeRequested = false;
	World *world = nullptr;

	Client clients[MAX_CLIENTS];

	uint16 port = 0;
	time_t timeout = my::time::seconds(10); // 10 seconds

	Buffer inBuf;
	Buffer outBuf;

	// time keeping
	time_t time = 0;

	// our listening socket
	my::net::ios_t ios;
	my::net::Socket socket;

public:
	Server(uint16 port);
	~Server();

	void run();

private:
	void handle(const Buffer &, const endpoint_t &);
	void checkInactive();

	void handleConnectionRequest(const endpoint_t &);
	void handleClientMessage(uint8 type, uint8 id, const Buffer &);

	void disconnect(uint8 id);
};

int main(int argc, char *argv[]) {
	initLogging("logging_srv.conf");

	LOG(INFO, "Starting server");
	LOG(TRACE, "Trace enabled");

	initUtil();
	Server server(8547);

	try {
		server.run();
	} catch (std::exception &e) {
		LOG(FATAL, "Exception: " << e.what());
		return -1;
	}

	return 0;

}

Server::Server(uint16 port) :
	port(port),
	inBuf(1024*64),
	outBuf(1024*64),
	ios(),
	socket(ios)
{
	LOG(INFO, "Creating Server");
	world = new World();
	for (uint8 i = 0; i < MAX_CLIENTS; i++) {
		clients[i].connected = false;
		clients[i].timeOfLastPacket = 0;
	}
}

Server::~Server() {
	delete world;
}

void Server::run() {
	LOG(INFO, "Starting Server on port " << port);

	time = my::time::now();

	socket.open();
	socket.bind(udp::endpoint(udp::v4(), port));

	if (socket.getSystemError() == asio::error::address_in_use) {
		LOG(FATAL, "Port already in use");
		return;
	}

	// this will make sure our async_recv calls get handled
	auto w = new asio::io_service::work(ios);
	future<void> f = async(launch::async, [this]{ ios.run(); });

	int tick = 0;
	while (!closeRequested) {
		time_t remTime;
		while ((remTime = time - now() + seconds(1) / TICK_SPEED) > 0) {
			inBuf.clear();
			endpoint_t endpoint;
			switch (socket.receiveFor(&inBuf, &endpoint, remTime)) {
			case Socket::OK:
				handle(inBuf, endpoint);
				break;
			case Socket::TIMEOUT:
				break;
			case Socket::SYSTEM_ERROR:
				LOG(ERROR, "boost::asio error " << socket.getSystemError()
						<< "while receiving packets");
				break;
			default:
				LOG(ERROR, "Unknown error while receiving packets");
			}
		}

		world->tick(tick, -1);
		tick++;
		time += seconds(1) / TICK_SPEED;

		checkInactive();
	}

	socket.close();
	delete w;
	f.get();
}

void Server::handle(const Buffer &buffer, const endpoint_t &endpoint) {
	LOG(TRACE, "Received message of length " << buffer.rSize());

	MessageHeader header;
	if (buffer.rSize() < sizeof (MessageHeader)) {
		LOG(DEBUG, "Message too short for basic protocol header");
		return;
	}

	buffer >> header;

	// checking magic
	if (memcmp(header.magic, MAGIC, sizeof (header.magic)) != 0) {
		LOG(DEBUG, "Incorrect magic");
		return;
	}

	if (header.type == CONNECTION_REQUEST) {
		handleConnectionRequest(endpoint);
	} else {
		int player = -1;
		for (int id = 0; id < (int) MAX_CLIENTS; ++id) {
			if (clients[id].endpoint == endpoint) {
				player =  id;
				break;
			}
		}

		if (player >= 0 && clients[player].connected) {
			uint8 id = player;

			ClientMessageHeader message;
			if (buffer.rSize() < sizeof (ClientMessageHeader)) {
				LOG(DEBUG, "Message stopped abruptly");
				return;
			}

			buffer >> message;

			if (clients[id].token != message.token) {
				LOG(DEBUG, "Invalid token for Player " << (int) id);
				return;
			}

			clients[id].timeOfLastPacket = my::time::now();

			handleClientMessage(header.type, id, buffer);
		} else {
			LOG(DEBUG, "Unknown remote address");

			outBuf.clear();
			outBuf << MAGIC << CONNECTION_RESET;
			switch (socket.send(outBuf, endpoint)) {
			case Socket::OK:
				break;
			case Socket::SYSTEM_ERROR:
				LOG(ERROR, "boost::asio error " << socket.getSystemError()
						<< "while sending packet");
			default:
				LOG(ERROR, "Unknown error while sending packet");
			}
		}
	}
}

void Server::handleConnectionRequest(const endpoint_t &endpoint) {
	LOG(INFO, "New connection from IP "
			<< endpoint.address().to_string()
			<< " and port " << endpoint.port()
	);

	// find a free spot for the new player
	int newPlayer = -1;
	int duplicate = -1;
	for (int i = 0; i < (int) MAX_CLIENTS; i++) {
		bool connected = clients[i].connected;
		if (newPlayer == -1 && !connected) {
			newPlayer = i;
		}
		if (connected && clients[i].endpoint == endpoint) {
			duplicate = i;
		}
	}

	// check for problems
	bool failure = false;
	uint8 reason;
	if (newPlayer == -1) {
		LOG(INFO, "Server is full");
		failure = true;
		reason = SERVER_FULL;
	} else if (duplicate != -1) {
		LOG(INFO, "Duplicate remote endpoint with player " << duplicate);
		failure = true;
		reason = DUPLICATE_ENDPOINT;
	}

	// build response
	outBuf.clear();
	if (!failure) {
		uint8 id = (uint) newPlayer;
		uint32 token = 0; //TODO

		clients[id].connected = true;
		clients[id].endpoint = endpoint;
		clients[id].token = token;
		clients[id].timeOfLastPacket = my::time::now();

		ConnectionAcceptedResponse msg;
		msg.token = token;
		msg.id = id;
		outBuf << MAGIC << CONNECTION_ACCEPTED << msg;
		LOG(INFO, "Player " << (int) id << " connected with token " << token);
	} else {
		ConnectionRejectedResponse msg;
		msg.reason = reason;
		outBuf << MAGIC << CONNECTION_REJECTED << msg;
	}

	// send response
	socket.send(outBuf, endpoint);
}

void Server::handleClientMessage(uint8 type, uint8 id, const Buffer &buffer) {
	LOG(TRACE, "Message " << (int) type << " for Player " << (int) id << " accepted");
	switch (type) {
	case ECHO_REQUEST: {
		outBuf.clear();
		outBuf << MAGIC << ECHO_RESPONSE << buffer;
		socket.send(outBuf, clients[id].endpoint);
		break;
	}
	default:
		break;
	}
}

void Server::checkInactive() {
	for (uint8 id = 0; id < MAX_CLIENTS; ++id) {
		if (!clients[id].connected)
			continue;

		if (my::time::now() - clients[id].timeOfLastPacket > timeout) {
			LOG(INFO, "Player " << (int) id << " timed out");

			outBuf.clear();
			outBuf << MAGIC << CONNECTION_TIMEOUT;
			socket.send(outBuf, clients[id].endpoint);

			disconnect(id);
		}
	}
}

void Server::disconnect(uint8 id) {
	LOG(INFO, "Player " << (int) id << " disconnected");
	clients[id].connected = false;
}
