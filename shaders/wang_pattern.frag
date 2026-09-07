#version 430 core

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outDiagnostics;
layout(location = 2) out vec4 outLinearColor;

struct TileData {
    uvec4 edges;
    vec4 phaseA;
    vec4 phaseB;
    ivec4 frequencyU;
    ivec4 frequencyV;
};

layout(std430, binding = 0) readonly buffer TileBuffer {
    TileData tiles[];
};

uniform ivec2 u_gridSize;
uniform vec2 u_worldOrigin;
uniform float u_pixelsPerTile;
uniform int u_debugView;

uniform int u_edgeCount;
uniform vec2 u_edgeParameters[16];

uniform int u_fourierCount;
uniform ivec2 u_fourierFrequency[16];
uniform float u_fourierAmplitude[16];
uniform float u_fourierPhase[16];
uniform float u_fourierNormalizer;

uniform sampler2D u_gradientTexture;
uniform vec4 u_noiseSettings;

uniform int u_colorStopCount;
uniform float u_colorStopPosition[8];
uniform vec3 u_colorStopValue[8];
uniform vec4 u_toneSettings;

uniform vec4 u_generatorWeights;
uniform vec4 u_worldSettings;

const float pi = 3.14159265358979323846;
const float tau = 6.28318530717958647692;

float edgeValue(uint colorIndex, float parameter) {
    vec2 coefficients = u_edgeParameters[min(colorIndex, uint(u_edgeCount - 1))];
    return parameter
        + coefficients.x * sin(pi * parameter)
        + coefficients.y * sin(tau * parameter);
}

float edgeDerivative(uint colorIndex, float parameter) {
    vec2 coefficients = u_edgeParameters[min(colorIndex, uint(u_edgeCount - 1))];
    return 1.0
        + pi * coefficients.x * cos(pi * parameter)
        + tau * coefficients.y * cos(tau * parameter);
}

vec2 coonsMap(vec2 parameter, uvec4 edges) {
    float south = edgeValue(edges.x, parameter.x);
    float north = edgeValue(edges.y, parameter.x);
    float west = edgeValue(edges.z, parameter.y);
    float east = edgeValue(edges.w, parameter.y);
    return vec2(
        mix(south, north, parameter.y),
        mix(west, east, parameter.x)
    );
}

// (dX/du, dX/dv, dY/du, dY/dv)，顺序与 CPU Jacobian2 完全一致。
vec4 coonsJacobian(vec2 parameter, uvec4 edges) {
    float south = edgeValue(edges.x, parameter.x);
    float north = edgeValue(edges.y, parameter.x);
    float west = edgeValue(edges.z, parameter.y);
    float east = edgeValue(edges.w, parameter.y);
    return vec4(
        mix(
            edgeDerivative(edges.x, parameter.x),
            edgeDerivative(edges.y, parameter.x),
            parameter.y),
        north - south,
        east - west,
        mix(
            edgeDerivative(edges.z, parameter.y),
            edgeDerivative(edges.w, parameter.y),
            parameter.x)
    );
}

bool inverseCoons(
    vec2 physical,
    uvec4 edges,
    out vec2 parameter,
    out float residualNorm,
    out float minimumDeterminant,
    out int iterations) {
    parameter = physical;
    residualNorm = 1.0 / 0.0;
    minimumDeterminant = 1.0 / 0.0;
    iterations = 0;

    for (int iteration = 0; iteration < 8; ++iteration) {
        vec2 residual = physical - coonsMap(parameter, edges);
        residualNorm = length(residual);
        vec4 jacobian = coonsJacobian(parameter, edges);
        float determinant = jacobian.x * jacobian.w - jacobian.y * jacobian.z;
        minimumDeterminant = min(minimumDeterminant, determinant);
        iterations = iteration;
        if (isnan(determinant) || isinf(determinant) || determinant <= 1.0e-6) {
            return false;
        }
        if (residualNorm <= 5.0e-7) {
            return true;
        }

        vec2 delta = vec2(
            (residual.x * jacobian.w - jacobian.y * residual.y) / determinant,
            (jacobian.x * residual.y - residual.x * jacobian.z) / determinant
        );
        parameter = clamp(parameter + delta, vec2(0.0), vec2(1.0));
    }

    residualNorm = length(physical - coonsMap(parameter, edges));
    vec4 jacobian = coonsJacobian(parameter, edges);
    minimumDeterminant = min(
        minimumDeterminant,
        jacobian.x * jacobian.w - jacobian.y * jacobian.z);
    iterations = 8;
    return residualNorm <= 2.0e-5;
}

