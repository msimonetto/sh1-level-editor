#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;

const int indexMatrix4x4[16] = int[](
    0,  8,  2, 10,
    12, 4, 14, 6,
    3, 11,  1, 9,
    15, 7, 13, 5
);

void main() {
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec3 color = texelColor.rgb;
    
    // Calculate Bayer matrix coordinates
    int x = int(gl_FragCoord.x) % 4;
    int y = int(gl_FragCoord.y) % 4;
    
    // Normalize matrix value to -0.5 .. 0.5
    float ditherValue = float(indexMatrix4x4[x + y * 4]) / 16.0 - 0.5;
    
    // PS1 color depth is roughly 15-bit (5 bits per channel = 32 steps)
    float colorSteps = 32.0;
    
    // Apply dither offset
    color = color + (ditherValue * (1.0 / colorSteps));
    
    // Quantize color
    color = floor(color * colorSteps) / colorSteps;
    
    finalColor = vec4(color, texelColor.a);
}
