// Fragment shader
#version 150

in vec2 v_texcoord;

out vec4 frag_color;

uniform sampler3D u_volumeTexture;
uniform sampler2D u_backFaceTexture;
uniform sampler2D u_frontFaceTexture;



void main()
{
    // Subtract the backface texture from the frontface texture to obtain ray direction vectors.
    vec3 ray_start = texture(u_frontFaceTexture, v_texcoord).xyz;
    vec3 ray_end = texture(u_backFaceTexture, v_texcoord).xyz;
    vec3 difference = ray_end - ray_start;
    if (difference == 0.0) {
    	discard;
    }
    vec4 ray_direction = vec4(normalize(difference), length(difference)); //normalize?
    vec3 ray_position = ray_start;
    vec4 color = vec4(0.0);

    // Given the ray starting points and direction vectors, a ray is cast from each pixel/fragment in the viewport through the 3D volume image. The rays enters the volume at the frontfaces of the bounding box and sample (look up) the voxel intensity values at even intervals until they hit the backfaces of the bounding box. Each fragment in the viewport can then be colored according to the voxel intensity values found along the corresponding ray.
    
    vec3 step = ray_direction.xyz * 0.001;
    
    // Maximum Intensity Projection (MIP) 
    float intensity = 0.0;
    float value = 0.0;

    while (length(ray_position - ray_start) < ray_direction.a) {
    	value = texture(u_volumeTexture, ray_position).r;
    	if (value > intensity) {
    		intensity = value;
    	}
    	ray_position += step;
    }



    color.rgb = vec3(intensity);

    color.a = 1.0;

    frag_color = color;
}
