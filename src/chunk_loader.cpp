#include "chunk_loader.hpp"

#include <cstring>

#include <thread>
#include <future>
#include <chrono>

#include "util.hpp"
#include "constants.hpp"
#include "chunk.hpp"
#include "monitor.hpp"

using namespace std;

ChunkLoader::ChunkLoader(World *world, uint64 seed, bool updateFaces) :
	perlin(seed), isLoaded(0, vec3i64HashFunc), queue(1000),
	chunkArchive("./region/")
{
	this->world = world;
	this->updateFaces = updateFaces;
}

ChunkLoader::~ChunkLoader() {
	storeChunksOnDisk();

	Chunk *chunk = nullptr;
	while ((chunk = next()) != nullptr) {
		delete chunk;
	}
}

void ChunkLoader::dispatch() {
	shouldHalt = false;
	fut = async(launch::async, [this]() { run(); });
}

void ChunkLoader::requestTermination() {
	 shouldHalt.store(true, memory_order_relaxed);
}


void ChunkLoader::wait() {
	requestTermination();
	fut.get();
}

Chunk *ChunkLoader::next() {
	Chunk *chunk;
	bool result = queue.pop(chunk);
	return result ? chunk : nullptr;
}

ProducerStack<vec3i64>::Node *ChunkLoader::getUnloadQueries() {
	return unloadQueries.consumeAll();
}

void ChunkLoader::run() {
	using namespace vec_auto_cast;
	static const int LENGTH = CHUNK_LOAD_RANGE * 2 + 1;
	static const int MAX_LOADS = LENGTH * LENGTH * LENGTH;

	for (uint8 i = 0; i < MAX_CLIENTS; i++) {
		playerChunkIndex[i] = 0;
		playerChunksLoaded[i] = 0;
	}

	for (uint8 i = 0; i < MAX_CLIENTS; i++) {
		updatePlayerInfo(i);
	}

	while (!shouldHalt.load(memory_order_seq_cst)) {
		bool didSomething = false;
		int loads = 0;
		int oldLoads;
		do {
			oldLoads = loads;
			for (uint8 i = 0; i < MAX_CLIENTS; i++) {
				// don't wait for the reading-lock to be released
				if (!updatePlayerInfo(i, false) || !isPlayerValid[i])
					continue;

				if (playerChunkIndex[i] >= LOADING_ORDER.size()
						|| playerChunksLoaded[i] > MAX_LOADS)
					continue;

				vec3i8 ccd = LOADING_ORDER[playerChunkIndex[i]++];
				while (ccd.maxAbs() > CHUNK_LOAD_RANGE
						&& playerChunkIndex[i] < LOADING_ORDER.size())
					ccd = LOADING_ORDER[playerChunkIndex[i]++];

				if (playerChunkIndex[i] >= LOADING_ORDER.size())
					continue;

				vec3i64 cc = ccd + lastPcc[i];
				auto iter = isLoaded.find(cc);
				if (iter == isLoaded.end()) {
					isLoaded.insert(cc);
					Chunk *chunk = chunkArchive.loadChunk(cc);
					if (!chunk)
						chunk = generateChunk(cc);
					chunk->setChunkLoader(this);
					if (updateFaces)
						chunk->initFaces();
					while (!queue.push(chunk))
						this_thread::sleep_for(milliseconds(100));
				}

				playerChunksLoaded[i]++;
				loads++;
				didSomething = true;
			}
		} while (loads < MAX_LOADS_UNTIL_UNLOAD && loads > oldLoads && !shouldHalt.load(memory_order_seq_cst));

		sendOffloadQueries();
		storeChunksOnDisk();

		if (!didSomething)
			this_thread::sleep_for(milliseconds(100));
	} // while not thread interrupted
}