float quinticFade(float value) {
    return value * value * value * (10.0 + value * (-15.0 + 6.0 * value));
}

float boundaryWindow(vec2 parameter) {
    const float fadeWidth = 0.18;
    vec4 distance = clamp(
        vec4(parameter, vec2(1.0) - parameter) / fadeWidth,
        vec4(0.0),
        vec4(1.0));
    vec4 faded = distance * distance * distance
        * (vec4(10.0) + distance * (vec4(-15.0) + 6.0 * distance));
    return faded.x * faded.y * faded.z * faded.w;
}

float evaluateFourier(vec2 parameter) {
    float value = 0.0;
    for (int index = 0; index < 16; ++index) {
        if (index >= u_fourierCount) {
            break;
        }
        value += u_fourierAmplitude[index] * cos(
            tau * dot(vec2(u_fourierFrequency[index]), parameter)
            + u_fourierPhase[index]);
    }
    return value / u_fourierNormalizer;
}

int positiveModulo(int value, int period) {
    int remainder = value % period;
    return remainder < 0 ? remainder + period : remainder;
}

vec2 gradientAt(ivec2 coordinate, int period) {
    ivec2 wrapped = ivec2(
        positiveModulo(coordinate.x, period),
        positiveModulo(coordinate.y, period));
    return texelFetch(u_gradientTexture, wrapped, 0).rg;
}

float evaluateNoiseOctave(vec2 parameter, int frequency) {
    vec2 scaled = parameter * float(frequency);
    ivec2 cell = ivec2(floor(scaled));
    vec2 local = fract(scaled);
    vec2 blend = vec2(quinticFade(local.x), quinticFade(local.y));

    float n00 = dot(gradientAt(cell, frequency), local);
    float n10 = dot(gradientAt(cell + ivec2(1, 0), frequency), local - vec2(1.0, 0.0));
    float n01 = dot(gradientAt(cell + ivec2(0, 1), frequency), local - vec2(0.0, 1.0));
    float n11 = dot(gradientAt(cell + ivec2(1, 1), frequency), local - vec2(1.0, 1.0));
    return mix(mix(n00, n10, blend.x), mix(n01, n11, blend.x), blend.y);
}

float evaluateNoise(vec2 parameter) {
    int frequency = int(u_noiseSettings.x + 0.5);
    int octaveCount = int(u_noiseSettings.y + 0.5);
    float persistence = u_noiseSettings.z;
    float amplitude = 1.0;
    float value = 0.0;
    for (int octave = 0; octave < 16; ++octave) {
        if (octave >= octaveCount) {
            break;
        }
        value += amplitude * evaluateNoiseOctave(parameter, frequency);
        amplitude *= persistence;
        frequency *= 2;
    }
    return value / u_noiseSettings.w;
}

vec2 tileDomainOffset(vec2 parameter, TileData tile) {
    return vec2(
        0.68 * sin(tau * (2.0 * parameter.x + parameter.y) + tile.phaseA.x)
            + 0.32 * cos(tau * (parameter.x - 2.0 * parameter.y) + tile.phaseA.y),
        0.68 * cos(tau * (parameter.x + 2.0 * parameter.y) + tile.phaseA.y)
            + 0.32 * sin(tau * (2.0 * parameter.x - parameter.y) + tile.phaseA.x)
    );
}

float tileVariation(vec2 parameter, TileData tile) {
    vec4 phases = vec4(tile.phaseA.zw, tile.phaseB.xy);
    vec4 values;
    for (int mode = 0; mode < 4; ++mode) {
        values[mode] = cos(
            tau * (float(tile.frequencyU[mode]) * parameter.x
                + float(tile.frequencyV[mode]) * parameter.y)
            + phases[mode]) / float(mode + 1);
    }
    return dot(values, vec4(1.0)) / 2.0833333333333333333;
}

float evaluateGenerator(vec2 parameter, vec2 world, TileData tile) {
    float worldPhase = tau * (
        u_worldSettings.z * world.x + u_worldSettings.w * world.y);
    float secondaryWorldPhase = tau * (
        -0.73 * u_worldSettings.w * world.x
        + 0.91 * u_worldSettings.z * world.y + 0.17);
    float window = boundaryWindow(parameter);
    vec2 warped = parameter
        + u_generatorWeights.w * window * tileDomainOffset(parameter, tile)
        + u_worldSettings.y * vec2(sin(worldPhase), cos(secondaryWorldPhase));
    float base = u_generatorWeights.x * evaluateFourier(warped)
        + u_generatorWeights.y * evaluateNoise(warped);
    return base
        + u_generatorWeights.z * window * tileVariation(parameter, tile)
        + u_worldSettings.x * sin(worldPhase);
}

