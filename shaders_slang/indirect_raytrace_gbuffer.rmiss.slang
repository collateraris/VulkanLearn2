#include "gi_pathtrace.h"

[shader("miss")]
void main(inout IndirectGbufferRayPayload indirectRpl)
{    
    indirectRpl.position_objectID = float4(0.f, 0.f, 0.f, -1.f);
}