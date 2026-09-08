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
uniform vec2 u_posterizeSettings;
uniform vec4 u_materialSettings;

uniform vec4 u_generatorWeights;
uniform vec4 u_worldSettings;
uniform int u_scalarProfile;
uniform float u_worldDetailAmplitude;
uniform vec3 u_styleStructureSettings;

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

uint hashCell(ivec2 cell, uint salt) {
    uint value = uint(cell.x) * 0x9e3779b9u
        ^ uint(cell.y) * 0x85ebca6bu
        ^ salt;
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    return value ^ (value >> 16u);
}

float unitFloat24(uint value) {
    return float(value >> 8u) / 16777216.0;
}

float cellularField(vec2 world, float scale) {
    vec2 point = world * scale;
    ivec2 origin = ivec2(floor(point));
    float nearest = 1.0e30;
    float secondNearest = 1.0e30;
    ivec2 nearestCell = origin;
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            ivec2 cell = origin + ivec2(offsetX, offsetY);
            vec2 feature = vec2(cell) + vec2(
                0.12 + 0.76 * unitFloat24(hashCell(cell, 0x68bc21ebu)),
                0.12 + 0.76 * unitFloat24(hashCell(cell, 0x02e5be93u)));
            vec2 delta = feature - point;
            float distanceSquared = dot(delta, delta);
            if (distanceSquared < nearest) {
                secondNearest = nearest;
                nearest = distanceSquared;
                nearestCell = cell;
            } else if (distanceSquared < secondNearest) {
                secondNearest = distanceSquared;
            }
        }
    }
    float separation = sqrt(secondNearest) - sqrt(nearest);
    float transition = clamp((separation - 0.018) / 0.12, 0.0, 1.0);
    float edgeBlend = transition * transition * (3.0 - 2.0 * transition);
    float regionValue = -0.70
        + 1.40 * unitFloat24(hashCell(nearestCell, 0xa511e9b3u));
    return -0.95 + edgeBlend * (regionValue + 0.95);
}

float softBump(float coordinate, float center, float halfWidth) {
    float amount = clamp(1.0 - abs(coordinate - center) / halfWidth, 0.0, 1.0);
    return amount * amount * (3.0 - 2.0 * amount);
}

float edgeInkCenter(uint color) {
    ivec2 key = ivec2(int(color), 0);
    return 0.18 + 0.64 * unitFloat24(hashCell(key, 0x6d2b79f5u));
}

float edgeInkWidth(uint color) {
    ivec2 key = ivec2(int(color), 0);
    return 0.038 + 0.018 * unitFloat24(hashCell(key, 0x1b873593u));
}

float smoothUnit(float value) {
    float amount = clamp(value, 0.0, 1.0);
    return amount * amount * (3.0 - 2.0 * amount);
}

float segmentInk(
    vec2 point,
    vec2 first,
    vec2 second,
    float firstWidth,
    float secondWidth) {
    vec2 direction = second - first;
    float lengthSquared = dot(direction, direction);
    float amount = lengthSquared > 0.0
        ? clamp(dot(point - first, direction) / lengthSquared, 0.0, 1.0)
        : 0.0;
    vec2 offset = point - mix(first, second, amount);
    float width = mix(firstWidth, secondWidth, amount);
    return softBump(length(offset), 0.0, width);
}

float cubicInk(
    vec2 point,
    vec2 start,
    vec2 firstControl,
    vec2 secondControl,
    vec2 end,
    float startWidth,
    float endWidth) {
    float coverage = 0.0;
    vec2 previous = start;
    float previousWidth = startWidth;
    for (int segment = 1; segment <= 16; ++segment) {
        float amount = float(segment) / 16.0;
        float inverse = 1.0 - amount;
        vec2 current = inverse * inverse * inverse * start
            + 3.0 * inverse * inverse * amount * firstControl
            + 3.0 * inverse * amount * amount * secondControl
            + amount * amount * amount * end;
        float currentWidth = mix(startWidth, endWidth, amount);
        coverage = max(
            coverage,
            segmentInk(point, previous, current, previousWidth, currentWidth));
        previous = current;
        previousWidth = currentWidth;
    }
    return coverage;
}

float boundaryInkStroke(uint color, float tangent, float inward) {
    float reach = 1.0 - smoothUnit(inward / 0.20);
    return reach * softBump(
        tangent,
        edgeInkCenter(color),
        edgeInkWidth(color));
}

