#include "perlin.hpp"

#include <algorithm>
#include <cstring>

#include "shared/engine/random.hpp"

double NoiseBase::noise2(double x, double y, uint octaves, double amplGain, double freqGain) {
	return noise3(x, y, 0, octaves, amplGain, freqGain);
}

void NoiseBase::noise3(
	double sx, double sy, double sz,
	double dx, double dy, double dz,
	uint nx, uint ny, uint nz,
	uint octaves, double amplGain, double freqGain,
	double *buffer)
{
	uint index = 0;
	uint ix, iy, iz;
	double x, y, z;
	for (iz = 0, z = sz; iz < nz; iz++, z += dz)
	for (iy = 0, y = sy; iy < ny; iy++, y += dy)
	for (ix = 0, x = sx; ix < nx; ix++, x += dx) {
		buffer[index++] = noise3(x, y, z, octaves, amplGain, freqGain);
	}
}

void NoiseBase::noise2(
	double sx, double sy,
	double dx, double dy,
	uint nx, uint ny,
	uint octaves, double amplGain, double freqGain,
	double *buffer)
{
	int index = 0;
	uint ix, iy;
	double x, y;
	for (iy = 0, y = sy; iy < ny; iy++, y += dy)
	for (ix = 0, x = sx; ix < nx; ix++, x += dx) {
		buffer[index++] = noise2(x, y, octaves, amplGain, freqGain);
	}
}

Perlin::Hasher::Hasher(uint64 seed) {
    std::mt19937 rng((uint32) (seed ^ (seed >> 32)));
    for (int i = 0; i < 0x400; ++i) {
        p[i] = i;
    }
	shuffle(p, p + 0x400, rng);
}

Perlin::Perlin(uint64 seed) : hasher(seed) {}

double Perlin::noise3(double x, double y, double z, uint octaves, double amplGain, double freqGain) {
    double total = 0;
    double freq = 1;
    double amplitude = 1;
    double max_value = 0;
    for (uint i = 0; i < octaves; i++) {
        total += perlin3(x * freq, y * freq, z * freq, i) * amplitude;
		// most implementations want to preserve the values falling in the interval [0, 1] but we
		// want to instead preserve the statistical properties of the individual octaves, namely
		// the variance.  This way the distribution of values from the noise function is not
		// dependent on the number of octaves used.
		max_value += amplitude * amplitude;
        amplitude *= amplGain;
        freq *= freqGain;
    }
	return total / sqrt(max_value);
}

double Perlin::noise2(double x, double y, uint octaves, double amplGain, double freqGain) {
    double total = 0;
    double freq = 1;
    double amplitude = 1;
    double max_value = 0;
    for (uint i = 0; i < octaves; i++) {
        total += perlin2(x * freq, y * freq, i) * amplitude;
		max_value += amplitude * amplitude;
        amplitude *= amplGain;
        freq *= freqGain;
    }
	return total / sqrt(max_value);
}

void Perlin::noise3(
	double sx, double sy, double sz,
	double dx, double dy, double dz,
	uint nx, uint ny, uint nz,
	uint octaves, double amplGain, double freqGain,
	double *buffer)
{
	memset(buffer, 0, nx * ny * nz * sizeof(double));

    double freq = 1;
    double amplitude = 1;
    double max_value = 0;
    for (uint i = 0; i < octaves; i++) {
        perlin3(sx * freq, sy * freq, sz * freq, dx * freq, dy * freq, dz * freq,
				nx, ny, nz, i, amplitude, buffer);
		max_value += amplitude * amplitude;
        amplitude *= amplGain;
        freq *= freqGain;
    }

	for (uint i = 0; i < nx * ny * nz; ++i) {
		buffer[i] = buffer[i] / sqrt(max_value);
	}
}

void Perlin::noise2(
	double sx, double sy,
	double dx, double dy,
	uint nx, uint ny,
	uint octaves, double amplGain, double freqGain,
	double *buffer)
{
	memset(buffer, 0, nx * ny * sizeof(double));

    double freq = 1;
    double amplitude = 1;
    double max_value = 0;
    for (uint i = 0; i < octaves; i++) {
        perlin2(sx * freq, sy * freq, dx * freq, dy * freq, nx, ny, i, amplitude, buffer);
		max_value += amplitude * amplitude;
        amplitude *= amplGain;
        freq *= freqGain;
    }

	for (uint i = 0; i < nx * ny; ++i) {
		buffer[i] = buffer[i] / sqrt(max_value);
	}
}

