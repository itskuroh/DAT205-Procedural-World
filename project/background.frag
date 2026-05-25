#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;

in vec2 texCoord;
in vec3 viewDir;

uniform vec3 lightPosition;
uniform vec3 camera_pos;
uniform float environment_multiplier;
#define PI 3.14159265359

// atm constants in unit sphere
const float earthRadius      = 1.0;
const float atmosphereRadius = 1.1;        // 10% thicker shell — more forgiving

const vec3 rayleighCoeff = vec3(11.6e-3, 27.0e-3, 66.2e-3);
const float rayleighScaleH   = 0.08;
const float mieCoeff         = 21e-3;
const float mieScaleH        = 0.012;
const float mieG             = 0.758;

const int viewSamples  = 16;
const int lightSamples = 8;

// Finds how far along a ray (from rayOrigin in rayDir) it exits a sphere of given radius.
// Returns -1 if no intersection.
vec2 raySphereIntersect(vec3 ro, vec3 rd, float radius) {
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, ro);
    float c = dot(ro, ro) - radius * radius;
    float disc = b * b - 4.0 * a * c;
    if (disc < 0.0) return vec2(-1.0);
    float s = sqrt(disc);
    return vec2((-b - s) / (2.0 * a),
                (-b + s) / (2.0 * a));
}

// Rayleigh phase function — how much light scatters toward the viewer
float rayleighPhase(float cosTheta) {
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

// Mie phase function (Henyey-Greenstein) — forward-peaked glow around sun
float miePhase(float cosTheta) {
    float g2 = mieG * mieG;
    return (3.0 / (8.0 * PI)) *
           ((1.0 - g2) * (1.0 + cosTheta * cosTheta)) /
           ((2.0 + g2) * pow(1.0 + g2 - 2.0 * mieG * cosTheta, 1.5));
}

vec3 atmosphere(vec3 rayDirIn, vec3 sunDir) {
    // Clamp below-horizon rays to just above horizon so they still get sky color
    vec3 rayDir = rayDirIn.y < 0.0
        ? normalize(vec3(rayDirIn.x, 0.0001, rayDirIn.z))
        : rayDirIn;

    vec3 rayOrigin = vec3(0.0, earthRadius + 0.0001, 0.0);

    vec2 tAtmo = raySphereIntersect(rayOrigin, rayDir, atmosphereRadius);
    if (tAtmo.y < 0.0) return vec3(0.0, 0.0, 0.02); // fallback deep blue

    float tMin = max(tAtmo.x, 0.0);
    float tMax = tAtmo.y;

    // Don't clip against earth — we already clamped rayDir above horizon
    float stepSize = (tMax - tMin) / float(viewSamples);
    if (stepSize <= 0.0) return vec3(0.0, 0.0, 0.02);

    vec3  rayleighAccum = vec3(0.0);
    vec3  mieAccum      = vec3(0.0);
    float rayleighDepth = 0.0;
    float mieDepth      = 0.0;

    for (int i = 0; i < viewSamples; i++) {
        vec3  samplePos = rayOrigin + rayDir * (tMin + (float(i) + 0.5) * stepSize);
        float sampleAlt = max(length(samplePos) - earthRadius, 0.0); // never negative

        float rayleighDens = exp(-sampleAlt / rayleighScaleH) * stepSize;
        float mieDens      = exp(-sampleAlt / mieScaleH)      * stepSize;

        rayleighDepth += rayleighDens;
        mieDepth      += mieDens;

        vec2  tSun    = raySphereIntersect(samplePos, sunDir, atmosphereRadius);
        float lightStep = max(tSun.y, 0.0) / float(lightSamples);

        float lightRayleigh = 0.0, lightMie = 0.0;
        for (int j = 0; j < lightSamples; j++) {
            vec3  lp  = samplePos + sunDir * (float(j) + 0.5) * lightStep;
            float alt = max(length(lp) - earthRadius, 0.0);
            lightRayleigh += exp(-alt / rayleighScaleH) * lightStep;
            lightMie      += exp(-alt / mieScaleH)      * lightStep;
        }

        vec3 transmittance = exp(-(
            rayleighCoeff * (rayleighDepth + lightRayleigh) +
            mieCoeff      * (mieDepth + lightMie) * 1.1
        ));

        rayleighAccum += rayleighDens * transmittance;
        mieAccum      += mieDens      * transmittance;
    }

    float cosTheta     = dot(rayDir, sunDir);
    float sunIntensity = 150.0 * max(sunDir.y, 0.0) + 25.0;   //1st value = midday brightness, 2nd = dawn/dusk

    return sunIntensity * (
        rayleighPhase(cosTheta) * rayleighCoeff * rayleighAccum +
        miePhase(cosTheta)      * mieCoeff      * mieAccum
    );
}

void main()
{
    vec3 dir    = normalize(viewDir);
    vec3 sunDir = normalize(lightPosition);  // already unit from drawBackground

    // --- Sky ---
    vec3 color = atmosphere(dir, sunDir);

    // --- Sun disk ---
    float cosToSun = dot(dir, sunDir);
    float sunDisk   = smoothstep(0.9997, 0.9999, cosToSun);
    // Sun color shifts redder near horizon (more atmosphere = more Rayleigh filtering)
    float horizonBlend = 1.0 - max(sunDir.y, 0.0);
    vec3  sunColor     = mix(vec3(1.0, 0.98, 0.9), vec3(1.0, 0.4, 0.1), pow(horizonBlend, 3.0));
    color += sunDisk * sunColor * 3.0;

    // --- Moon disk (opposite of sun) ---
    float cosToMoon = dot(dir, -sunDir);
    float moonDisk  = smoothstep(0.9992, 0.9995, cosToMoon);
    // Moon is only visible at night — fade it out during the day
    float nightFactor = smoothstep(0.1, -0.15, sunDir.y);
    color += moonDisk * vec3(0.85, 0.88, 1.0) * nightFactor * 2.0;

    // --- Stars (only at night, random hash pattern) ---
    // Simple star field using the view direction as a hash seed
    if (sunDir.y < 0.1) {
        vec3 absDir = abs(dir);
        float starSeed = fract(sin(dot(floor(dir * 200.0), vec3(127.1, 311.7, 74.3))) * 43758.5);
        float star = pow(starSeed, 280.0) * 6.0;
        star *= smoothstep(0.15, -0.05, sunDir.y); // fade in as sun sets
        star *= max(dir.y, 0.0);                    // no stars below horizon
        color += vec3(0.9, 0.9, 1.0) * star;
    }

    // Tone map: the raw scattering values can exceed 1.0, compress them naturally
    color = 1.0 - exp(-3 * color);    //2nd value = exposure. higher = more light passes through

    // Subtle gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    fragmentColor = vec4(color, 1.0);
}