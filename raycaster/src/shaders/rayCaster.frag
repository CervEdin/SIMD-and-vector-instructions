// Fragment shader
#version 150

in vec2 v_texcoord;

out vec4 frag_color;

uniform sampler3D u_volumeTexture;
uniform sampler2D u_backFaceTexture;
uniform sampler2D u_frontFaceTexture;

void main()
{
    // 3. (in the ray-casting fragment shader) subtract the backface texture from the frontface texture to obtain ray direction vectors.
    // 4. Given the ray starting points and direction vectors, cast (still in the fragment shader) a ray from each fragment into the volume image texture (Fig. 4).
    // Figure 4: Basic idea of ray-casting. A ray is cast from each pixel/fragment in the viewport through the 3D volume image. The rays enters the volume at the frontfaces of the bounding box and sample (look up) the voxel intensity values at even intervals until they hit the backfaces of the bounding box. Each fragment in the viewport can then be colored according to the voxel intensity values found along the corresponding ray.
    vec4 color = vec4(0.0);

    color.rg = v_texcoord;
    color.a = 1.0;

    frag_color = color;
}
