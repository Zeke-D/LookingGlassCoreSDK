uniform float iTime;
uniform int qs_rows;
uniform int qs_columns;
uniform int qs_width;
uniform int qs_height;

in vec2 texCoords;
out vec4 fragColor;


// from https://github.com/glslify/glsl-look-at/blob/gh-pages/index.glsl
vec3 lookAt(vec3 origin, vec3 target) {
  vec3 rr = vec3(sin(0), cos(0), 0.0);
  vec3 ww = normalize(target - origin);
  vec3 uu = normalize(cross(ww, rr));
  vec3 vv = normalize(cross(uu, ww));
  return mat3(uu, vv, ww) * origin;
}

vec2 model(vec3 point);
// cubic smoothmin via Inigo Quilez https://iquilezles.org/articles/smin/
float smin( float a, float b, float k )
{
    float h = max( k-abs(a-b), 0.0 )/k;
    return min( a, b ) - h*h*h*k*(1.0/6.0);
}
vec3 rotateX( in vec3 p, float t )
{
    float co = cos(t);
    float si = sin(t);
    p.yz = mat2(co,-si,si,co)*p.yz;
    return p;
}
vec3 rotateY( in vec3 p, float t )
{
    float co = cos(t);
    float si = sin(t);
    p.xz = mat2(co,-si,si,co)*p.xz;
    return p;
}
vec3 rotateZ( in vec3 p, float t )
{
    float co = cos(t);
    float si = sin(t);
    p.xy = mat2(co,-si,si,co)*p.xy;
    return p;
}
// https://iquilezles.org/articles/normalsSDF/
vec3 calcNormal( in vec3 point )
{
    const float eps = 0.002;             
    const vec3 v1 = vec3( 1.0,-1.0,-1.0);
    const vec3 v2 = vec3(-1.0,-1.0, 1.0);
    const vec3 v3 = vec3(-1.0, 1.0,-1.0);
    const vec3 v4 = vec3( 1.0, 1.0, 1.0);
	return normalize( v1*model( point + v1*eps ).x + 
					  v2*model( point + v2*eps ).x + 
					  v3*model( point + v3*eps ).x + 
					  v4*model( point + v4*eps ).x );
}

#define SKY       0.
#define DIRT      1.
#define GRASS     2.
#define TREE      3.
#define MAINTREE  4.
#define RAIN      5.
#define LEAVES    6.

#define TAU 6.2831853071
#define SCALE .5
// sphere sdf
float sphere(vec3 point, float radius) {
    return length(point) - radius;
}

float sdCone( vec3 p, vec2 c, float h )
{
  float q = length(p.xz);
  return max(dot(c.xy,vec2(q,p.y)),-h-p.y);
}

float leaf(vec3 point, float radius) {
    vec3 p = point;
    //p.xy *= p.xy; / swap with y -> Z
    // p.y  *= 5.;
    //p.z = p.z - sqrt(p.x ) * .5;
    // p.y = p.y + .5 * pow(p.z, 1./3.);
    return 20. * sdCone(rotateX(p, -.25 * TAU), vec2(radius, radius/2.), 2.);
    
}

float ground(vec3 point) {
    
    return point.y + .3 
         - point.z / 10. // slope up
    + .13 * sin(point.x * 2.) + .118 * sin(point.z * 3.3)           // octave 1
    + .01 * sin(point.x * 24.778582) + .01 * sin(point.z * 13.2389) // octave 2
    + .0001 * sin(point.x * 531.342) + .0005 * cos(point.z * 4592.) // octave 4
    // + .1 * abs(fract(point.x) - .5) + .2 * abs(fract(point.z) - .5)
    //+ .02 * texture(iChannel0, point.xz).g // texture octave
    ;
}

float oakTree(vec3 point, vec3 treeCenter) {
    float mainTree = sphere(vec3(point.x, 0., point.z) - treeCenter, .1);
    vec3 rotP =  rotateY(point - treeCenter, .4) + treeCenter;
    float branch1 = max(
        sphere(vec3(0, rotP.y - .4, rotP.z - .3), .05),
        sphere(rotP - vec3(0., .4, .3), .15)
    );
    float branch2 = max(
        sphere(vec3(point.x - .2, point.y - .55, 0), .05),
        sphere(point - vec3(.2, .55, .3), .4)
        );

    return smin(branch2, smin(mainTree, branch1, .1 ), .1);
}

