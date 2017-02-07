var car = require('./car.js');

console.log('[gas.js] Car module (incomplete):', Object.keys(car));

// If I try to invoke "car.accelerate()" here, that method is not yet defined and it will error!
// This is what you want to avoid - directly using another dependency in the high-level code that
// runs while this module itself is being defined.
//
// Uncomment this line and see yourself:
//car.accelerate();

exports.burn = function () {
	// But invoking "car.accelerate()" in here is fine.  By the time
	// the code inside this function is running, car.accelerate()
	// will be defined.
	console.log('[gas.js] Car module (complete):', Object.keys(car));
	car.accelerate();
};
