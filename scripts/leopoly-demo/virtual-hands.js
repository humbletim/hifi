// interim "virtual hand" abstraction module -- humbletim

var META = {
    TRIGGER: { ON: 0.15, OFF: 0.1 },
    GRIP: { ON: 0.99, OFF: 0.95 },
    left: {
        jointName: '_CONTROLLER_LEFTHAND',
        pose: Controller.Standard.LeftHand,
        trigger: Controller.Standard.LT,
        click: Controller.Standard.LTClick,
        grip: Controller.Standard.LeftGrip,
    },
    right: {
        jointName: '_CONTROLLER_RIGHTHAND',
        pose: Controller.Standard.RightHand,
        trigger: Controller.Standard.RT,
        click: Controller.Standard.RTClick,
        grip: Controller.Standard.RightGrip,
    },
};

function VirtualHand(id) {
    if (id === 'dominant') {
        this.onDominantHandChanged = function() { this.id = MyAvatar.getDominantHand(); };
        MyAvatar.dominantHandChanged.connect(this, 'onDominantHandChanged');
        var self = this;
        Script.scriptEnding.connect(function() { MyAvatar.dominantHandChanged.disconnect(self, 'onDominantHandChanged'); });
        id = 'dominant<'+MyAvatar.getDominantHand()+'>';
    }
    this.id = id;
}

VirtualHand.prototype = Object.create({
    constructor: VirtualHand,
    toString: function() {
        return '[VirtualHand id='+this.id+' trigger='+this.triggerValue+' grip='+this.gripValue+']';
    },
}, {
    id: { enumerable: true, get: function() { return this._id; }, set: function(_id) {
        var id = /left/i.test(_id) ? 'left' : /right/i.test(_id) ? 'right' : 'unknown';
        if (id !== 'left' && id !== 'right') {
            throw new Error('expected "left" or "right" for hand.id ' + _id);
        }
        this._id = id;
        this.hand = META[this._id];
        //this.hand.jointIndex = MyAvatar.getJointIndex(this.hand.jointName);
        print('VirtualHand id updated', this);
    }},
    pose: { enumerable: true, get: function() {
        return Controller.getPoseValue(this.hand.pose);
    }},
    reference: {
        enumerable: false,
        get: function() {
            return this.pose.valid ? MyAvatar : Camera;
        },
    },
    translation: {
        enumerable: true,
        get: function() {
            var pose = this.pose;
            if (pose.valid) {
                return pose.translation;
            } else {
                var result = this.rayIntersection;
                var intersection = result.intersection || Vec3.ZERO;
                var ref = this.reference;
                return Vec3.multiplyQbyV(Quat.inverse(ref.orientation), Vec3.subtract(intersection, ref.position));
            }
        },
    },
    position: {
        enumerable: true,
        get: function() {
            var ref = this.reference;
            return Vec3.sum(Vec3.multiplyQbyV(ref.orientation, this.translation), ref.position);
        },
    },
    rotation: {
        enumerable: true,
        get: function() {
            var pose = this.pose;
            if (pose.valid) {
                return pose.rotation;
            } else {
                var result = this.rayIntersection;
                var surfaceNormal = result.surfaceNormal || Vec3.UNIT_Y;
                result = result || { intersection: Vec3.ZERO };
                var rot = Quat.lookAtSimple(Vec3.ZERO, surfaceNormal);
                rot = Quat.multiply(rot, Quat.fromPitchYawRollDegrees(90,0,0));
                //rot = Quat.multiply(Quat.inverse(this.reference.orientation), rot);
                return rot;
            }
        },
    },
    orientation: {
        enumerable: true,
        get: function() {
            return this.pose.valid ? Quat.multiply(this.reference.orientation, this.rotation) : this.rotation;
        },
    },
    _result: { intersects: false, intersection: Vec3.ZERO },
    _intersection: { intersects: false, intersection: Vec3.ZERO },
    intersection: {
        enumerable: false,
        get: function() {
            return this._intersection;
        },
    },
    rayIntersection: {
        enumerable: false,
        get: function() {
            var result = Entities.findRayIntersection(
                this.pose.valid ? this.rayController : this.rayMouse, true, this.whitelist, this.blacklist, false);
            this._intersection = result;
            if (!result.intersects) {
                result = this._result;
            } else {
                this._result = result;
            }
            return result || { intersects: false };
        },
    },
    rayController: { enumerable: true, get: function() {
        return {
            point: { x: 0.5, y: 0.5, z: 0 },
            origin: this.position,
            direction: Vec3.multiplyQbyV(this.orientation, Vec3.UNIT_Z),
        };
    }},
    rayMouse: { enumerable: true, get: function() {
        var pt = Reticle.position || {
            x: Controller.getValue(Controller.Hardware.Keyboard.MouseX),
            y: Controller.getValue(Controller.Hardware.Keyboard.MouseY)
        };
        var ray = Camera.computePickRay(pt.x, pt.y);
        ray.point = pt;
        if (0&&JSON.stringify(ray,0,2) !== this.lastray) {
            this.lastray = JSON.stringify(ray,0,2);
            print(this.lastray);
        }
        return ray;
    }},
    leftMouse: { enumerable: true, get: function() {
        return Controller.getValue(Controller.Hardware.Keyboard.LeftMouseButton);
    }},
    middleMouse: { enumerable: true, get: function() {
        return Controller.getValue(Controller.Hardware.Keyboard.MiddleMouseButton);
    }},
    rightMouse: { enumerable: true, get: function() {
        return Controller.getValue(Controller.Hardware.Keyboard.RightMouseButton);
    }},
    trigger: { enumerable: true, get: function() {
        return Controller.getValue(this.hand.trigger);
    }},
    grip: { enumerable: true, get: function() {
        return Controller.getValue(this.hand.grip);
    }},
    triggerValue: { enumerable: true, get: function() {
        return this.pose.valid ? this.trigger : (this.middleMouse ? META.TRIGGER.ON+0.01 : this.leftMouse ? 1.0 : 0.0);
    }},
    gripValue: { enumerable: true, get: function() {
        return this.pose.valid ? this.grip : (this.rightMouse ? 1.0 : 0.0);
    }},
    triggerPressed: { enumerable: true, get: function() {
        return this._triggerPressed = (this.triggerValue > (this._triggerPressed ? META.TRIGGER.ON : META.TRIGGER.OFF));
    }},
    gripPressed: { enumerable: true, get: function() {
        return this._gripPressed = (this.gripValue > (this._gripPressed ? META.GRIP.ON : META.GRIP.OFF));
    }},
});

module.exports = {
    version: '0.0.0',
    config: META,
    VirtualHand: VirtualHand,
    left: new VirtualHand('left'),
    right: new VirtualHand('right'),
    dominant: new VirtualHand('dominant'),
};