double Perlin::perlin3(double x, double y, double z, int which_octave) {
	// lowest corner of the cell, opposite corner have xi + 1 etc
    const int xi = (int) floor(x);
    const int yi = (int) floor(y);
    const int zi = (int) floor(z);

	// relative position in the cell
    const double xf = x - floor(x);
    const double yf = y - floor(y);
    const double zf = z - floor(z);

	// fade constants to smooth the solution
    const double u = fade(xf);
    const double v = fade(yf);
    const double w = fade(zf);

	// calculate pseudorandom hashes for all the corners
	// we also hash the number of the octave, so the octaves will not be correlated
	hasher.reset() << which_octave << xi << yi << zi;
	const uint8 aaa = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi << yi + 1 << zi;
	const uint8 aba = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi << yi << zi + 1;
	const uint8 aab = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi << yi + 1 << zi + 1;
	const uint8 abb = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi + 1 << yi << zi;
	const uint8 baa = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi + 1 << yi + 1 << zi;
	const uint8 bba = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi + 1 << yi << zi + 1;
	const uint8 bab = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi + 1 << yi + 1 << zi + 1;
	const uint8 bbb = hasher.get() & 0xFF;

	// multiply the relative coordinate in the cell with the random gradient and lerp it together
    const double caa = lerp(grad3(aaa, xf, yf, zf), grad3(baa, xf - 1, yf, zf), u);
    const double cba = lerp(grad3(aba, xf, yf - 1, zf), grad3(bba, xf - 1, yf - 1, zf), u);
    const double cab = lerp(grad3(aab, xf, yf, zf - 1), grad3(bab, xf - 1, yf, zf - 1), u);
    const double cbb = lerp(grad3(abb, xf, yf - 1, zf - 1), grad3(bbb, xf - 1, yf - 1, zf - 1), u);
    const double cca = lerp(caa, cba, v);
    const double ccb = lerp(cab, cbb, v);
    return lerp(cca, ccb, w);
}

double Perlin::perlin2(double x, double y, int which_octave) {
	// lowest corner of the cell, opposite corner have xi + 1 etc
    const int xi = (int) floor(x);
    const int yi = (int) floor(y);

	// relative position in the cell
    const double xf = x - floor(x);
    const double yf = y - floor(y);

	// fade constants to smooth the solution
    const double u = fade(xf);
    const double v = fade(yf);

	// calculate pseudorandom hashes for all the corners
	// we also hash the number of the octave, so the octaves will not be correlated
	hasher.reset() << which_octave << xi << yi;
	const uint8 aa = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi << yi + 1;
	const uint8 ab = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi + 1 << yi;
	const uint8 ba = hasher.get() & 0xFF;
	hasher.reset() << which_octave << xi + 1 << yi + 1;
	const uint8 bb = hasher.get() & 0xFF;

	// multiply the relative coordinate in the cell with the random gradient and lerp it together
    const double ca = lerp(grad2(aa, xf, yf), grad2(ba, xf - 1, yf), u);
    const double cb = lerp(grad2(ab, xf, yf - 1), grad2(bb, xf - 1, yf - 1), u);
    return lerp(ca, cb, v);
}

void Perlin::perlin3(double sx, double sy, double sz, double dx, double dy, double dz,
	uint nx, uint ny, uint nz, int which_octave, double amplitude, double *buffer)
{
	uint ix, iy, iz;
	double x, y, z;

	// if there are at least 4 points in each cell
	if (dx < 0.5 && dy < 0.5) {
		int xcelln, ycelln, zcelln;
		for (iz = 0, z = sz; iz < nz; iz += zcelln, z += zcelln * dz)
		for (iy = 0, y = sy; iy < ny; iy += ycelln, y += ycelln * dy)
		for (ix = 0, x = sx; ix < nx; ix += xcelln, x += xcelln * dx) {
			// lowest corner of the cell, opposite corner have xi + 1 etc
			const int xcelli = (int) floor(x);
			const int ycelli = (int) floor(y);
			const int zcelli = (int) floor(z);

			// calculate pseudorandom hashes for all the corners
			// we also hash the number of the octave, so the octaves will not be correlated
			hasher.reset() << which_octave << xcelli << ycelli << zcelli;
			const uint8 aaa = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli << ycelli + 1 << zcelli;
			const uint8 aba = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli << ycelli << zcelli + 1;
			const uint8 aab = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli << ycelli + 1 << zcelli + 1;
			const uint8 abb = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli << zcelli;
			const uint8 baa = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli + 1 << zcelli;
			const uint8 bba = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli << zcelli + 1;
			const uint8 bab = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli + 1 << zcelli + 1;
			const uint8 bbb = hasher.get() & 0xFF;

			// relative position in the cell
			const double xf = x - floor(x);
			const double yf = y - floor(y);
			const double zf = z - floor(z);
			
			// how many points are in this cell?
			xcelln = std::min(1 + (int) ((1.0 - xf) / dx), (int) (nx - ix));
			ycelln = std::min(1 + (int) ((1.0 - yf) / dy), (int) (ny - iy));
			zcelln = std::min(1 + (int) ((1.0 - zf) / dz), (int) (nz - iz));

			int ixx, iyy, izz;
			double xxf, yyf, zzf;
			for (izz = 0, zzf = zf; izz < zcelln; ++izz, zzf += dz)
			for (iyy = 0, yyf = yf; iyy < ycelln; ++iyy, yyf += dy)
			for (ixx = 0, xxf = xf; ixx < xcelln; ++ixx, xxf += dx) {
				const double u = fade(xxf);
				const double v = fade(yyf);
				const double w = fade(zzf);
				const double caa = lerp(grad3(aaa, xxf, yyf, zzf),
						grad3(baa, xxf - 1, yyf, zzf), u);
				const double cba = lerp(grad3(aba, xxf, yyf - 1, zzf),
						grad3(bba, xxf - 1, yyf - 1, zzf), u);
				const double cab = lerp(grad3(aab, xxf, yyf, zzf - 1),
						grad3(bab, xxf - 1, yyf, zzf - 1), u);
				const double cbb = lerp(grad3(abb, xxf, yyf - 1, zzf - 1),
						grad3(bbb, xxf - 1, yyf - 1, zzf - 1), u);
				const double cca = lerp(caa, cba, v);
				const double ccb = lerp(cab, cbb, v);
				int index = ((izz + iz) * ny + (iyy + iy)) * nx + (ixx + ix);
				buffer[index] += lerp(cca, ccb, w) * amplitude;
			}
		}
	} else {
		int index = 0;
		for (iz = 0, z = sz; iz < nz; iz++, z += dz)
		for (iy = 0, y = sy; iy < ny; iy++, y += dy)
		for (ix = 0, x = sx; ix < nx; ix++, x += dx) {
			// lowest corner of the cell, opposite corner have xi + 1 etc
			const int xcelli = (int) floor(x);
			const int ycelli = (int) floor(y);
			const int zcelli = (int) floor(z);

			// calculate pseudorandom hashes for all the corners
			// we also hash the number of the octave, so the octaves will not be correlated
			hasher.reset() << which_octave << xcelli << ycelli << zcelli;
			const uint8 aaa = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli << ycelli + 1 << zcelli;
			const uint8 aba = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli << ycelli << zcelli + 1;
			const uint8 aab = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli << ycelli + 1 << zcelli + 1;
			const uint8 abb = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli << zcelli;
			const uint8 baa = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli + 1 << zcelli;
			const uint8 bba = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli << zcelli + 1;
			const uint8 bab = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli + 1 << zcelli + 1;
			const uint8 bbb = hasher.get() & 0xFF;

			// relative position in the cell
			const double xf = x - floor(x);
			const double yf = y - floor(y);
			const double zf = z - floor(z);

			// fade constants to smooth the solution
			const double u = fade(xf);
			const double v = fade(yf);
			const double w = fade(zf);

			// multiply the relative coordinate in the cell with the random gradient and lerp it together
			const double caa = lerp(
					grad3(aaa, xf, yf, zf), grad3(baa, xf - 1, yf, zf), u);
			const double cba = lerp(
					grad3(aba, xf, yf - 1, zf), grad3(bba, xf - 1, yf - 1, zf), u);
			const double cab = lerp(
					grad3(aab, xf, yf, zf - 1), grad3(bab, xf - 1, yf, zf - 1), u);
			const double cbb = lerp(
					grad3(abb, xf, yf - 1, zf - 1), grad3(bbb, xf - 1, yf - 1, zf - 1), u);
			const double cca = lerp(caa, cba, v);
			const double ccb = lerp(cab, cbb, v);
			buffer[index++] += lerp(cca, ccb, w) * amplitude;
		}
	}
}

