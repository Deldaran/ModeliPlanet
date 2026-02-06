shader_type spatial;
render_mode unshaded, cull_front, depth_draw_never; // Important: ne pas ecrire dans le Z-buffer pour permettre la transparence

// --- UNIFORMS ---
uniform float planet_radius = 6371.0;
uniform float cloud_min_height = 2000.0;
uniform float cloud_max_height = 4000.0;
uniform sampler3D noise_texture : hint_default_white;
uniform float time_scale = 1.0;
uniform vec3 planet_center_offset = vec3(0.0); // Offset relative to Node position

// Light logic references Godot's built-in light if possible, or manual direction
uniform vec3 sun_direction = vec3(0.5, 0.5, 0.0);
uniform vec3 sun_color = vec3(1.0, 0.95, 0.85);

// Constants
const int STEPS = 64;

// Varyings passed from Vertex
varying vec3 v_obj_pos; // Object Space Position
varying vec3 v_world_pos;
varying vec3 v_cam_pos_local; // Camera Position in Object Space

void vertex() {
    v_obj_pos = VERTEX;
    v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    // Calculate Camera pos in Local space for easier raymath
    v_cam_pos_local = (inverse(MODEL_MATRIX) * vec4(CAMERA_POSITION_WORLD, 1.0)).xyz;
}

// --- UTILS ---
float remap(float v, float minOld, float maxOld, float minNew, float maxNew) {
    return minNew + (v - minOld) * (maxNew - minNew) / (maxOld - minOld);
}

vec2 intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return vec2(-1.0);
    h = sqrt(h);
    return vec2(-b - h, -b + h);
}

float getDensityForCloud(float weather, float hFrac) {
    float stratusHeight = smoothstep(0.0, 0.1, hFrac) * smoothstep(0.3, 0.2, hFrac);
    float cumulusHeight = smoothstep(0.1, 0.2, hFrac) * smoothstep(0.9, 0.6, hFrac);
    return mix(stratusHeight, cumulusHeight, smoothstep(0.4, 0.7, weather));
}

// --- FLUID DOMAIN WARPING ---
vec3 getFluidPos(vec3 p, float scale, float warpStrength, float timeSpeed) {
    // Coordinate distortion
    vec3 flowCoord = p * (scale * 0.5) + vec3(TIME * timeSpeed, TIME * timeSpeed * 0.5, -TIME * timeSpeed * 0.3);
    
    // Texture lookup (Requires a 3D Noise Texture setup in Godot Material)
    vec4 noiseVal = texture(noise_texture, flowCoord);
    
    vec3 flowDir = noiseVal.rgb * 2.0 - 1.0;
    vec3 warpedP = p + (flowDir * warpStrength);
    
    return warpedP * scale; 
}

float sampleCloudDensity(vec3 p) {
    float len = length(p); // Assuming Planet Center is (0,0,0) in Local Space
    float hFrac = (len - (planet_radius + cloud_min_height)) / (cloud_max_height - cloud_min_height);
    if(hFrac < 0.0 || hFrac > 1.0) return 0.0;

    // --- A. WEATHER MAP ---
    vec3 weatherPos = getFluidPos(p, 0.00004, 2000.0, 0.05); // Speed adjusted for Godot TIME
    float bigWeather = texture(noise_texture, weatherPos).r;
    
    // Echelle 2 : Breakup
    vec3 breakupPos = getFluidPos(p, 0.00015, 800.0, 0.2);
    float breakupNoise = texture(noise_texture, breakupPos).g; 
    
    float compositeWeather = bigWeather - (breakupNoise * 0.30);
    float weatherCoverage = smoothstep(0.25, 0.85, compositeWeather);
    weatherCoverage = clamp(weatherCoverage * 1.35, 0.0, 1.0);
    
    if (weatherCoverage < 0.01) return 0.0;

    // --- B. SHAPE ---
    vec3 shapePos = getFluidPos(p, 0.0002, 400.0, 0.3);
    vec4 shapeSample = texture(noise_texture, shapePos);
    float shapeFBM = shapeSample.g * 0.625 + shapeSample.b * 0.25 + shapeSample.a * 0.125;

    float baseCloud = remap(shapeFBM, 1.0 - weatherCoverage, 1.0, 0.0, 1.0);
    if (baseCloud <= 0.0) return 0.0; 
    
    float densityHeightParams = getDensityForCloud(weatherCoverage, hFrac);
    baseCloud *= densityHeightParams;

    // --- C. EROSION ---
    vec3 detailPos = getFluidPos(p, 0.002, 50.0, 0.5); 
    vec3 detailNoise = texture(noise_texture, detailPos).gba;
    float highFreq = detailNoise.r * 0.625 + detailNoise.g * 0.25 + detailNoise.b * 0.125;
    
    float erosionModifier = mix(highFreq, 1.0 - highFreq, clamp(hFrac, 0.0, 1.0));
    float finalCloud = remap(baseCloud, erosionModifier * 0.25, 1.0, 0.0, 1.0);
    
    return clamp(finalCloud * 6.0, 0.0, 1.0);
}

