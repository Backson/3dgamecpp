#ifndef CHARACTER_HPP
#define CHARACTER_HPP

#include "shared/engine/vmath.hpp"
#include "shared/net.hpp"

class World;

class Character {
public:
	static const double FLY_ACCELERATION;
	static const double FLY_SPRINT_ACCELERATION;
	static const double FLY_FRICTION;
	static const double GROUND_ACCELERATION;
	static const double GROUND_SPRINT_ACCELERATION;
	static const double GROUND_FRICTION;
	static const double AIR_ACCELERATION;
	static const double AIR_FRICTION;
	static const double AIR_ACCELERATION_PENALTY;
	static const double JUMP_SPEED;

	static const int MOVE_INPUT_FLAG_STRAFE_RIGHT = 1;
	static const int MOVE_INPUT_FLAG_STRAFE_LEFT = 2;
	static const int MOVE_INPUT_FLAG_FLY_UP = 4;
	static const int MOVE_INPUT_FLAG_FLY_DOWN = 8;
	static const int MOVE_INPUT_FLAG_MOVE_FORWARD = 16;
	static const int MOVE_INPUT_FLAG_MOVE_BACKWARD = 32;
	static const int MOVE_INPUT_FLAG_SPRINT = 64;

	static const int EYE_HEIGHT = 1700;
	static const int RADIUS = 300;
	static const int HEIGHT = 1800;
	static const int TARGET_RANGE = 16;

private:
	World *world = nullptr;

	bool isFlying;

	vec3i64 pos;
	vec3d vel;
	int yaw;
	int pitch;

	int moveInput;

	bool valid = false;

	// currently selected building block
	uint8 block = 1;

public:
	void tick();
	
	void setPos(vec3i64 pos);
	void setOrientation(int yaw, int pitch);
	void setFly(bool fly);
	void setMoveInput(int moveInput);

	void setBlock(uint8 b) { block = b; }

	vec3i64 getPos() const;
	vec3d getVel() const;
	int getYaw() const;
	int getPitch() const;
	bool getFly() const;
	int getMoveInput() const;
	vec3i64 getChunkPos() const;
	uint8 getBlock() const { return block; }

	void create(World *world);
	void destroy();
	bool isValid() const;

	bool getTargetedFace(vec3i64 *outBlock, int *outFaceDir) const;

	void applySnapshot(const CharacterSnapshot &snapshot, bool local);
	CharacterSnapshot makeSnapshot(int tick) const;

private:
	void calcVel();
	void collide();
	void ghost();

	bool isGrounded() const;
};

#endif // CHARACTER_HPP