void Perlin::perlin2(double sx, double sy, double dx, double dy,
	uint nx, uint ny, int which_octave, double amplitude, double *buffer)
{
	uint ix, iy;
	double x, y;

	// if there are at least 4 points in each cell
	if (dx < 0.5 && dy < 0.5) {
		int xcelln, ycelln;
		for (iy = 0, y = sy; iy < ny; iy += ycelln, y += ycelln * dy)
		for (ix = 0, x = sx; ix < nx; ix += xcelln, x += xcelln * dx) {
			// lowest corner of the cell, opposite corner have xi + 1 etc
			const int ycelli = (int) floor(y);
			const int xcelli = (int) floor(x);

			// calculate pseudorandom hashes for all the corners
			// we also hash the number of the octave, so the octaves will not be correlated
			hasher.reset() << which_octave << xcelli << ycelli;
			const uint8 aa = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli << ycelli + 1;
			const uint8 ab = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli;
			const uint8 ba = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xcelli + 1 << ycelli + 1;
			const uint8 bb = hasher.get() & 0xFF;

			// relative position in the cell
			const double xf = x - floor(x);
			const double yf = y - floor(y);
			
			// how many points are in this cell?
			xcelln = std::min(1 + (int) ((1.0 - xf) / dx), (int) (nx - ix));
			ycelln = std::min(1 + (int) ((1.0 - yf) / dy), (int) (ny - iy));

			int ixx, iyy;
			double xxf, yyf;
			for (iyy = 0, yyf = yf; iyy < ycelln; ++iyy, yyf += dy) {
				int index = (iyy + iy) * nx + ix;
				for (ixx = 0, xxf = xf; ixx < xcelln; ++ixx, ++index, xxf += dx) {
					const double u = fade(xxf);
					const double v = fade(yyf);
					const double ca = lerp(grad2(aa, xxf, yyf), grad2(ba, xxf - 1, yyf), u);
					const double cb = lerp(grad2(ab, xxf, yyf - 1), grad2(bb, xxf - 1, yyf - 1), u);
					buffer[index] += lerp(ca, cb, v) * amplitude;
				}
			}
		}
	} else {
		int index = 0;
		for (iy = 0, y = sy; iy < ny; iy++, y += dy)
		for (ix = 0, x = sx; ix < nx; ix++, x += dx) {
			// lowest corner of the cell, opposite corner have xi + 1 etc
			const int xi = (int) floor(x);
			const int yi = (int) floor(y);

			// relative position in the cell
			const double xf = x - floor(x);
			const double yf = y - floor(y);

			// fade constants to smooth the solution
			const double u = fade(xf);
			const double v = fade(yf);

			// calculate pseudorandom hashes for all the corners
			// we also hash the number of the octave, so the octaves will not be correlated
			hasher.reset() << which_octave << xi << yi;
			const uint8 aa = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xi << yi + 1;
			const uint8 ab = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xi + 1 << yi;
			const uint8 ba = hasher.get() & 0xFF;
			hasher.reset() << which_octave << xi + 1 << yi + 1;
			const uint8 bb = hasher.get() & 0xFF;

			// multiply the relative coordinate in the cell with the random gradient and lerp it together
			const double ca = lerp(grad2(aa, xf, yf), grad2(ba, xf - 1, yf), u);
			const double cb = lerp(grad2(ab, xf, yf - 1), grad2(bb, xf - 1, yf - 1), u);
			buffer[index++] += lerp(ca, cb, v) * amplitude;
		}
	}
}

