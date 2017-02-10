if (typeof Script !== 'object') {
    print = console.log.bind(console);
    Script = { require: require };
}

var vec3 = Script.require('vec3');
print('typeof vec3', typeof vec3);

print('vec3()', vec3());
print('vec3(1)', vec3(1));
print('vec3(1,2)', vec3(1,2));
print('vec3(1,2,3)', vec3(1,2,3));
print('vec3(PI)', vec3(Math.PI));

try {
    print('vec3(NaN)', vec3(NaN));
} catch(e) {
    print('vec3(NaN)', e);
}
