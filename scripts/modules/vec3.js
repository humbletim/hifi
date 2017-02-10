// vec3.js -- example "system" module
//
//   var vec3 = Script.require('vec3');
//   MyAvatar.position = { x:1, y:2, z: Math.PI };
//   print('normalized position:', vec3.normalize(MyAvatar.position));
//   // logs "normalized position: [Vec3 (1.000, 2.000, 3.142)]"
//

var DEBUG = false;

try {
    // for Script.require
    module.exports = vec3;
} catch(e) {
    // for Script.include
    Script.registerValue("vec3", vec3);
}

vec3.prototype = {
    // inherit Vec3's constants and methods
    __proto__: Vec3,
    isValid: function() {
        return isFinite(this.x) && isFinite(this.y) && isFinite(this.z);
    },
    toString: function() {
        return "[Vec3 (" + [this.x, this.y, this.z].map(fixed) + ")]";
    },
};

function vec3(x, y, z) {
    if (!(this instanceof vec3)) {
        // if called as a function then re-invoke as a constructor
        return new vec3(x, y, z);
    }

    // unfold default arguments (vec3(), vec3(.5), vec3(0,1), etc.)
    this.x = x !== undefined ? x : 0;
    this.y = y !== undefined ? y : this.x;
    this.z = z !== undefined ? z : this.y;

    if (DEBUG) {
        if (!this.isValid())
            throw new Error('vec3() -- invalid initial values [' + [].slice.call(arguments) + ']');
    }
}

function fixed(number) {
    return number.toFixed(3);
}
