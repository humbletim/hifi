function MyEntity(filename) {
    return {
        preload: function(uuid) {
            print("===========================================================");
            print("OK!! Entity preloaded via MyEntity module", uuid, typeof module);
            if (typeof module === 'object') {
                print("module.filename", module.filename);
                print("module.parent.filename", module.parent && module.parent.filename);
                filename = filename || (module.parent ? module.parent.filename : module.filename);
            } else {
                filename = filename || Script.resolvePath('');
            }
            Entities.editEntity(uuid, {
                text: filename.split('/').pop(),
                dimensions: { x: 4, y: .4, z: .01 },
            });
        },
        clickDownOnEntity: function(uuid, evt) {
            print('... updating scriptTimestamp ...');
            Entities.editEntity(uuid, {
                text: '... reloading script ...',
                scriptTimestamp: +new Date
            });
        },
    };
}

try { module.exports = MyEntity; } catch(e) { }
print('entityConstructor MyEntity', typeof MyEntity);
(MyEntity)
