console.log('a starting');
exports.done = false;
const b = require('./b.js');
console.log('in a, b.done = %j', b.done);
exports.done = true;
console.log('a done');

exports.name = 'a';
exports['a.done?'] = exports.done;
exports['b.done?'] = b.done;
