(function constructor() {
    // .require within Entity constructor
    var entityConstructor = Script.require('./entityConstructor.js');

    MyEntity.prototype = new entityConstructor(Script.resolvePath(''));
    function MyEntity() {
        this.preload =  function(uuid) {
            print("===========================================================");
            print("Regular Entity script -- with embedded requires", uuid);
            var filename = Script.resolvePath('');
            print("filename", filename);
            // .require within preload handler
            var example = Script.require('../example.json');
            print("example.name", example.name);
            Entities.editEntity(uuid, {
                text: filename.split('/').pop() + "\n" + example.name,
                dimensions: { x: 4, y: .4, z: .01 },
            });
        };
    }
    return new MyEntity();
})
