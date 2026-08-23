#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Fetch texel color from diffuse texture
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec4 color = texelColor * colDiffuse * fragColor;

    // Discard transparent fragments (alpha cutout)
    // Discarding fragments prevents them from writing to the OpenGL depth buffer,
    // allowing geometry behind transparent areas to be properly drawn and visible.
    if (color.a < 0.1) {
        discard;
    }

    finalColor = color;
}
