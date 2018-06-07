// Fragment shader
#version 150

in vec2 v_texcoord;

out vec4 frag_color;

uniform int u_render_mode;
uniform float u_ray_step;
uniform float u_threshold;
uniform float u_delta;
uniform sampler3D u_volumeTexture;
uniform sampler2D u_backFaceTexture;
uniform sampler2D u_frontFaceTexture;

uniform vec3 u_background_color;

uniform vec3 u_pos_light_pos;
uniform vec3 u_pos_light_col;
uniform vec3 u_ambient_color;
uniform vec3 u_diffuse_color;
uniform vec3 u_specular_color;
uniform float u_specular_power;

vec4 gradient_estimation(vec3 uvw, vec3 position) {
	vec3 sample1, sample2;
	float sample = texture(u_volumeTexture, uvw).x;
	sample1.x = texture(u_volumeTexture, uvw - vec3(u_delta, 0.0, 0.0)).x;
	sample2.x = texture(u_volumeTexture, uvw + vec3(u_delta, 0.0, 0.0)).x;
	sample1.y = texture(u_volumeTexture, uvw - vec3(0.0, u_delta, 0.0)).x;
	sample2.y = texture(u_volumeTexture, uvw + vec3(0.0, u_delta, 0.0)).x;
	sample1.z = texture(u_volumeTexture, uvw - vec3(0.0, 0.0, u_delta)).x;
	sample2.z = texture(u_volumeTexture, uvw + vec3(0.0, 0.0, u_delta)).x;
	vec3 normal = normalize(sample2 - sample1);
	vec3 light = normalize(u_pos_light_pos - position);
	vec3 view = normalize(-position);
	vec3 half_way = normalize(light + view);
	vec3 illumination_ambient = u_ambient_color;
	vec3 illumination_diffuse = u_diffuse_color * u_pos_light_col * max(dot(normal, light), 0);
	float specAngle = max(dot(normal, half_way), 0.0);
	vec3 illumination_specular = u_specular_color * u_pos_light_col * pow(specAngle, u_specular_power);
	vec3 result = vec3(0.0);
    result += illumination_ambient;
	result += illumination_diffuse;
	result += illumination_specular;
    result = pow(result, vec3(1.0/2.2)); // sRGB gamma correction
	vec4 color = vec4(result, 1.0);
	return color;
}

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
	
	vec3 step = ray_direction.xyz * u_ray_step;
	
	if (u_render_mode == 0) {
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
		if (intensity > 0.0) {
			color.rgb = vec3(intensity);
		} else {
			color.rgb = u_background_color;
		}
		color.a = 1.0;
	} else if (u_render_mode == 1) {
		// Iso-surface
		float value = 0.0;

		while (length(ray_position - ray_start) < ray_direction.a) {
			value = texture(u_volumeTexture, ray_position).r;
			if (value >= u_threshold) {
				color = gradient_estimation(ray_position, ray_direction.xyz);
				break;
			}
			ray_position += step;
		}
	}
	frag_color = color;
}
