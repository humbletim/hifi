console.log('main starting');
const a = require('./a.js');
const b = require('./b.js');
console.log('in main, a.done=%j, b.done=%j', a.done, b.done);

exports.name = 'main';
exports.a = a;
exports.b = b;
exports['a.done?'] = a.done;
exports['b.done?'] = b.done;
