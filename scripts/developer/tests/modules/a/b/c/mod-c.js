print("...Hello from: " + Script.resolvePath(''));
var tmp = require;
require = 5;
print("require", require);
print(JSON.stringify({
    __dirname: typeof __dirname !== 'undefined' && __dirname,
    'module.parent': module.parent === module,
    'module.children': module.children,
    'module.loaded': module.loaded,
    exports: exports,
},0,2));

var m = module;
exports.toString = function() { return "[module mod-c " + module.children.length + ' '+typeof Object.keys(m.parent||{}) + " ]"; };
print('exports.toString...', exports);