// returns distance from the model based on point query
vec2 model(vec3 point) {


    // ground
    float grnd = ground(point);

    
    // mini trees
    vec2 scale = vec2(1);
    vec3 p = point;
    p.xz = (fract((scale * p.xz + vec2(.5))) - .5) / scale;
    
    vec3 treeCenter = point - p;
    float radius = .1;
    float groundDistAtTreeCenter = ground(treeCenter) - radius / 2.;
    p.y += groundDistAtTreeCenter;
    p.x += .03 * sin(p.y * 100.);
    p.z += .03 * cos(p.y * 100.) + .1;
    p.y /= .4 + 5. * sin(2.143 + treeCenter.z * 12.32);
    p.xz *= .8 * clamp(1. - p.y, 0., 1.);
    p *= 1.2;
    // a l'il sway
    p.x += p.y * .3 * sin(iTime + (.1 + treeCenter.x) * 320.1737 * treeCenter.z);
    p.x +=  .3 + .2* sin(treeCenter.z * 10. );
    
    float tree = sphere(p, radius) * 1.3;

    // grass
    scale = vec2(10.);
    vec3 p2 = point;
    p2.xz = (fract((scale * p2.xz + vec2(.5))) - .5) / scale;
    vec3 grassCenter = point - p2;
    radius = .05;
    float groundDistAtGrassCenter = ground(grassCenter) - radius / 2.;
    p2.y += groundDistAtGrassCenter;
    p2.x += .02 * sin(30. * grassCenter.z);
    // p2.xy *= 1.2;
    p2.y += .03 * sin(p.x * 10.);
    float grass = max(
        sphere(p2, .002 + .15 * sin(.2 + point.x * point.y * point.z)),
        sphere(vec3(p2.x, 0., p2.z), radius));


    // main tree
    vec3 mainTreeCenter = vec3(.2, 0., .3);
    float mainTree = oakTree(rotateY(point - mainTreeCenter, .3) + mainTreeCenter, mainTreeCenter);

    // leaves
    vec3 lScale = vec3(10.);
    vec3 shift = vec3(
        .05 * sin(.428 * iTime) * length(point - mainTreeCenter) + .25 * (.2-point.z),
        .01 * sin(.4 * iTime) + .25 * point.x,
        0.
    );
    
    vec3 p3 = point + shift;
    p3 = (fract((lScale * p3 + .5)) - .5) / lScale;
    vec3 leafCenter = point - p3;
    vec3 canopyBoundsLoc = leafCenter - mainTreeCenter + shift;
    canopyBoundsLoc.y -= .8;
    float canopyBounds = min(sphere(canopyBoundsLoc + vec3(-.1, .35, 0.1), .3), 
        min(sphere(canopyBoundsLoc + vec3(.4, .2, 0.), .3), sphere(canopyBoundsLoc, .4)));
    // p3 = lookAt(p3, vec3(mainTreeCenter.x, -.4, mainTreeCenter.z));
    // p3 *= 1. + .6 * sin(24.382* leafCenter.x + leafCenter.y * 462.123);

    // p3 = rotateY(rotateX(p3, -.25 * TAU), TAU * 1. * normalize(dot(leafCenter - mainTreeCenter, vec3(mainTreeCenter.x, .4, mainTreeCenter.z) - mainTreeCenter)));
    float leaves = max(canopyBounds, leaf(p3, .02));

    
    
    // rain
    /*
    float t = fract(.5 * iTime);
    p = point;
    p.y -= t + 4. * point.x - .262 * point.z;
    p.x += t;
    vec3 rScale = vec3(3);

    p = (fract((rScale * p + .5)) - .5) / rScale;
    
        // - 4. * fract((point.x-p.x) + (point.z - p.z))
        // - 2. * fract(.356 + .32798 * point.x * 1673.2372 * point.z);
    ;
    // fract(p.x * 72.7642646 + p.z * 271.04725153) + 
    float rain = sphere(p, .003);
    */
    
    float                                         mat = DIRT;
    if (leaves < tree && leaves < mainTree && leaves < grnd)  mat = LEAVES;
    else if (tree < grnd && tree < mainTree)      mat = TREE;
    else if (mainTree < grnd && mainTree < grass) mat = MAINTREE;
    else if (grass < grnd)                        mat = GRASS;
    
    
    float d = min(leaves, smin(mainTree, min(grass, min(grnd, tree)), .4));

    // TODO: Rain
    /*
    const bool sadScene = false;
    if (sadScene && rain < d) {
        d = rain;
        mat = RAIN;
    }
    */
    

    
    // d *= .5;

    return vec2(d, mat);
}

