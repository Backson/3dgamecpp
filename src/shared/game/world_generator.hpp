/*
 * world_generator.hpp
 *
 *  Created on: 17.09.2014
 *      Author: lars
 */

#ifndef WORLD_GENERATOR_HPP_
#define WORLD_GENERATOR_HPP_

#include "shared/engine/macros.hpp"

#include "elevation_generator.hpp"
#include "perlin.hpp"
#include "chunk.hpp"

class ElevationGenerator;

struct WorldParams {
	double overall_scale = 1;

	double elevation_xy_scale  = 100000;
	double elevation_z_scale   = 100;
	int    elevation_octaves   = 6;
	double elevation_ampl_gain = 0.4;
	double elevation_freq_gain = 5.0;

	double mountain_xy_scale  = 5000;
	double mountain_z_scale   = 6000;
	int    mountain_octaves   = 7;
	double mountain_ampl_gain = 0.4;
	double mountain_freq_gain = 2.5;
	double mountain_exp       = 12;

	double surfaceScale           = 70;
	double surfaceRelDepth        = 0.3;
	int    surfaceOctaves         = 6;
	double surfaceAmplGain        = 0.4;
	double surfaceFreqGain        = 2.0;
	double surfaceThresholdXScale = 1;

	double cavenessScale     = 100;
	int    cavenessOctaves   = 2;
	double cavenessAmplGain  = 0.3;
	double cavenessFreqGain  = 2.0;
	double cavenessDepthGain1 = 50.0;
	double cavenessDepthGain2 = 300.0;
	double cavenessDepthGainFac1 = 0.8;
	double cavenessDepthGainFac2 = 0.2;

	double tunnelSwitchOverlap  = 0.1;
	double tunnelSwitchScale    = 600;
	int    tunnelSwitchOctaves  = 1;
	double tunnelSwitchAmplGain = 0.3;
	double tunnelSwitchFreqGain = 2.0;

	double tunnelScale    = 200;
	int    tunnelOctaves  = 1;
	double tunnelAmplGain = 0.3;
	double tunnelFreqGain = 3.0;

	double caveRoomValue = 600;

	double caveThreshold = 500;

	double vegetation_xy_scale  = 1000;
	double temperature_xy_scale = 1500;
	double hollowness_xy_scale  = 800;

	double desert_threshold = 0.25;
	double grasland_threshold = -0.2;
};

class WorldGenerator {
public:
	WorldGenerator(uint64 seed, WorldParams params);
	~WorldGenerator();

	void generateChunk(Chunk *);
	vec3i64 getSpawnLocation();

private:
	WorldParams wp;

	ElevationGenerator elevationGenerator;
	Perlin vegetation_perlin;
	Perlin temperature_perlin;
	
	Perlin surfacePerlin;
	Perlin cavenessPerlin;
	Perlin tunnelSwitchPerlin;
	Perlin tunnelPerlin1a;
	Perlin tunnelPerlin2a;
	Perlin tunnelPerlin3a;
	Perlin tunnelPerlin1b;
	Perlin tunnelPerlin2b;
	Perlin tunnelPerlin3b;

	double *tunnelSwitchBuffer;
	double *cavenessBuffer;
};

#endif // WORLD_GENERATOR_HPP_
