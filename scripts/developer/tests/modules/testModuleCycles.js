(typeof Script === 'object' ? Script : module).require('./jasmine-isomorphic.js');

var require = Script.require;

var json = require('./example.json');
console.log("example.json keys are: " + Object.keys(json));
console.log('mod.js == ', require('./a/mod.js'));

console.info(
    'example again (relative to mod.js cached Module entry): ',
    require.cache[require.resolve('./a/mod.js')].require('../example.json').name
);
// car/gas cyclic dependency / incomplete module test
//require('./ec528092f53d1797da94-e9c6af9cec2febcb62747d55a19239fbe08b6d8b/main.js');
require('./gist/main.js');
// a/b cyclic dependency test
require('./cycles/main.js');
