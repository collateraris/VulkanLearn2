#include "gi_pathtrace.h"

[shader("miss")]
void main(inout ShadowHitInfo payload)
{
	// For shadow rays, miss means that light is unoccluded. Note that there's no closest hit shader for shadows. That's because we don't need to know details of the closest hit, just whether any opaque hit occured or not.
	payload.hasHit = false;
}