double Perlin::grad3(uint8 hash, double x, double y, double z) {
	static const double lookup_table[0x100 * 3] = {
		// 256 pseudo-random vectors that were uniformly distributed on a unit sphere
		// computed via monte-carlo shooting inside of a volumetric sphere, then repeatedly
		// normalized and pushed from each other via a force modeled after coulombs force
		+0.98559246, +0.11364151, -0.12527218,
		-0.58847808, +0.46947497, -0.65824525,
		-0.27877906, -0.47497370, -0.83467492,
		+0.48064663, +0.01263201, -0.87682339,
		+0.27377059, -0.74678245, -0.60610695,
		+0.78744990, -0.08416656, -0.61060514,
		+0.43003221, +0.90174909, +0.04382788,
		-0.22382160, -0.89031502, -0.39653884,
		-0.67068651, -0.61276852, +0.41796453,
		+0.71762351, +0.13037618, +0.68411881,
		+0.50976131, +0.74418719, +0.43165824,
		-0.52190210, -0.84280455, -0.13152448,
		-0.79313370, -0.38271142, +0.47378361,
		-0.19147294, +0.85642052, +0.47946012,
		-0.60263575, +0.09735091, +0.79205615,
		-0.32750315, -0.92213410, -0.20593784,
		-0.96744828, -0.22537640, -0.11510563,
		+0.19668402, -0.59332566, -0.78056393,
		-0.62845192, +0.60579774, -0.48791114,
		-0.55437451, +0.34106112, +0.75917470,
		-0.50988956, -0.79348411, +0.33225834,
		-0.39233055, +0.54876488, -0.73819634,
		+0.73402796, -0.66406388, -0.14220450,
		-0.85621717, -0.13989681, +0.49731382,
		+0.94305573, -0.33191586, -0.02185742,
		+0.12719149, +0.41783813, +0.89957413,
		+0.71485573, -0.09999399, +0.69208561,
		+0.85068732, -0.02477169, +0.52508804,
		+0.39743281, -0.90130187, -0.17234297,
		-0.10627868, +0.95808664, -0.26603539,
		-0.76015576, +0.64165230, -0.10220348,
		-0.04208603, +0.87691027, -0.47880805,
		-0.26786159, +0.39572321, -0.87843799,
		+0.67373421, +0.05490263, -0.73693142,
		+0.55162126, -0.20133332, +0.80943121,
		-0.54585906, -0.13997086, -0.82610293,
		+0.83845430, +0.20403057, +0.50533743,
		+0.62197756, +0.39996998, -0.67317749,
		+0.15901006, +0.19860441, +0.96709466,
		+0.00145344, +0.74917102, -0.66237502,
		+0.81112392, -0.42485424, +0.40196624,
		+0.40154969, +0.73413931, -0.54753751,
		+0.42133292, +0.56329219, -0.71076050,
		-0.73088051, +0.22362709, +0.64482913,
		-0.86697838, +0.09896559, +0.48842022,
		-0.85736012, +0.51246892, +0.04805443,
		+0.99046476, +0.08991492, +0.10437848,
		-0.68652359, -0.72602564, -0.03965015,
		-0.48509486, +0.85485565, +0.18413257,
		-0.58515464, -0.13629883, +0.79938519,
		+0.22809148, -0.23313490, -0.94531603,
		+0.22702374, +0.66770451, -0.70896468,
		+0.11924988, -0.86888773, -0.48043062,
		-0.47278721, +0.79701563, -0.37581689,
		+0.43626642, -0.21100731, -0.87472711,
		+0.79052760, +0.42535624, +0.44061116,
		-0.81646750, -0.57368655, +0.06530363,
		-0.09534084, -0.82564831, -0.55607104,
		+0.63369581, -0.52542590, -0.56776513,
		-0.04761062, +0.39940652, -0.91553681,
		+0.75931164, +0.22001820, -0.61240332,
		+0.27787517, +0.55598620, +0.78336756,
		-0.53247879, -0.36110836, +0.76555019,
		-0.31304749, -0.45626917, +0.83295841,
		-0.86575402, +0.42686712, +0.26125550,
		+0.27227618, -0.96219378, +0.00698608,
		+0.34477399, +0.86641035, -0.36119802,
		-0.37594910, -0.03066356, -0.92613283,
		-0.48482445, +0.33045096, -0.80978233,
		+0.06069417, +0.99264837, -0.10471593,
		+0.50079807, +0.84635756, -0.18132892,
		-0.65491022, +0.70062651, -0.28322268,
		+0.50814431, -0.80857802, +0.29663267,
		-0.69038487, -0.23112382, -0.68552936,
		+0.68536464, +0.35626437, +0.63509921,
		-0.67722875, +0.24464709, -0.69390851,
		+0.08204649, -0.98247764, +0.16735013,
		+0.29474666, +0.82919345, +0.47493435,
		-0.95265633, -0.02425968, +0.30307983,
		-0.33849880, -0.75309836, -0.56414664,
		+0.23607534, -0.53362196, +0.81210592,
		-0.98555185, -0.13707440, +0.09948952,
		+0.55380579, -0.77537582, -0.30346580,
		-0.20167953, -0.10650625, +0.97364356,
		-0.88685502, +0.29397036, -0.35646822,
		-0.92820535, -0.15867312, -0.33653776,
		+0.97826198, -0.13863408, +0.15422093,
		+0.47114712, -0.54104331, +0.69662941,
		-0.02339252, +0.04081720, +0.99889276,
		+0.87715599, -0.19005397, -0.44099529,
		-0.13085245, -0.57957191, +0.80434696,
		-0.87232781, -0.37849765, -0.30948944,
		+0.61503141, +0.56357281, +0.55147262,
		-0.26382409, +0.85958061, -0.43762773,
		-0.93578442, +0.20578699, +0.28628522,
		+0.12563994, +0.93724217, -0.32525639,
		-0.75831571, +0.38796663, -0.52386943,
		+0.62394934, -0.16969953, -0.76281668,
		+0.80837996, -0.41288050, -0.41958495,
		+0.70184358, +0.62282065, +0.34570223,
		+0.18727155, -0.03990354, +0.98149736,
		-0.11585986, -0.92781935, +0.35458108,
		+0.49298083, +0.47266238, +0.73045204,
		-0.44411060, +0.53433958, +0.71919885,
		-0.17470486, +0.98395316, -0.03625446,
		+0.84473920, +0.46725072, -0.26094532,
		-0.19841411, -0.67107916, -0.71434207,
		-0.37700198, -0.24249843, +0.89390381,
		-0.43349807, -0.57134587, +0.69688114,
		+0.85351951, +0.52027635, -0.02858252,
		+0.05565725, +0.04949013, -0.99722264,
		+0.33074623, +0.19646435, -0.92304317,
		+0.04200521, -0.45435720, +0.88982869,
		-0.89083206, +0.42648936, -0.15660485,
		+0.85201122, +0.47429864, +0.22162509,
		+0.21555007, +0.97305283, +0.08189232,
		-0.89768126, -0.43122269, -0.09063860,
		-0.99281281, +0.08635899, +0.08285444,
		+0.33545455, -0.42332928, -0.84158337,
		-0.63528664, -0.71268528, -0.29747365,
		-0.92629250, -0.35765850, +0.11858579,
		-0.84087263, -0.08229535, -0.53493989,
		+0.09805833, -0.41222654, -0.90578907,
		-0.32836564, +0.91813874, -0.22180456,
		-0.30859133, +0.39390634, +0.86579974,
		-0.32791298, -0.24963751, -0.91112797,
		-0.82594411, +0.15239945, -0.54276213,
		+0.70111654, -0.61201298, -0.36589030,
		-0.59638915, +0.73042292, +0.33286984,
		-0.54793560, +0.10289866, -0.83016772,
		-0.78820222, +0.51804007, -0.33222244,
		+0.85482928, -0.47448731, -0.21006832,
		-0.80169859, -0.31074344, -0.51060541,
		+0.72239574, +0.67121028, -0.16619614,
		-0.33755890, +0.72569361, +0.59951878,
		-0.49315292, -0.36389639, -0.79017695,
		-0.53747441, +0.83127904, -0.14176185,
		+0.90746801, -0.19743061, +0.37084089,
		-0.63278757, -0.45212689, -0.62861846,
		-0.16872316, -0.05722284, -0.98400104,
		+0.72932785, -0.31449450, -0.60759699,
		+0.91756806, +0.25757512, +0.30285956,
		+0.12321120, +0.26082362, -0.95749154,
		-0.82103228, +0.33061526, +0.46540257,
		-0.45037767, -0.81100503, -0.37340434,
		-0.63797061, +0.76875846, +0.04476519,
		-0.78482849, -0.58625499, -0.20087142,
		+0.34396103, -0.84786740, -0.40349929,
		+0.67646732, -0.72746607, +0.11482632,
		+0.10107571, -0.91706595, +0.38571199,
		-0.43665806, +0.69335314, -0.57322871,
		-0.52892788, -0.84190215, +0.10693950,
		-0.08543905, -0.81703225, +0.57022668,
		-0.24923593, -0.70346030, +0.66560127,
		-0.17752668, +0.59205263, -0.78610302,
		+0.04301361, +0.61632103, +0.78631941,
		-0.10141986, +0.47010409, +0.87676459,
		+0.46911566, -0.87851444, +0.09023787,
		+0.05841205, +0.86202939, +0.50348124,
		+0.42273201, -0.58613282, -0.69119170,
		+0.90647348, +0.24144462, -0.34642506,
		-0.71170433, +0.00401796, -0.70246765,
		+0.36910744, +0.11523937, +0.92221450,
		-0.75060201, -0.01923686, +0.66047450,
		+0.21104165, +0.45602963, -0.86457990,
		+0.15771540, +0.94002103, +0.30246706,
		+0.54540684, -0.39214561, -0.74077878,
		+0.05157910, -0.67242038, +0.73837011,
		-0.23572488, +0.13621083, +0.96222679,
		+0.87168105, +0.07755300, -0.48389843,
		+0.57816013, +0.78617995, +0.21829329,
		-0.22421101, +0.74577559, -0.62733419,
		-0.73258539, -0.52807075, -0.42948799,
		-0.96658850, +0.20949117, -0.14771635,
		+0.37724808, +0.88685510, +0.26678065,
		+0.41100255, +0.38465989, -0.82650691,
		+0.91257892, -0.35847886, +0.19670440,
		+0.93820308, +0.32085390, -0.12972181,
		-0.82102313, -0.49597945, +0.28271081,
		-0.07326740, +0.94380667, +0.32227451,
		+0.40459931, -0.37496595, +0.83408629,
		+0.95425676, -0.01952937, -0.29834985,
		+0.04851850, -0.72666021, -0.68528162,
		+0.58524497, -0.80705086, -0.07846808,
		-0.65775110, +0.45623698, +0.59934240,
		-0.74519124, +0.63944386, +0.18921301,
		+0.21312155, -0.30216128, +0.92912742,
		+0.66126569, -0.66056375, +0.35550418,
		+0.58922763, +0.58467511, -0.55764310,
		-0.71502325, -0.25882861, +0.64942244,
		+0.06721538, -0.99550020, -0.06679408,
		-0.42381534, +0.21126698, +0.88076491,
		+0.99201057, -0.10590044, -0.06855747,
		-0.42988431, -0.57412352, -0.69683690,
		+0.04107731, +0.58217839, -0.81202277,
		+0.18529450, +0.82559945, -0.53296482,
		+0.94517802, +0.03402988, +0.32477757,
		+0.66092811, -0.52347802, +0.53772185,
		-0.33612871, +0.18406645, -0.92365417,
		-0.26186197, +0.94274990, +0.20652102,
		+0.79864659, -0.56002921, +0.22029731,
		+0.55484960, +0.22380529, -0.80128217,
		-0.12423274, -0.33466927, -0.93411065,
		+0.40749561, +0.66770544, +0.62299018,
		+0.37461481, -0.12964809, +0.91807141,
		+0.01967895, -0.17472304, -0.98442095,
		+0.29533115, -0.92393797, +0.24314222,
		+0.49747795, -0.70230806, -0.50919454,
		+0.62739575, -0.36514770, +0.68778029,
		-0.95017535, +0.30493001, +0.06468766,
		-0.38614478, +0.92243282, +0.00314528,
		-0.14997319, -0.33859571, +0.92890311,
		-0.93361792, +0.06752163, -0.35184997,
		-0.29990726, -0.82982947, +0.47057271,
		-0.99106816, -0.00958672, -0.13301128,
		+0.49542283, -0.69566299, +0.52020114,
		-0.72954022, +0.54927162, +0.40751903,
		-0.06724505, +0.27307063, +0.95964084,
		-0.91388560, -0.25696319, +0.31429767,
		+0.54457064, +0.25564481, +0.79880445,
		+0.55557553, +0.74655093, -0.36605674,
		+0.18039913, +0.73510751, +0.65350831,
		-0.22810158, +0.60958767, +0.75919203,
		-0.34079542, -0.93981539, +0.02460713,
		-0.63187308, -0.48983015, +0.60066866,
		+0.34423715, +0.34288140, +0.87403268,
		-0.41543202, -0.01868029, +0.90943240,
		+0.01975665, -0.20108665, +0.97937421,
		+0.26505639, -0.01505261, -0.96411541,
		+0.94573950, +0.31304907, +0.08704637,
		+0.13134899, -0.80659178, +0.57633076,
		+0.28757814, +0.94672699, -0.14493729,
		-0.01496491, +0.99299264, +0.11722486,
		-0.68564488, -0.69795616, +0.20675662,
		+0.32610772, -0.83080249, +0.45102216,
		-0.55501576, -0.65029179, -0.51872739,
		-0.12354183, +0.18460300, -0.97501751,
		-0.14020393, -0.98360632, -0.11340838,
		-0.08178329, +0.75259324, +0.65338725,
		+0.78109936, +0.39536104, -0.48329435,
		-0.13285912, -0.98333185, +0.12412467,
		+0.55705494, +0.02873670, +0.82997831,
		+0.71196944, +0.59156472, -0.37835262,
		+0.29990503, -0.68854617, +0.66027354,
		+0.78602202, -0.26690339, +0.55761273,
		-0.48099543, -0.69819367, +0.53025371,
		+0.84038510, -0.54187061, +0.01136343,
		+0.62249273, +0.78257711, -0.00871066,
		-0.05007772, -0.55570012, -0.82987324,
		-0.02577539, -0.94890178, -0.31451715,
		+0.75050790, +0.65057546, +0.11614412,
		+0.93559753, -0.25924332, -0.23968764,
		-0.32713357, -0.91067751, +0.25228974,
		+0.19183455, -0.94772226, -0.25499416,
		-0.39241823, +0.83205689, +0.39203222,
		-0.53894042, +0.65037643, +0.53530713,
	};
	// pick a random vector and multiply the relative location vector with it
	const double *gradient = lookup_table + (hash & 0xFF) * 3;
	return gradient[0] * x + gradient[1] * y + gradient[2] * z;
}

