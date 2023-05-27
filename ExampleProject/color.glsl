#define SKY       0.
#define DIRT      1.
#define GRASS     2.
#define TREE      3.
#define MAINTREE  4.
#define RAIN      5.
#define LEAVES    6.
#define NUM_MATERIALS 6.

#define TAU 6.2831853071
#define MAX_DIST 20.

struct Hit {
    vec3 position;
    vec3 normal;
    float material;
};
    
vec3 scene(Hit hit) {
    hit.material *= NUM_MATERIALS;
    vec3 color = vec3(0);

    const vec3 deepBlue = vec3(0.000,0.184,0.180);
    const vec3 copper = vec3(1.000,0.502,0.251);
    const vec3 dirtBrown = vec3(0.141,0.141,0.059);
    const vec3 warmHighlight = vec3(1.000,0.961,0.749);
    const vec3 green = vec3(0.024,0.541,0.322);
    const vec3 debugPink = vec3(1, 0, 1);
    const vec3 skyBlue = vec3(.4, .6, 1.);
    const vec3 barkRed = vec3(48., 10., 5.) / 255.;
    const vec3 leafColor = green; // TODO: make uniform
       
    // SKY render
    if (hit.position.z >= MAX_DIST) {
        return vec3(0.05);
        return skyBlue;
        // return texture(iChannel1, ray_dir).rgb;
        // color = texture(iChannel0, ray_dir); // color = vec4(index, 0., 1.);
    }
    
    vec3 lightPos = vec3(-1, 2, -2);
    float lightContrib = max(0., dot(normalize(lightPos), hit.normal));
     
    // result's y contains hit.materialerial info
    if (hit.material < DIRT + .5) {
        color = mix(dirtBrown, copper, lightContrib);
        color = mix(deepBlue, color, color.r * 1.2);
        color.r += .2;
        color.b += .2;
        color = color * .24;
    }
    else if (hit.material < GRASS + .5) {
        color = mix(
            barkRed, mix(deepBlue, copper * .6, .2 + .8 * sin(hit.position.z * 10.)), lightContrib)
        
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
        color = mix(color, color * vec3(1.2, 1.5, .8), -.4 * hit.position.x);
    }
    else if (hit.material < MAINTREE + .5) {
        color = mix(
            deepBlue * .5, barkRed, .5 + lightContrib)
           + .8 * copper * pow(lightContrib, 4.) // soft specular
           ;
        // color *= vec3(.7, .5, 1.2);
        // color = mix(color, color * vec3(.6, .2, .8), sin(hit.position.y * 127. * hit.position.x) + sin(hit.position.x * 2010.));
    }
    else if (hit.material < RAIN + .5) {
        color += vec3(.5, .9, 1.);
    }
    else if (hit.material < LEAVES + .5) {
        color = mix(vec3(0.0, .2, .1), vec3(0.2, 1., .1), lightContrib);
        color = mix(color, deepBlue * .7, 1.2 - 2. * hit.position.y);
    }
    else {
        return debugPink;
    }
    
    // color += .5 * clamp(skyBlue * (.1 - hit.position.y * 2.), vec3(0), vec3(1)); // fog
    
    // atmospheric attenuation
    // return vec3(1. - dist_travelled / 4.); // render depth map
    vec3 atmosphericColor = vec3(.1, .1, .2);
    color = mix(color, atmosphericColor, hit.position.z / MAX_DIST);
    
    return color;
}


in vec2 texCoords;
out vec4 fragColor;

uniform float iTime;
uniform sampler2D posMatTex;
uniform sampler2D normalTex;

void main() {

    vec4 posMat = texture(posMatTex, texCoords);
    vec4 normal = texture(normalTex, texCoords);
    
    Hit hit = Hit(posMat.rgb, normal.rgb, normal.a);
    vec3 col = scene(hit);

    fragColor = vec4(col, 1.);
    // fragColor.rgb = posMat.aaa;
}