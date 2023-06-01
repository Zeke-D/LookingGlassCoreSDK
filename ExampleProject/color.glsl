#version 330 core

#define SKY       0.
#define DIRT      1.
#define GRASS     2.
#define TREE      3.
#define MAINTREE  4.
#define RAIN      5.
#define LEAVES    6.
#define NUM_MATERIALS 6.

#define TAU 6.2831853071
#define MAX_DIST 4.

struct Hit {
    vec3 position;
    vec3 normal;
    float material;
};

in vec2 texCoords;
out vec4 fragColor;

uniform float iTime;
uniform int renderSwitch;
uniform sampler2D posMatTex;
uniform sampler2D normalTex;
uniform sampler2D customTex;

vec3 scene(Hit hit) {
    hit.material *= NUM_MATERIALS;
    vec3 color = vec3(0);

    float t = .5 + .5 * sin(iTime);
    
    // const vec3 deepBlue = vec3(0.000,0.184,0.180);
    const vec3 deepBlue = vec3(7., 2., 58.) / 255.;
    const vec3 copper = vec3(1.000,0.502,0.251);
    const vec3 dirtBrown = vec3(0.141,0.141,0.059);
    // const vec3 warmHighlight = vec3(1.000,0.961,0.749) ;
    const vec3 warmHighlight = vec3(244, 226, 22) / 255.;
    // const vec3 green = vec3(0.024,0.541,0.322);
    const vec3 green = vec3(10, 124, 46) / 255.;
    const vec3 debugPink = vec3(1, 0, 1);
    const vec3 pink = vec3(244, 45, 136) / 255.;
    const vec3 skyBlue = vec3(.4, .6, 1.);
    // const vec3 barkRed = vec3(48., 10., 5.) / 255.;
    const vec3 barkRed = vec3(61., 0., 28) / 255.;

    vec3 leafColorA = mix(vec3(0.0, .2, .1), barkRed, t);
    vec3 leafColorB = vec3(0.2, 1., .1);
    
    float gradientFactor = clamp(hit.position.y / MAX_DIST + .2, 0., 1.);
    vec3 dayGradient = mix(deepBlue, skyBlue, gradientFactor);
    vec3 sunsetGradient = mix(pink, copper, gradientFactor);
    vec3 skyGradient = mix(sunsetGradient, dayGradient, t);

    
    // SKY render
    if (hit.position.z >= 10.) {
        return skyGradient;
        // return texture(iChannel1, ray_dir).rgb;
        // color = texture(iChannel0, ray_dir); // color = vec4(index, 0., 1.);
    }
    
    vec3 lightPos = vec3(-1, 2, -2);
    float lightContrib = max(0., dot(normalize(lightPos), hit.normal));
     
    // result's y contains hit.materialerial info
    if (hit.material < DIRT + .5) {
        color = mix(dirtBrown, copper, lightContrib);
        color = mix(deepBlue, color, color.r * 1.2);

        color *= texture(customTex, texCoords * vec2(8., 6.)).rgb;
        
        color.r += .2;
        color.b += .2;
        color = color * .24;
    }
    else if (hit.material < GRASS + .5) {
        color = mix(
            barkRed, mix(green, copper * .6, .2 + .8 * sin(hit.position.z * 10.)), lightContrib)
        
           + .4 * warmHighlight * pow(lightContrib, 4.) // soft specular
           + .1 * warmHighlight * pow(lightContrib, 10.) // harsh specular
           ;
    }
    else if (hit.material < TREE + .5) {
        color = mix(deepBlue, green, lightContrib)
           + .4 * warmHighlight * pow(lightContrib, 4.) // soft specular
           + .1 * warmHighlight * pow(lightContrib, 10.) // harsh specular
           ;
        // more color falloff for mini tree contrast
        color = mix(color * 1.2, deepBlue * .1, (.2 - hit.position.y * 2.) * (1. - hit.position.z));
        color = mix(color, color * vec3(1.2, 1.8, .8), -.8 * hit.position.x);
    }
    else if (hit.material < MAINTREE + .5) {
        color = mix(
            deepBlue * .5, barkRed, .5 + lightContrib)
           + .8 * copper * pow(lightContrib, 4.) // soft specular
           ;
        color = mix(color, vec3(0), 1.5 * (1. - length(hit.position - vec3(.2, .8, .3))));
        // color *= texture(customTex, texCoords * texCoords).rgb;
        // color *= vec3(.7, .5, 1.2);
        // color = mix(color, color * vec3(.6, .2, .8), sin(hit.position.y * 127. * hit.position.x) + sin(hit.position.x * 2010.));
    }
    else if (hit.material < RAIN + .5) {
        color += vec3(.5, .9, 1.);
    }
    else if (hit.material < LEAVES + .5) {
        color = mix(leafColorA, leafColorB, lightContrib);
        color = mix(color, vec3(0), 1.2 * (1. - length(hit.position - vec3(.2, .8, .3))));
        color = mix(color, deepBlue * .7, 1.2 - 2. * hit.position.y);
    }
    else {
        return debugPink;
    }
    
    // color += .5 * clamp(skyBlue * (.1 - hit.position.y * 2.), vec3(0), vec3(1)); // fog
    
    // atmospheric attenuation
    // return vec3(1. - dist_travelled / 4.); // render depth map
    vec3 atmosphericColor = skyGradient;//vec3(.1, .1, .2);
    color = mix(color, atmosphericColor, (hit.position.z + .5) / MAX_DIST);

    color += .2 * hit.position.z * pow(lightContrib, 3.) * skyGradient;
    
    
    return color;
}


void main() {

    vec4 posMat = texture(posMatTex, texCoords);
    vec4 normal = texture(normalTex, texCoords);

    if (renderSwitch == 1) {
        fragColor = posMat;
        return;
    }
    if (renderSwitch == 2) {
        fragColor = normal;
        return;
    }
    
    Hit hit = Hit(posMat.rgb, normal.rgb, normal.a);
    vec3 col = scene(hit);

    fragColor = vec4(col * 1.2, 1.);
    // fragColor.rgb = posMat.aaa;
}