float henyeyGreenstein(float g1, float g2, float mixFactor, float cosTheta) {
    float g1_2 = g1 * g1;
    float g2_2 = g2 * g2;
    float forward = (1.0 - g1_2) / pow(1.0 + g1_2 - 2.0 * g1 * cosTheta, 1.5);
    float backward = (1.0 - g2_2) / pow(1.0 + g2_2 - 2.0 * g2 * cosTheta, 1.5);
    return mix(forward, backward, mixFactor) / 4.0 * 3.14159;
}

float getLight(vec3 p, vec3 lightDir) {
    // Simple fast lighting for now
    float totalDens = 0.0;
    vec3 shadowP = p;
    float stepL = 50.0; // Larger step for lighting
    
    for (int i=0; i<4; i++) {
        shadowP += lightDir * stepL;
        totalDens += sampleCloudDensity(shadowP) * stepL; 
        stepL *= 2.0; 
    }
    
    float beer = exp(-totalDens * 0.05);
    float powder = 1.0 - exp(-totalDens * 0.1);
    return mix(beer, beer * powder + 0.2 * beer, 0.5); 
}

void fragment() {
    // In Godot Spatial shaders, fragment() operates on pixels of the mesh surface.
    // To do volumetric raymarching for a planet, we typically render a Cube or Sphere containing the volume.
    // We raymarch from the Camera Position (converted to Local Space) into the direction of the pixel.
    
    vec3 ro = v_cam_pos_local;
    vec3 rayDir = normalize(v_obj_pos - ro);
    
    vec3 planet_center_local = vec3(0.0);
    float minR = planet_radius + cloud_min_height;
    float maxR = planet_radius + cloud_max_height;
    
    // Intersections (Local Space)
    vec2 hitOut = intersectSphere(ro, rayDir, planet_center_local, maxR);
    float tNear = hitOut.x;
    float tFar = hitOut.y;
    
    if (tFar < 0.0) discard;
    
    float tStart = max(0.0, tNear);
    float tEnd = tFar;
    
    // Occlusion by planet inner solid sphere
    vec2 hitSolid = intersectSphere(ro, rayDir, planet_center_local, planet_radius);
    if (hitSolid.x > 0.0 && hitSolid.x < tEnd) {
         tEnd = min(tEnd, hitSolid.x);
    }
    
    // Camera inside clouds logic
    if (length(ro) > minR && length(ro) < maxR) {
        tStart = 0.0;
    }
    
    if (tEnd <= tStart) discard;
    
    // Raymarch
    int steps = STEPS;
    float stepSize = (tEnd - tStart) / float(steps);
    vec3 p = ro + rayDir * tStart;
    
    vec3 lightDir = normalize(sun_direction); // Should use Godot LIGHT builtin but simplified here
    float cosTheta = dot(rayDir, lightDir);
    float phase = henyeyGreenstein(0.6, -0.3, 0.7, cosTheta);
    
    vec3 sum = vec3(0.0);
    float transmittance = 1.0;
    
    vec3 ambientHigh = vec3(0.3, 0.4, 0.6); 
    vec3 ambientLow  = vec3(0.6, 0.55, 0.5);

    for(int i=0; i<steps; i++) {
        if(transmittance < 0.01) break;
        
        float density = sampleCloudDensity(p);
        
        if(density > 0.001) {
            float h = (length(p) - minR) / (maxR - minR);
            float lightTrans = getLight(p, lightDir);
            vec3 ambient = mix(ambientLow, ambientHigh, h);
            
            // Just basic day lighting
            float sunDot = dot(normalize(p), lightDir);
            float dayFactor = smoothstep(-0.2, 0.1, sunDot);
            ambient *= dayFactor;
            
            vec3 scattering = sun_color * lightTrans * phase + ambient * 0.4;
            float alpha = 1.0 - exp(-density * stepSize * 0.1);
            
            sum += scattering * alpha * transmittance;
            transmittance *= (1.0 - alpha);
        }
        p += rayDir * stepSize;
    }
    
    if (transmittance >= 0.99) discard;
    
    ALBEDO = sum;
    ALPHA = 1.0 - transmittance;
}
