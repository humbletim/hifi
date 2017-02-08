// Example of how "system" modules could be used to decouple system implementation details from JS-side use cases.
//  (ie: a way for the JS side to focus on the "what" -- and the C++ side to evolve even faster in terms of "how")
//
// Consider the idea of moving all math functions into JS -- which in *some* cases would see a performance improvement,
//  and in others the opposite...
//
// If "Vec3" were afforded to the user as a system module, they could just do this:
//   var vec3 = Script.require('vec3');
//
// And then backend C++ developers could freely migrate some/all/none of the math functions into pure JS.
// And also prototype new kinds of math helpers in pure JS here -- porting to native C++ once proven out.

// for this example module both Script.include and Script.require scenarios are supported
try {
    // Script.require
    module.exports = vec3;
} catch(e) {
    // Script.include
    Script.registerValue("vec3", vec3);
}

vec3.prototype = {
    // simple way to detect NaN and Infinity values
    isValid: function() {
        return isFinite(this.x) && isFinite(this.y) && isFinite(this.z);
    },
    // "pretty print" Vec3's
    // example:
    //     var v = vec3();
    //     print(v); // outputs [Vec3 (0.000, 0.000, 0.000)]
    // note: the Console... and Script Editor... debug prompts also invoke toString on immediate values
    toString: function() {
        function fixed(n) { return n.toFixed(3); }
        return "[Vec3 (" + [this.x, this.y, this.z].map(fixed) + ")]";
    },
};

vec3.DEBUG = true; // scripts could disable this for a nominal performance increase

function vec3(x, y, z) {
    if (!(this instanceof vec3)) {
        // if vec3 is called as a function then re-invoke as a constructor
        return new vec3(x, y, z);
    }

    // unfold default arguments (vec3(), vec3(.5), vec3(0,1), etc.)
    this.x = x !== undefined ? x : 0;
    this.y = y !== undefined ? y : this.x;
    this.z = z !== undefined ? z : this.y;

    if (vec3.DEBUG && !this.isValid())
        throw new Error('vec3() -- invalid initial values ['+[].slice.call(arguments)+']');
};

