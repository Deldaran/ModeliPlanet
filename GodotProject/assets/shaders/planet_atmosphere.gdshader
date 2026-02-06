shader_type spatial;
render_mode unshaded, cull_front, depth_draw_never; // Transparence totale sans ecriture dans le Z-Buffer

// --- UNIFORMS ---
uniform float inner_radius = 6371.0;
uniform float outer_radius = 6471.0;
uniform vec3 planet_center = vec3(0.0);

uniform vec3 sky_blue_col : source_color = vec3(0.3, 0.5, 0.9);
uniform vec3 sunset_col : source_color = vec3(1.0, 0.4, 0.1);
uniform vec3 sun_direction = vec3(0.5, 0.5, 0.0); // Should be global param

// Varyings
varying vec3 v_world_pos;
varying vec3 v_normal;

void vertex() {
    v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    v_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

void fragment() {
    vec3 view_pos = CAMERA_POSITION_WORLD;
    vec3 ray_dir = normalize(v_world_pos - view_pos);
    vec3 light_dir = normalize(sun_direction);
    
    vec3 N = normalize(v_normal);
    // Not actually used much because we do view dependent fresnel mainly
    
    // --- DISTANCES ---
    float dist_cam_center = length(view_pos - planet_center);
    bool is_inside = dist_cam_center < (outer_radius - 10.0);
    
    vec3 final_color = vec3(0.0);
    float alpha = 0.0;
    
    float dot_sun_normal = dot(normalize(v_world_pos - planet_center), light_dir);
    float day_factor = smoothstep(-0.25, 0.25, dot_sun_normal);

    // --- RENDER LOGIC ---
    
    if (is_inside) {
        // Looking up at sky
        // Simple gradient based on view angle up
        // "Up" is normal from planet center through camera
        vec3 up = normalize(view_pos - planet_center);
        float view_elev = dot(ray_dir, up); // 1.0 = zenith, 0.0 = horizon
        
        vec3 zenith_col = vec3(0.1, 0.4, 0.8);
        vec3 horizon_col = vec3(0.6, 0.8, 0.9);
        
        if (view_elev > -0.1) {
             float t = pow(max(view_elev, 0.0), 0.5);
             final_color = mix(horizon_col, zenith_col, t);
             
             // Density fades as we look up (less air)
             // But actually we just want a transparent skybox overlay
             alpha = (0.2 + 0.6 * (1.0 - t)); // More opaque at horizon
        } else {
             // Looking down at ground -> thin atmosphere
             alpha = 0.0;
        }
    } else {
        // Space View
        // Fresnel Effect at edges
        // Angle between Normal (pointing out) and -RayDir (pointing in)
        // At center of planet: Normal // -RayDir -> dot = 1 -> fresnel = 0
        // At edge: Normal perp RayDir -> dot = 0 -> fresnel = 1
        
        // We need normal of the sphere at current fragment
        vec3 surf_normal = normalize(v_world_pos - planet_center);
        float mu = dot(surf_normal, -ray_dir);
        
        // Standard Fresnel Rim
        float fresnel = pow(1.0 - max(mu, 0.0), 4.0);
        
        final_color = sky_blue_col;
        alpha = fresnel * 2.5;
        
        // Fade center to see ground
        alpha = clamp(alpha, 0.0, 1.0);
    }
    
    // Sunset
    // Based on light direction vs position
    // If we are near terminator (dot_sun_normal ~ 0), redden
    if (abs(dot_sun_normal) < 0.2) {
         final_color = mix(final_color, sunset_col, 0.6);
    }
    
    // Sun Halo (Mie Scattering approx)
    float sun_view_check = dot(ray_dir, light_dir);
    float sun_halo = pow(max(sun_view_check, 0.0), 50.0);
    final_color += vec3(1.0, 0.95, 0.8) * sun_halo;

    // Apply Night/Day fade
    // At night (day_factor 0), sky is dark transparent
    // alpha *= (0.2 + 0.8 * day_factor); 
    // Actually keep rim visible slightly at night looks cool, but let's fade it
    alpha *= (0.5 + 0.5 * day_factor);
    final_color *= (0.2 + 0.8 * day_factor);

    ALBEDO = final_color;
    ALPHA = alpha;
}