float edgeConnectedInk(vec2 parameter, TileData tile) {
    vec2 endpoints[4] = vec2[4](
        vec2(edgeInkCenter(tile.edges.x), 0.0),
        vec2(edgeInkCenter(tile.edges.y), 1.0),
        vec2(0.0, edgeInkCenter(tile.edges.z)),
        vec2(1.0, edgeInkCenter(tile.edges.w)));
    vec2 inwardDirections[4] = vec2[4](
        vec2(0.0, 1.0),
        vec2(0.0, -1.0),
        vec2(1.0, 0.0),
        vec2(-1.0, 0.0));
    float widths[4] = float[4](
        edgeInkWidth(tile.edges.x),
        edgeInkWidth(tile.edges.y),
        edgeInkWidth(tile.edges.z),
        edgeInkWidth(tile.edges.w));
    int pairing = (tile.frequencyU[0] + tile.frequencyV[1]) % 3;
    ivec4 pairs;
    if (pairing == 0) {
        pairs = ivec4(0, 1, 2, 3);
    } else if (pairing == 1) {
        pairs = ivec4(0, 2, 1, 3);
    } else {
        pairs = ivec4(0, 3, 1, 2);
    }
    float firstStartReach = 0.25 + 0.06 * sin(tile.phaseA.z);
    float firstEndReach = 0.25 + 0.06 * cos(tile.phaseA.w);
    float secondStartReach = 0.25 + 0.06 * sin(tile.phaseB.x);
    float secondEndReach = 0.25 + 0.06 * cos(tile.phaseB.y);
    float boundaryInk
        = boundaryInkStroke(tile.edges.x, parameter.x, parameter.y)
        + boundaryInkStroke(tile.edges.y, parameter.x, 1.0 - parameter.y)
        + boundaryInkStroke(tile.edges.z, parameter.y, parameter.x)
        + boundaryInkStroke(tile.edges.w, parameter.y, 1.0 - parameter.x);
    float interiorInk = boundaryWindow(parameter) * (
        cubicInk(
            parameter,
            endpoints[pairs[0]],
            endpoints[pairs[0]] + firstStartReach * inwardDirections[pairs[0]],
            endpoints[pairs[1]] + firstEndReach * inwardDirections[pairs[1]],
            endpoints[pairs[1]],
            widths[pairs[0]],
            widths[pairs[1]])
        + cubicInk(
            parameter,
            endpoints[pairs[2]],
            endpoints[pairs[2]] + secondStartReach * inwardDirections[pairs[2]],
            endpoints[pairs[3]] + secondEndReach * inwardDirections[pairs[3]],
            endpoints[pairs[3]],
            widths[pairs[2]],
            widths[pairs[3]]));
    float coverage = smoothUnit(clamp(boundaryInk + interiorInk, 0.0, 1.0));
    return 0.58 - 1.48 * coverage;
}

float evaluateGenerator(vec2 parameter, vec2 world, TileData tile) {
    float worldPhase = tau * (
        u_worldSettings.z * world.x + u_worldSettings.w * world.y);
    float secondaryWorldPhase = tau * (
        -0.73 * u_worldSettings.w * world.x
        + 0.91 * u_worldSettings.z * world.y + 0.17);
    float worldDetail = 0.0;
    if (u_worldDetailAmplitude != 0.0) {
        float tertiaryWorldPhase = tau * (
            (1.31 * u_worldSettings.z + 0.47 * u_worldSettings.w) * world.x
            + (-0.59 * u_worldSettings.z + 1.17 * u_worldSettings.w) * world.y
            + 0.43);
        float quaternaryWorldPhase = tau * (
            (-1.73 * u_worldSettings.z + 0.29 * u_worldSettings.w) * world.x
            + (0.41 * u_worldSettings.z + 1.53 * u_worldSettings.w) * world.y
            + 0.71);
        worldDetail = (
            sin(worldPhase)
            + 0.63 * cos(secondaryWorldPhase)
            + 0.41 * sin(tertiaryWorldPhase)
            + 0.28 * cos(quaternaryWorldPhase)) / 2.32;
    }
    float worldGrain = 0.0;
    if (u_styleStructureSettings.x != 0.0) {
        worldGrain = (
            sin(5.3 * worldPhase + 0.8 * sin(1.7 * secondaryWorldPhase))
            + 0.5 * cos(7.1 * secondaryWorldPhase - 0.35 * worldPhase)
            + 0.25 * sin(13.7 * worldPhase + 0.6 * secondaryWorldPhase))
            / 1.75;
    }
    float window = boundaryWindow(parameter);
    vec2 warped = parameter
        + u_generatorWeights.w * window * tileDomainOffset(parameter, tile)
        + u_worldSettings.y * vec2(sin(worldPhase), cos(secondaryWorldPhase));
    float base = u_generatorWeights.x * evaluateFourier(warped)
        + u_generatorWeights.y * evaluateNoise(warped);
    float edgeStructure = 0.0;
    if (u_styleStructureSettings.z != 0.0) {
        edgeStructure = u_styleStructureSettings.z * edgeConnectedInk(parameter, tile);
    }
    float value = base
        + u_generatorWeights.z * window * tileVariation(parameter, tile)
        + u_worldSettings.x * sin(worldPhase)
        + u_worldDetailAmplitude * worldDetail
        + u_styleStructureSettings.x * worldGrain
        + edgeStructure;
    // 标量 profile 只重排连续场的层级，不改变 Wang 边界的取值一致性。
    float bounded = clamp(value, -1.0, 1.0);
    if (u_scalarProfile == 1) {
        return 1.0 - 2.0 * abs(bounded);
    }
    if (u_scalarProfile == 2) {
        return clamp(
            cellularField(world, u_styleStructureSettings.y) + 0.12 * tanh(value),
            -1.0,
            1.0);
    }
    return value;
}

