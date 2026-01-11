#ifndef ALPHA_CUTOUT_THRESHOLD
#define ALPHA_CUTOUT_THRESHOLD 0.5
#endif

#include "common/example.glsl"

uniform sampler2D u_Texture;

void main() {
    vec4 color = texture(u_Texture, fIn.texCoords);

#ifdef USE_ALPHA_CUTOUT
    if (color.a < ALPHA_CUTOUT_THRESHOLD) {
        discard;
    }
#endif

    out_Color = color;
}