double Perlin::grad2(uint8 hash, double x, double y) {
	static const double lookup_table[0x100 * 2] = {
		// 256 pseudo-random vectors that were uniformly distributed on a unit circle
		// generating uniformly spaced vectors on a unit circle is trivial.  The values were
		// shuffled to increase appareant randomness, when hashes have low entropy
		+0.09801714, +0.99518473,
		-0.21910124, +0.97570213,
		-0.80320753, +0.59569930,
		+0.21910124, +0.97570213,
		-0.70710678, +0.70710678,
		+0.84485357, +0.53499762,
		-0.49289819, +0.87008699,
		+0.97003125, -0.24298018,
		+0.31368174, -0.94952818,
		-0.57580819, +0.81758481,
		+0.63439328, -0.77301045,
		+0.85772861, +0.51410274,
		+0.40524131, +0.91420976,
		-0.42755509, +0.90398929,
		+0.95694034, -0.29028468,
		-1.00000000, +0.00000000,
		+0.92387953, -0.38268343,
		+0.97570213, -0.21910124,
		+0.74095113, -0.67155895,
		-0.61523159, +0.78834643,
		+0.97003125, +0.24298018,
		-0.98078528, -0.19509032,
		-0.94952818, +0.31368174,
		-0.40524131, +0.91420976,
		+0.17096189, -0.98527764,
		-0.78834643, -0.61523159,
		+0.80320753, +0.59569930,
		+0.88192126, +0.47139674,
		+0.02454123, -0.99969882,
		+0.33688985, -0.94154407,
		-0.35989504, -0.93299280,
		-0.65317284, -0.75720885,
		-0.17096189, +0.98527764,
		+0.53499762, -0.84485357,
		-0.35989504, +0.93299280,
		-0.49289819, -0.87008699,
		-0.96377607, -0.26671276,
		+0.61523159, -0.78834643,
		-0.91420976, -0.40524131,
		+0.83146961, -0.55557023,
		+0.94952818, +0.31368174,
		+0.96377607, -0.26671276,
		+0.81758481, -0.57580819,
		+0.68954054, -0.72424708,
		-0.24298018, -0.97003125,
		+0.55557023, -0.83146961,
		+0.74095113, +0.67155895,
		+0.04906767, +0.99879546,
		-0.26671276, -0.96377607,
		+0.00000000, +1.00000000,
		-0.99969882, +0.02454123,
		+0.51410274, +0.85772861,
		-0.74095113, -0.67155895,
		+0.98917651, +0.14673047,
		+0.44961133, +0.89322430,
		-0.89322430, +0.44961133,
		+0.31368174, +0.94952818,
		-0.19509032, -0.98078528,
		+0.17096189, +0.98527764,
		+0.98078528, +0.19509032,
		-0.99729046, -0.07356456,
		-0.96377607, +0.26671276,
		-0.12241068, +0.99247953,
		+0.72424708, +0.68954054,
		-0.98917651, -0.14673047,
		-0.24298018, +0.97003125,
		-0.97003125, +0.24298018,
		-0.67155895, -0.74095113,
		+0.94952818, -0.31368174,
		+0.94154407, +0.33688985,
		-0.57580819, -0.81758481,
		+0.29028468, -0.95694034,
		-0.87008699, +0.49289819,
		+0.99969882, +0.02454123,
		+0.35989504, +0.93299280,
		-0.95694034, +0.29028468,
		-0.98917651, +0.14673047,
		-0.40524131, -0.91420976,
		-0.44961133, -0.89322430,
		-0.65317284, +0.75720885,
		-0.47139674, +0.88192126,
		+0.07356456, -0.99729046,
		+0.98078528, -0.19509032,
		+0.38268343, -0.92387953,
		-0.61523159, -0.78834643,
		+0.89322430, -0.44961133,
		+0.12241068, +0.99247953,
		+0.51410274, -0.85772861,
		-0.90398929, -0.42755509,
		-0.33688985, -0.94154407,
		-0.77301045, -0.63439328,
		+0.90398929, -0.42755509,
		-0.87008699, -0.49289819,
		+0.47139674, -0.88192126,
		-0.92387953, +0.38268343,
		-0.83146961, +0.55557023,
		-0.68954054, +0.72424708,
		+0.42755509, +0.90398929,
		-0.68954054, -0.72424708,
		-0.81758481, -0.57580819,
		-0.72424708, +0.68954054,
		+0.14673047, +0.98917651,
		+0.99969882, -0.02454123,
		+0.57580819, -0.81758481,
		+0.29028468, +0.95694034,
		-0.26671276, +0.96377607,
		+0.67155895, -0.74095113,
		+0.19509032, -0.98078528,
		+1.00000000, +0.00000000,
		-0.98527764, +0.17096189,
		-0.92387953, -0.38268343,
		+0.44961133, -0.89322430,
		-0.93299280, -0.35989504,
		-0.21910124, -0.97570213,
		+0.19509032, +0.98078528,
		+0.91420976, +0.40524131,
		+0.07356456, +0.99729046,
		-0.84485357, -0.53499762,
		+0.99729046, +0.07356456,
		-0.04906767, -0.99879546,
		+0.87008699, +0.49289819,
		-0.33688985, +0.94154407,
		-0.99518473, +0.09801714,
		-0.88192126, +0.47139674,
		+0.99729046, -0.07356456,
		-0.89322430, -0.44961133,
		+0.24298018, -0.97003125,
		-0.02454123, -0.99969882,
		+0.67155895, +0.74095113,
		-0.99518473, -0.09801714,
		-0.09801714, +0.99518473,
		-0.51410274, +0.85772861,
		+0.49289819, +0.87008699,
		+0.94154407, -0.33688985,
		+0.97570213, +0.21910124,
		-0.12241068, -0.99247953,
		-0.29028468, -0.95694034,
		+0.42755509, -0.90398929,
		-0.99879546, -0.04906767,
		-0.81758481, +0.57580819,
		-0.14673047, +0.98917651,
		+0.93299280, +0.35989504,
		+0.68954054, +0.72424708,
		+0.61523159, +0.78834643,
		+0.55557023, +0.83146961,
		+0.85772861, -0.51410274,
		-0.97003125, -0.24298018,
		+0.59569930, +0.80320753,
		-0.53499762, +0.84485357,
		+0.96377607, +0.26671276,
		+0.35989504, -0.93299280,
		+0.99518473, -0.09801714,
		+0.78834643, +0.61523159,
		+0.63439328, +0.77301045,
		+0.99879546, +0.04906767,
		+0.70710678, +0.70710678,
		+0.75720885, +0.65317284,
		-0.55557023, -0.83146961,
		-0.59569930, -0.80320753,
		-0.99879546, +0.04906767,
		+0.98917651, -0.14673047,
		-0.99247953, +0.12241068,
		+0.12241068, -0.99247953,
		-0.19509032, +0.98078528,
		-0.93299280, +0.35989504,
		+0.84485357, -0.53499762,
		-0.83146961, -0.55557023,
		-0.72424708, -0.68954054,
		-0.42755509, -0.90398929,
		+0.78834643, -0.61523159,
		-0.31368174, -0.94952818,
		+0.49289819, -0.87008699,
		-0.44961133, +0.89322430,
		+0.40524131, -0.91420976,
		+0.98527764, -0.17096189,
		-0.91420976, +0.40524131,
		-0.38268343, +0.92387953,
		-0.85772861, +0.51410274,
		+0.21910124, -0.97570213,
		-0.74095113, +0.67155895,
		-0.29028468, +0.95694034,
		+0.26671276, +0.96377607,
		-0.31368174, +0.94952818,
		-0.90398929, +0.42755509,
		+0.91420976, -0.40524131,
		+0.65317284, -0.75720885,
		+0.77301045, -0.63439328,
		+0.92387953, +0.38268343,
		+0.87008699, -0.49289819,
		-0.02454123, +0.99969882,
		-0.84485357, +0.53499762,
		+0.09801714, -0.99518473,
		+0.81758481, +0.57580819,
		-0.53499762, -0.84485357,
		+0.99247953, -0.12241068,
		-0.75720885, +0.65317284,
		-0.07356456, +0.99729046,
		+0.99247953, +0.12241068,
		-0.78834643, +0.61523159,
		+0.59569930, -0.80320753,
		+0.83146961, +0.55557023,
		+0.24298018, +0.97003125,
		-0.94154407, +0.33688985,
		-0.63439328, +0.77301045,
		-0.97570213, +0.21910124,
		+0.53499762, +0.84485357,
		-0.51410274, -0.85772861,
		-0.75720885, -0.65317284,
		+0.26671276, -0.96377607,
		-0.47139674, -0.88192126,
		-0.99729046, +0.07356456,
		+0.38268343, +0.92387953,
		-0.99247953, -0.12241068,
		-0.99969882, -0.02454123,
		+0.72424708, -0.68954054,
		+0.57580819, +0.81758481,
		-0.94154407, -0.33688985,
		+0.14673047, -0.98917651,
		-0.70710678, -0.70710678,
		-0.55557023, +0.83146961,
		-0.00000000, -1.00000000,
		+0.77301045, +0.63439328,
		+0.02454123, +0.99969882,
		-0.80320753, -0.59569930,
		-0.97570213, -0.21910124,
		+0.04906767, -0.99879546,
		-0.09801714, -0.99518473,
		-0.77301045, +0.63439328,
		+0.70710678, -0.70710678,
		-0.98078528, +0.19509032,
		-0.17096189, -0.98527764,
		+0.98527764, +0.17096189,
		+0.89322430, +0.44961133,
		+0.88192126, -0.47139674,
		-0.94952818, -0.31368174,
		-0.88192126, -0.47139674,
		-0.98527764, -0.17096189,
		-0.14673047, -0.98917651,
		+0.65317284, +0.75720885,
		-0.38268343, -0.92387953,
		+0.99518473, +0.09801714,
		-0.63439328, -0.77301045,
		+0.47139674, +0.88192126,
		+0.90398929, +0.42755509,
		-0.95694034, -0.29028468,
		-0.04906767, +0.99879546,
		+0.95694034, +0.29028468,
		+0.80320753, -0.59569930,
		-0.67155895, +0.74095113,
		-0.59569930, +0.80320753,
		-0.07356456, -0.99729046,
		+0.93299280, -0.35989504,
		+0.99879546, -0.04906767,
		-0.85772861, -0.51410274,
		+0.75720885, -0.65317284,
		+0.33688985, +0.94154407,
	};
	// pick a random vector and multiply the relative location vector with it
	const double *gradient = lookup_table + (hash & 0xFF) * 2;
	return gradient[0] * x + gradient[1] * y;
}

double Perlin::fade(double t) {
	// 6t^5 - 15t^4 + 10t^3
	// fifth order polynomial with f(0) = 0, f(1) = 1, f'(0) = 0 and f'(1) = 0
    return t * t * t * (t * (t * 6 - 15) + 10);
}

double Perlin::lerp(double a, double b, double x) {
	// linear interpolation
    return a + x * (b - a);
}
