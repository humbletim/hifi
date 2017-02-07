var gas = require('./gas.js');

var speed = 0;

exports.drive = function () {
	gas.burn();
};

exports.accelerate = function () {
	speed += 10;
	console.log('[car.js] Car is going: ' + speed);
};