#define MAX_STEPS 40
#define MIN_DIST 0.008 // fine tune for better perf
#define MAX_DIST 20.
// ray marches the scene
vec3 scene(vec3 ro, vec3 ray_dir) {
    vec3 color = vec3(0);
    
    vec3 p = ro;
    vec2 res = model(p);
    float dist_travelled = 0.;
    
    for (int i = 0; i < MAX_STEPS && dist_travelled <= MAX_DIST && res.x > MIN_DIST; i++) {
        p += res.x * ray_dir;
        res = model(p);
        dist_travelled += res.x;
    }

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
    if (dist_travelled >= MAX_DIST) {
        return vec3(0.05);
        return skyBlue;
        // return texture(iChannel1, ray_dir).rgb;
        // color = texture(iChannel0, ray_dir); // color = vec4(index, 0., 1.);
    }
    
    vec3 lightPos = vec3(-1, 2, -2);
    vec3 normal = calcNormal(p);
    float lightContrib = max(0., dot(normalize(lightPos), normal));
     
    // result's y contains material info
    if (res.y < DIRT + .5) {
        color = mix(dirtBrown, copper, lightContrib);
        color = mix(deepBlue, color, color.r * 1.2);
        color.r += .2;
        color.b += .2;
        color = color * .24;
    }
    else if (res.y < GRASS + .5) {
        color = mix(
            barkRed, mix(deepBlue, copper * .6, .2 + .8 * sin(p.z * 10.)), lightContrib)
        
           + .4 * warmHighlight * pow(lightContrib, 4.) // soft specular
           + .1 * warmHighlight * pow(lightContrib, 10.) // harsh specular
           ;
    }
    else if (res.y < TREE + .5) {
        color = mix(deepBlue, green, lightContrib)
           + .4 * warmHighlight * pow(lightContrib, 4.) // soft specular
           + .1 * warmHighlight * pow(lightContrib, 10.) // harsh specular
           ;
        // more color falloff for mini tree contrast
        color = mix(color * 1.2, deepBlue * .1, (.2 - p.y * 2.) * (1. - p.z));
        color = mix(color, color * vec3(1.2, 1.5, .8), -.4 * p.x);
    }
    else if (res.y < MAINTREE + .5) {
        color = mix(
            deepBlue * .5, barkRed, .5 + lightContrib)
           + .8 * copper * pow(lightContrib, 4.) // soft specular
           ;
        // color *= vec3(.7, .5, 1.2);
        // color = mix(color, color * vec3(.6, .2, .8), sin(p.y * 127. * p.x) + sin(p.x * 2010.));
    }
    else if (res.y < RAIN + .5) {
        color += vec3(.5, .9, 1.);
    }
    else if (res.y < LEAVES + .5) {
        color = mix(vec3(0.0, .2, .1), vec3(0.2, 1., .1), lightContrib);
        color = mix(color, deepBlue * .7, 1.2 - 2. * p.y);
    }
    else {
        return debugPink;
    }
    
    //color = vec3(.4 * dot(vec3(-1, 1, -2), normal));
    // color = texture(iChannel1, reflect(ray_dir, normal)).rgb;
    // color += .5 * clamp(skyBlue * (.1 - p.y * 2.), vec3(0), vec3(1)); // fog
    
    // atmospheric attenuation
    // return vec3(1. - dist_travelled / 4.); // render depth map
    vec3 atmosphericColor = vec3(.1, .1, .2);
    color = mix(color, atmosphericColor, dist_travelled / MAX_DIST);
    
    
    return color;
}

void main() {

    // setup shadertoy constants for LKG
    vec2 iResolution = vec2(qs_width, qs_height);
    float numRows = float(qs_rows);
    float numCols = float(qs_columns);

    // -.5 -> .5
    vec2 uv = fract(texCoords * vec2(numCols, numRows) ) - .5;

    vec2 index = vec2(floor(texCoords.x * numCols),
                      floor(texCoords.y * numRows));
              
    // convert 2D index to 1D
    float flatIndex = (index.y * numCols + index.x) / (numRows * numCols);
    // flatIndex is from 0 -> 1
    
    // TODO: figure out math to make this shift be optimal for LKG display
    // using trigonometry based on FOV. For now, horizontal range guess & check is fine
    vec3 leftShift = vec3(1.17, 0., 0.) * (flatIndex - .5);
    
    vec3 moveForward = 0. * vec3(0, 0, 1) * iTime;
    vec3 ro = vec3(0, .2, -1.) + leftShift + moveForward;
   
    vec2 focal_plane_dimensions = 1.8 * (iResolution.xy / iResolution.y) * (vec2(numRows, numCols) / vec2(numCols));
    // make each ray fall within the uv coords projected into the focal plane
    vec3 ray_destination = vec3(uv * focal_plane_dimensions, ro.z + 1.3);
    vec3 ray_dir = normalize(ray_destination - ro);

    vec3 sceneColor = scene(ro, ray_dir);

    // sceneColor *= 1.2;
    
    fragColor = vec4(sceneColor, 1.);
}