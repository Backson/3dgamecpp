#include "local_server_interface.hpp"

LocalServerInterface::LocalServerInterface(World *world, uint64 seed)
		: world(world), chunkLoader(world, seed, true), conf(conf) {
	world->addPlayer(0);
	chunkLoader.setRenderDistance(CHUNK_LOAD_RANGE);
	chunkLoader.dispatch();
}


LocalServerInterface::~LocalServerInterface() {
	// nothing
}

void LocalServerInterface::togglePlayerFly() {
	// TODO move this to client.cpp
	if (!world->isPaused())
		world->getPlayer(0).setFly(!world->getPlayer(0).getFly());
}

void LocalServerInterface::setPlayerMoveInput(int moveInput) {

}

void LocalServerInterface::setPlayerOrientation(double yaw, double pitch) {

}

void LocalServerInterface::edit(vec3i64 bc, uint8 type) {
	// TODO move this to client.cpp
	if (!world->isPaused())
		world->setBlock(bc, type, true);
}

void LocalServerInterface::receive(uint64 timeLimit) {
	Chunk *chunk = nullptr;
	while ((chunk = chunkLoader.next()) != nullptr)
		world->insertChunk(chunk);

	auto unloadQueries = chunkLoader.getUnloadQueries();
	while (unloadQueries)
	{
		Chunk *chunk = world->removeChunk(unloadQueries->data);
		chunk->free();
		auto tmp = unloadQueries->next;
		delete unloadQueries;
		unloadQueries = tmp;
	}
}

void LocalServerInterface::sendInput() {

}

void LocalServerInterface::setConf(const GraphicsConf &conf) {
	GraphicsConf old_conf = this->conf;
	this->conf = conf;

	if (conf.render_distance != old_conf.render_distance) {
		world->clearChunks();
		chunkLoader.setClientRenderDistance(0, conf.render_distance);
	}
}

int LocalServerInterface::getLocalClientID() {
	return 0;
}

void LocalServerInterface::stop() {
	chunkLoader.wait();
	world->deletePlayer(0);
}

