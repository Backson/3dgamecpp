#include "chunk.hpp"
#include "world.hpp"
#include "util.hpp"

static size_t faceHashFunc(Face f) {
		static const int prime = 31;
		int result = 1;
		result = prime * result + f.block[0];
		result = prime * result + f.block[1];
		result = prime * result + f.block[2];
		result = prime * result + f.dir;
		return result;
}

Chunk::Chunk(vec3i64 cc) :
		cc(cc), faces(0, faceHashFunc) {
	// nothing
}

bool operator == (const Face &lhs, const Face &rhs) {
	if (&lhs == &rhs)
		return true;
    return lhs.block == rhs.block && lhs.dir == rhs.dir;
}

const double World::GRAVITY = -9.81 * RESOLUTION / 60.0 / 60.0 * 4;


void Chunk::init(uint8 blocks[WIDTH * WIDTH * WIDTH]) {
	for (uint i = 0; i < WIDTH * WIDTH * WIDTH; i++) {
		this->blocks[i] = blocks[i];
	}
}

void Chunk::initFaces(World &world) {
	// TODO only one loop
	for (uint z = 0; z < WIDTH; z++) {
		for (uint y = 0; y < WIDTH; y++) {
			for (uint x = 0; x < WIDTH; x++) {
				updateBlockFaces(vec3ui8(x, y, z), world);
			}
		}
	}
}

bool Chunk::setBlock(vec3ui8 icc, uint8 type, World &world) {
	if (getBlock(icc) == type)
		return true;
	blocks[getBlockIndex(icc)] = type;
	updateBlockFaces(icc, world);
	changed = true;
	return true;
}

uint8 Chunk::getBlock(vec3ui8 icc) const {
	return blocks[getBlockIndex(icc)];
}

const Chunk::FaceSet &Chunk::getFaceSet() const {
	return faces;
}

bool Chunk::pollChanged() {
	bool tmp = changed;
	changed = false;
	return tmp;
}
/*
void Chunk::write(ByteBuffer buffer) const {
	buffer.putLong(cx);
	buffer.putLong(cy);
	buffer.putLong(cz);
	Deflater deflater = new Deflater();
	deflater.setInput(blocks);
	deflater.finish();
	byte cBytes[(int) ((WIDTH * WIDTH * WIDTH) * 1.1)];
	int written = deflater.deflate(cBytes, 0, cBytes.length,
			Deflater.SYNC_FLUSH);
	buffer.putShort((short) written);
	buffer.put(cBytes, 0, written);
}

static Chunk Chunk::readChunk(ByteBuffer buffer) {
	vec3i64 cc(
		buffer.getLong(),
		buffer.getLong(),
		buffer.getLong()
	);
	short written = buffer.getShort();
	byte cBytes[written];
	buffer.get(cBytes);
	Inflater inflater = new Inflater();
	inflater.setInput(cBytes);
	byte blocks[WIDTH * WIDTH * WIDTH];
	try {
		inflater.inflate(blocks);
	} catch (DataFormatException e) {
		e.printStackTrace();
	}
	return Chunk(cc, blocks);
}
*/
int Chunk::getBlockIndex(vec3ui8 icc) {
	return icc[2] * WIDTH * WIDTH + icc[1] * WIDTH + icc[0];
}

static uint8 helperFunc(uint8 t) {
	return (t + Chunk::WIDTH) % Chunk::WIDTH;
}

void Chunk::updateBlockFaces(vec3ui8 icc, World &world) {
	using namespace vec_auto_cast;
	for (uint8 d = 0; d < 6; d++) {
		uint8 invD = (d + 3) % 6;
		vec3i dir = DIRS[d];
		vec3ui8 nIcc = (icc + dir).cast<uint8>();
		uint8 neighborType;
		FaceSet *nFaces = nullptr;
		if (		nIcc[0] < 0 || nIcc[0] >= WIDTH
				||	nIcc[1] < 0 || nIcc[1] >= WIDTH
				||	nIcc[2] < 0 || nIcc[2] >= WIDTH) {
			nIcc.applyPW(helperFunc);
			vec3i64 nc = cc + dir;
			// TODO this is probably not thread safe
			auto it = world.getChunks().find(nc);
			if (it == world.getChunks().end())
				continue;
			neighborType = it->second->getBlock(nIcc);
			if (neighborType != 0)
				it->second->changed = true;
			nFaces = &it->second->faces;
		} else {
			neighborType = getBlock(nIcc);
			nFaces = &faces;
		}

		// TODO thread safe?
		if (neighborType != 0) {
			if (getBlock(icc) != 0)
				nFaces->erase(Face{nIcc, invD});
			else
				nFaces->insert(Face{nIcc, invD});
		} else {
			if (getBlock(icc) != 0)
				faces.insert(Face{icc, d});
			else
				faces.erase(Face{icc, d});
		}
	}
}