Chunk *ChunkLoader::generateChunk(vec3i64 cc) {
	Chunk *chunk = new Chunk(cc);
	for (uint ix = 0; ix < Chunk::WIDTH; ix++) {
		for (uint iy = 0; iy < Chunk::WIDTH; iy++) {
			long x = round((cc[0] * Chunk::WIDTH + ix) / overAllScale);
			long y = round((cc[1] * Chunk::WIDTH + iy) / overAllScale);
			double ax = x / perlinAreaXYScale;
			double ay = y / perlinAreaXYScale;
			double ap = perlin.perlin(ax, ay, 0);
			double mFac = (1 + tanh((ap - perlinAreaMountainThreshold)
					* perlinAreaSharpness)) / 2;
			double fFac = 1 - mFac;

			double mx = x / perlinMountainXYScale;
			double my = y / perlinMountainXYScale;
			double mh = perlin.octavePerlin(mx, my, 0,
					perlinMountainOctaves, perlinMountainExp)
					* perlinMountainMaxHeight;

			double fx = x / perlinFlatlandXYScale;
			double fy = y / perlinFlatlandXYScale;
			double fh = perlin.octavePerlin(fx, fy, 0,
					perlinFlatlandOctaves, perlinFlatlandExp)
					* perlinFlatLandMaxHeight;

			double h = fh * fFac + mh * mFac;
			for (uint iz = 0; iz < Chunk::WIDTH; iz++) {
				int index = Chunk::getBlockIndex(vec3ui8(ix, iy, iz));
				long wz = iz + cc[2] * Chunk::WIDTH;
				if (wz > h) {
					chunk->initBlock(index, 0);
					continue;
				}
				double px = (cc[0] * Chunk::WIDTH + ix) / perlinCaveScale;
				double py = (cc[1] * Chunk::WIDTH + iy) / perlinCaveScale;
				double pz = (cc[2] * Chunk::WIDTH + iz) / perlinCaveScale;
				double v = perlin.octavePerlin(px, py, pz, 6, 0.5);
				if (v > 0.12 * (3 - 1 / ((floor(h) - wz) / 10 + 1)))
					chunk->initBlock(index, 1);
				else
					chunk->initBlock(index, 0);
			}
		}
	}
	return chunk;
}

void ChunkLoader::storeChunksOnDisk() {
	int counter = 0;
	auto deletedChunksList = deletedChunks.consumeAll();
	while (deletedChunksList)
	{
		Chunk *chunk = deletedChunksList->data;
		chunkArchive.storeChunk(*chunk);
		delete chunk;

		auto tmp = deletedChunksList->next;
		delete deletedChunksList;
		deletedChunksList = tmp;
		counter++;
	}
}

void ChunkLoader::sendOffloadQueries() {
	for (uint8 i = 0; i < MAX_CLIENTS; i++) {
		updatePlayerInfo(i);
	}

	for (auto iter = isLoaded.begin(); iter != isLoaded.end();) {
		vec3i64 cc = *iter;
		bool inRange = false;
		for (uint8 i = 0; i < MAX_CLIENTS; i++) {
			if (!isPlayerValid[i])
				continue;
			if ((cc - lastPcc[i]).maxAbs() <= CHUNK_UNLOAD_RANGE) {
				inRange = true;
				break;
			}
		}
		if (!inRange) {
			unloadQueries.push(cc);
			iter = isLoaded.erase(iter);
		} else
			iter++;
	}
}

bool ChunkLoader::updatePlayerInfo(uint8 i, bool wait) {
	int handle;
	bool valid;
	vec3i64 pcc;
	Player &player = world->getPlayer(i);
	Monitor &validPosMonitor = player.getValidPosMonitor();
	bool success = false;
	do {
		handle = validPosMonitor.startRead();
		valid = player.isValid();
		pcc = player.getChunkPos();
		success = validPosMonitor.finishRead(handle);
	} while (!success && wait);
	if (!success)
		return false;

	if (!valid || lastPcc[i] != pcc) {
		playerChunkIndex[i] = 0;
		playerChunksLoaded[i] = 0;
	}

	lastPcc[i] = pcc;
	isPlayerValid[i] = valid;
	return true;
}
