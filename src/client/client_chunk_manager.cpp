#include "client_chunk_manager.hpp"

#include "shared/engine/logging.hpp"
#include "shared/engine/time.hpp"
#include "shared/game/world.hpp"
#include "client/client.hpp"
#include "client/server_interface.hpp"

using namespace std;

static logging::Logger logger("ccm");

ClientChunkManager::ClientChunkManager(Client *client, std::unique_ptr<ChunkArchive> archive) :
	loadedStoredQueue(1024),
	toLoadStoreQueue(1024),
	chunks(0, vec3i64HashFunc),
	oldRevisions(0, vec3i64HashFunc),
	needCounter(0, vec3i64HashFunc),
	client(client),
	archive(std::move(archive))
{
	for (int i = 0; i < CHUNK_POOL_SIZE; i++) {
		chunkPool[i] = new Chunk(Chunk::ChunkFlags::VISUAL);
		unusedChunks.push(chunkPool[i]);
	}
	dispatch();
}

ClientChunkManager::~ClientChunkManager() {
	LOG_TRACE(logger) << "Destroying ChunkManager";
	storeChunks();
	for (int i = 0; i < CHUNK_POOL_SIZE; i++) {
		delete chunkPool[i];
	}
}

void ClientChunkManager::tick() {
	while (!requestedQueue.empty() && !unusedChunks.empty()) {
		vec3i64 cc = requestedQueue.front();
		Chunk *chunk = unusedChunks.top();
		chunk->initCC(cc);
		if (!chunk)
			LOG_ERROR(logger) << "Chunk allocation failed";
		ArchiveOperation op = {chunk, LOAD};
		if (toLoadStoreQueue.push(op)) {
			requestedQueue.pop();
			unusedChunks.pop();
		} else
			break;
	}

	while (!preToStoreQueue.empty()) {
		Chunk *chunk = preToStoreQueue.front();
		ArchiveOperation op = {chunk, STORE};
		if (toLoadStoreQueue.push(op)) {
			preToStoreQueue.pop();
			continue;
		} else
			break;
	}

	while(!notInCacheQueue.empty()) {
		Chunk *chunk = notInCacheQueue.front();
		if (client->getServerInterface()->requestChunk(chunk, false, -1))
			notInCacheQueue.pop();
		else
			break;
	}

	ArchiveOperation op;
	while (loadedStoredQueue.pop(op)) {
		switch(op.type) {
		case LOAD:
			if (op.chunk->isInitialized()) {
				if (insertLoadedChunk(op.chunk))
					oldRevisions.insert({op.chunk->getCC(), op.chunk->getRevision()});
			} else {
				notInCacheQueue.push(op.chunk);
			}
			numSessionChunkLoads++;
			break;
		case STORE:
			recycleChunk(op.chunk);
			break;
		}
	}

	Chunk *chunk;
	while ((chunk = client->getServerInterface()->getNextChunk()) != nullptr) {
		if (!chunk->isInitialized())
			LOG_WARNING(logger) << "Server interface didn't initialize chunk";
		insertLoadedChunk(chunk);
		numSessionChunkGens++;
	}
}

void ClientChunkManager::doWork() {
	ArchiveOperation op;
	if (toLoadStoreQueue.pop(op)) {
		switch (op.type) {
		case LOAD:
			archive->loadChunk(op.chunk);
			break;
		case STORE:
			archive->storeChunk(*op.chunk);
			break;
		}
		while (!loadedStoredQueue.push(op)) {
			sleepFor(millis(50));
		}
	} else {
		sleepFor(millis(100));
	}
}

void ClientChunkManager::onStop() {
	ArchiveOperation op;
	while (toLoadStoreQueue.pop(op)) {
		if (op.type == STORE) {
			archive->storeChunk(*op.chunk);
		}
	}
}

void ClientChunkManager::storeChunks() {
	requestTermination();
	ArchiveOperation op;
	while(loadedStoredQueue.pop(op));
	wait();
	while (!preToStoreQueue.empty()) {
		Chunk *chunk = preToStoreQueue.front();
		preToStoreQueue.pop();
		archive->storeChunk(*chunk);
	}
	for (auto it1 = chunks.begin(); it1 != chunks.end(); ++it1) {
		auto it2 = oldRevisions.find(it1->first);
		if (it2 == oldRevisions.end() || it1->second->getRevision() != it2->second)
			archive->storeChunk(*it1->second);
	}
}

void ClientChunkManager::placeBlock(vec3i64 chunkCoords, size_t intraChunkIndex,
		uint blockType, uint32 revision) {
	auto it = chunks.find(chunkCoords);
	if (it != chunks.end()) {
		if (it->second->getRevision() == revision)
			it->second->setBlock(intraChunkIndex, blockType);
		else
			LOG_WARNING(logger) << "couldn't apply chunk patch";
	}
	// TODO operate on cache if chunk is not loaded
}

const Chunk *ClientChunkManager::getChunk(vec3i64 chunkCoords) const {
	auto it = chunks.find(chunkCoords);
	if (it != chunks.end())
		return it->second;
	return nullptr;
}

void ClientChunkManager::requestChunk(vec3i64 chunkCoords) {
	auto it = needCounter.find(chunkCoords);
	if (it == needCounter.end()) {
		requestedQueue.push(chunkCoords);
		needCounter.insert({chunkCoords, 1});
	} else {
		it->second++;
	}
}

void ClientChunkManager::releaseChunk(vec3i64 chunkCoords) {
	auto it1 = needCounter.find(chunkCoords);
	if (it1 != needCounter.end()) {
		it1->second--;
		if (it1->second == 0) {
			needCounter.erase(it1);
			auto it2 = chunks.find(chunkCoords);
			if (it2 != chunks.end()) {
				auto it3 = oldRevisions.find(chunkCoords);
				if (it3 == oldRevisions.end() || it2->second->getRevision() != it3->second)
					preToStoreQueue.push(it2->second);
				else
					recycleChunk(it2->second);
				chunks.erase(it2);
				if (it3 != oldRevisions.end())
					oldRevisions.erase(it3);
			}
		}
	}
}

int ClientChunkManager::getNumNeededChunks() const {
	return needCounter.size();
}

int ClientChunkManager::getNumAllocatedChunks() const {
	return CHUNK_POOL_SIZE - unusedChunks.size();
}

int ClientChunkManager::getNumLoadedChunks() const {
	return chunks.size();
}

int ClientChunkManager::getRequestedQueueSize() const {
	return requestedQueue.size();
}

int ClientChunkManager::getNotInCacheQueueSize() const {
	return notInCacheQueue.size();
}

bool ClientChunkManager::insertLoadedChunk(Chunk *chunk) {
	auto it = needCounter.find(chunk->getCC());
	if (it != needCounter.end()) {
		chunks.insert({chunk->getCC(), chunk});
		return true;
	} else {
		recycleChunk(chunk);
		return false;
	}
}

void ClientChunkManager::recycleChunk(Chunk *chunk) {
	chunk->reset();
	unusedChunks.push(chunk);
}
