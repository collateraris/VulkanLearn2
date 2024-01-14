vec4 updateReservoir(uint randSeed, vec4 reservoir, uint lightToSample, float weight) {
	reservoir.x = reservoir.x + weight; // r.w_sum
	reservoir.z = reservoir.z + 1.0f; // r.M
	if (nextRand(randSeed) < weight / reservoir.x) {
		reservoir.y = float(lightToSample); // r.y
	}

	return reservoir;
}

