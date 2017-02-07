(function constructor() {
    return {
        preload: function(uuid) {
            print("===========================================================");
            print("Regularly Entity script -- with inner require", uuid);
            var filename = Script.resolvePath('');
            var example = Script.require('../example.json');
            print("filename", filename);
            print("example.name", example.name);
            Entities.editEntity(uuid, {
                text: filename.split('/').pop() + "\n" + example.name,
                dimensions: { x: 4, y: .4, z: .01 },
            });
        },
    };
})
