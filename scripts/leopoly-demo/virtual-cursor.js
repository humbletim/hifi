
// virtual cursor/stylus UI module -- humbletim

function assign(target, sources) {
    [].slice.call(arguments, 1).forEach(function(source) {
        Object.keys(source||{}).forEach(function(key) {
            target[key] = source[key];
        });
    });
    return target;
}

//var HTsrc = 'http://192.168.0.5:8000/HT/src',
//    Markers = Script.require(HTsrc + '/hifi.coffee.js')(HTsrc + '/hifi.debugmarkers.coffee');

module.exports = {
    version: '0.0.0',
    cursors: {},
    defaults: {
        name: 'Cursor',
        type: 'Shape',
        shape: 'Cone',
        dimensions: { x: .25, y: 1, z: .25 },
        color: { red: 0x99, green: 0x99, blue: 0x99 },
        collisionless: true,
        position: MyAvatar.position,
        lifetime: 3600,
        //registrationPoint: Vec3.HALF,
        //visible: false,
    },
    create: function(source) {
        var props = assign({
            position: MyAvatar.position,
        }, this.defaults, source);
        
        print('creating', props.type, /^[a-z]/.test(props.type));
        source.id = /^[a-z]/.test(props.type||'Sphere') ? Overlays.addOverlay(props.type.toLowerCase(), props) :
            Entities.addEntity(props, props.clientOnly || !Entities.serversExist());
        return this.wrap(source);
    },
    wrap: function(source) {
        if (typeof source.update === 'function') {
            return source;
        }
        var self = this;
        return assign(source, {
            update: function(props) {
                return self.update(assign({ id: this.id, name: this.name }, props));
            },
            dispose: function() {
                return self.destroy(this);
            }
        });
    },
    update: function(source) {
        var self = this;
        source.id = !Uuid.isNull(source.id) ? source.id : Object.keys(this.cursors).filter(function(id) {
            return self.cursors[id].name === source.name;
        })[0];

        if (Uuid.isNull(source.id)) {//!this.isValidObject(source.id)) {
            source = this.create(source);
        }
        if (Uuid.isNull(source.id)) {
            throw new Error('couldn\'t create cursor');
        }
        var result = this.wrap(this.cursors[source.id] = assign(source, this.editObject(source.id, source)));
        if (typeof Markers == 'object') {
            if (source.text) {
                var offset = Vec3.multiplyVbyV(
                    Vec3.subtract(Vec3.HALF, result.registrationPoint || Vec3.HALF), result.dimensions
                );
                //offset.y -= .5;
                var correction = Quat.IDENTITY;//inverse(Quat.lookAtSimple(Camera.position, source.position));
                Markers.update({
                    visible: false,
                    id: source.id,
                    color: source.color,
                    position: source.position,
                    rotation: source.rotation,
                    text: {
                        text: source.text || '??',
                        fontSize: 3,
                        margin: 0,
                        textMargin:0,
                        localPosition: Vec3.multiply(-.15, Vec3.multiplyQbyV(source.rotation, offset)),
                        localRotation: Quat.multiply(Quat.fromPitchYawRollDegrees(90,120+45,0), correction),
                        isFacingAvatar: false,
                        position: source.position,
                        rotation: source.rotation,
                    },
                    //hoverID: Leopoly.metadata.entityID,
                });
            } else if (Markers.exists(source.id)) {
                Markers.remove(source.id);
            }
        }
        return result;
    },
    _calculatePosition: function(props) {
        props.dimensions = props.dimensions || Overlays.getProperty(props.id, 'dimensions') || Vec3.ONE;
        var offset = Vec3.multiplyVbyV(Vec3.subtract(Vec3.HALF, props.registrationPoint), props.dimensions)
        var rot = props.rotation;//Quat.multiply(props.rotation, Quat.fromPitchYawRollDegrees(0,0,0));
        return Vec3.sum(props.position, Vec3.multiplyQbyV(rot, offset));
        //print(JSON.stringify(offset));
    },
    isValidObject: function(id) {
        var type = Entities.getNestableType(id);
        return type === 'entity' || type === 'overlay';
    },
    editObject: function(id, source) {
        var props = JSON.parse(JSON.stringify(source));
        //props.rotation = Quat.multiply(Quat.fromPitchYawRollDegrees(90,0,0), props.rotation);
        var type = Entities.getNestableType(id);
        if (type === 'entity') {
            Entities.editEntity(id, props);
            return Entities.getEntityProperties(id, Object.keys(props));
        } else if (type === 'overlay') {
            if (props.position && props.registrationPoint) {
                props.position = this._calculatePosition(props);
                //props.position = Vec3.sum(props.localPosition||Vec3.ZERO, Vec3.sum(props.position, Vec3.multiplyQbyV(rot, offset));
            }
            Overlays.editOverlay(id, props);
            return Overlays.getProperties(id, Object.keys(props));
        } else {
            print('editObject -- Unknown', id, Object.keys(props));
            return { id: undefined };
        }
    },
    destroy: function(source) {
        if (typeof source === 'string') {
            source = { id: source };
        }
        if (Uuid.isNull(source.id)) {
            var self = this;
            source.id = Object.keys(this.cursors).filter(function(id) {
                return self.cursors[id].name === source.name;
            })[0];
            if (Uuid.isNull(source.id)) {
                print('cursors.destroy -- couldn\'t find cursor by name:', source.name);
                return false;
            }
        }
        var type = Entities.getNestableType(source.id);
        if (type === 'entity') {
            return Entities.deleteEntity(source.id);
        } else if (type === 'overlay') {
            return Overlays.deleteOverlay(source.id);
        } else {
            print('cursor.destroy -- unknown cursor:', source.id, type);
        }
        return false;
    },
    dispose: function() {
        var cursors = this.cursors;
        this.cursors = {};
        for (var id in cursors) {
            this.destroy(id);
        }
        return cursors;
    },
};
Script.scriptEnding.connect(module.exports, 'dispose');
print('virtual-cursor', module.exports.version);