float toneCoordinate(float scalar) {
    float coordinate = 0.5 + 0.5 * tanh(u_toneSettings.y * (scalar - u_toneSettings.x));
    if (u_toneSettings.z > 0.0 && u_toneSettings.w > 0.0) {
        float band = 0.5 + 0.5 * sin(tau * u_toneSettings.z * coordinate);
        coordinate = mix(coordinate, band, u_toneSettings.w);
    }
    coordinate = clamp(coordinate, 0.0, 1.0);
    int posterizeLevels = int(u_posterizeSettings.x + 0.5);
    if (posterizeLevels >= 2) {
        float intervals = float(posterizeLevels - 1);
        float scaled = coordinate * intervals;
        float lower = min(floor(scaled), intervals - 1.0);
        float fraction = scaled - lower;
        float transition = clamp(
            (fraction - (0.5 - u_posterizeSettings.y))
                / (2.0 * u_posterizeSettings.y),
            0.0,
            1.0);
        coordinate = (lower + transition * transition * (3.0 - 2.0 * transition))
            / intervals;
    }
    return coordinate;
}

vec3 linearRgbToOklab(vec3 color) {
    vec3 lms = mat3(
        0.4122214708, 0.2119034982, 0.0883024619,
        0.5363325363, 0.6806995451, 0.2817188376,
        0.0514459929, 0.1073969566, 0.6299787005) * color;
    vec3 roots = sign(lms) * pow(abs(lms), vec3(1.0 / 3.0));
    return mat3(
        0.2104542553, 1.9779984951, 0.0259040371,
        0.7936177850, -2.4285922050, 0.7827717662,
        -0.0040720468, 0.4505937099, -0.8086757660) * roots;
}

vec3 oklabToLinearRgb(vec3 color) {
    vec3 roots = mat3(
        1.0, 1.0, 1.0,
        0.3963377774, -0.1055613458, -0.0894841775,
        0.2158037573, -0.0638541728, -1.2914855480) * color;
    vec3 lms = roots * roots * roots;
    return clamp(mat3(
        4.0767416621, -1.2684380046, -0.0041960863,
        -3.3077115913, 2.6097574011, -0.7034186147,
        0.2309699292, -0.3413193965, 1.7076147010) * lms, 0.0, 1.0);
}

vec3 samplePalette(float scalar) {
    float coordinate = toneCoordinate(scalar);

    for (int index = 1; index < 8; ++index) {
        if (index >= u_colorStopCount || coordinate <= u_colorStopPosition[index]) {
            int secondIndex = min(index, u_colorStopCount - 1);
            int firstIndex = max(secondIndex - 1, 0);
            float interval = u_colorStopPosition[secondIndex]
                - u_colorStopPosition[firstIndex];
            float local = interval > 0.0
                ? (coordinate - u_colorStopPosition[firstIndex]) / interval
                : 0.0;
            vec3 first = linearRgbToOklab(u_colorStopValue[firstIndex]);
            vec3 second = linearRgbToOklab(u_colorStopValue[secondIndex]);
            return oklabToLinearRgb(mix(
                first,
                second,
                smoothstep(0.0, 1.0, local)));
        }
    }
    return u_colorStopValue[u_colorStopCount - 1];
}

vec3 applyMaterial(vec3 linearColor, float scalar, vec2 parameter) {
    float safe = boundaryWindow(parameter);
    float contourFrequency = u_materialSettings.x;
    float contourStrength = u_materialSettings.y;
    if (contourFrequency > 0.0 && contourStrength > 0.0) {
        float phase = toneCoordinate(scalar) * contourFrequency;
        float distanceToLine = abs(fract(phase + 0.5) - 0.5);
        float antialiasWidth = max(fwidth(phase), 1.0e-4);
        float contour = 1.0 - smoothstep(
            u_materialSettings.z,
            u_materialSettings.z + antialiasWidth,
            distanceToLine);
        linearColor *= 1.0 - safe * contourStrength * contour;
    }

    float reliefStrength = u_materialSettings.w;
    if (reliefStrength > 0.0) {
        // 屏幕导数跨接缝不保证一致，因此所有法线效果都由边界窗口平滑归零。
        vec2 slope = vec2(dFdx(scalar), dFdy(scalar)) * u_pixelsPerTile;
        vec3 normal = normalize(vec3(-0.12 * slope, 1.0));
        const vec3 light = normalize(vec3(-0.45, 0.55, 0.82));
        float neutralLight = light.z;
        float relief = dot(normal, light) - neutralLight;
        linearColor *= max(0.65, 1.0 + safe * reliefStrength * relief);
    }
    return clamp(linearColor, 0.0, 1.0);
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
        // 画布外保留可辨认的深色工作区，避免宽屏窗口出现纯黑断层。
        outColor = vec4(0.025, 0.036, 0.055, 1.0);
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
        vec3 displayLinear = u_materialSettings.y > 0.0 || u_materialSettings.w > 0.0
            ? applyMaterial(linearColor, scalar, parameter)
            : linearColor;
        color = linearToSrgb(displayLinear);
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