vec3 samplePalette(float scalar) {
    float coordinate = 0.5 + 0.5 * tanh(u_toneSettings.y * (scalar - u_toneSettings.x));
    if (u_toneSettings.z > 0.0 && u_toneSettings.w > 0.0) {
        float band = 0.5 + 0.5 * sin(tau * u_toneSettings.z * coordinate);
        coordinate = mix(coordinate, band, u_toneSettings.w);
    }
    coordinate = clamp(coordinate, 0.0, 1.0);

    for (int index = 1; index < 8; ++index) {
        if (index >= u_colorStopCount || coordinate <= u_colorStopPosition[index]) {
            int secondIndex = min(index, u_colorStopCount - 1);
            int firstIndex = max(secondIndex - 1, 0);
            float interval = u_colorStopPosition[secondIndex]
                - u_colorStopPosition[firstIndex];
            float local = interval > 0.0
                ? (coordinate - u_colorStopPosition[firstIndex]) / interval
                : 0.0;
            return mix(
                u_colorStopValue[firstIndex],
                u_colorStopValue[secondIndex],
                smoothstep(0.0, 1.0, local));
        }
    }
    return u_colorStopValue[u_colorStopCount - 1];
}

vec3 linearToSrgb(vec3 linearColor) {
    vec3 value = clamp(linearColor, vec3(0.0), vec3(1.0));
    vec3 low = 12.92 * value;
    vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

vec3 jacobianColor(float determinant) {
    float amount = clamp((determinant - 0.45) / 0.75, 0.0, 1.0);
    return mix(vec3(0.82, 0.08, 0.16), vec3(0.08, 0.78, 0.62), amount);
}

void main() {
    vec2 world = u_worldOrigin + gl_FragCoord.xy / u_pixelsPerTile;
    if (any(lessThan(world, vec2(0.0)))
        || any(greaterThanEqual(world, vec2(u_gridSize)))) {
        outColor = vec4(0.008, 0.014, 0.024, 1.0);
        outDiagnostics = vec4(-1.0);
        outLinearColor = vec4(-1.0);
        return;
    }

    ivec2 tileCoordinate = ivec2(floor(world));
    int tileIndex = tileCoordinate.y * u_gridSize.x + tileCoordinate.x;
    TileData tile = tiles[tileIndex];
    vec2 physical = world - vec2(tileCoordinate);
    vec2 parameter;
    float residual;
    float minimumDeterminant;
    int iterations;
    bool valid = inverseCoons(
        physical,
        tile.edges,
        parameter,
        residual,
        minimumDeterminant,
        iterations);
    if (!valid) {
        outColor = vec4(1.0, 0.0, 1.0, 1.0);
        outDiagnostics = vec4(parameter, -1.0, minimumDeterminant);
        outLinearColor = vec4(-1.0, -1.0, -1.0, residual);
        return;
    }

    float scalar = evaluateGenerator(parameter, world, tile);
    vec3 linearColor = samplePalette(scalar);
    vec4 finalJacobian = coonsJacobian(parameter, tile.edges);
    float finalDeterminant = finalJacobian.x * finalJacobian.w
        - finalJacobian.y * finalJacobian.z;
    outDiagnostics = vec4(parameter, scalar, finalDeterminant);
    outLinearColor = vec4(linearColor, residual);

    vec3 color;
    if (u_debugView == 1) {
        color = jacobianColor(minimumDeterminant);
    } else if (u_debugView == 2) {
        float intensity = clamp(log2(1.0 + residual * 2.0e6) / 8.0, 0.0, 1.0);
        color = mix(vec3(0.01, 0.03, 0.06), vec3(1.0, 0.18, 0.02), intensity);
    } else {
        color = linearToSrgb(linearColor);
        if (u_debugView == 3) {
            vec2 local = fract(world);
            float edgeDistance = min(
                min(local.x, 1.0 - local.x),
                min(local.y, 1.0 - local.y)) * u_pixelsPerTile;
            float line = 1.0 - smoothstep(0.6, 1.8, edgeDistance);
            color = mix(color, vec3(0.2, 0.95, 1.0), 0.82 * line);
        }
    }
    outColor = vec4(color, 1.0);
}
