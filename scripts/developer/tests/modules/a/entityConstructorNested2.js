function constructor() {
    print("===========================================================");
    print("entityConstructorNested2");

    // when used as a module this will invalidate ourselves and any child modules
    function clearUsedModuleCache(module) {
        print("====>_clearUsedModuleCache", JSON.stringify({
            module: module,
            children: module.children.map(function(x) { return x.id }),
        },0,2));
        module.children.concat(module).forEach(function(mod) {
            print('setting require.cache[' + mod.id + '] = null');
            Script.require.cache[mod.id] = null;
        });
    }


    var Entity = Script.require('./entityConstructorNested.js');
    print("===== EntityNested2", Object.keys(new Entity));
    function SubEntity() {}
    SubEntity.prototype = new MyEntity(Script.resolvePath('#viarequirex')+'\n' + (new Date).toJSON());
    var entity = new SubEntity();
    entity.clickDownOnEntity = function(uuid, evt) {
        if (evt.isShifted || !evt.isPrimaryButton) {
            var id = Script.require.resolve('./entityConstructorNested.js');
            clearUsedModuleCache(typeof module === 'object' ? module : Script.require.cache[id]);
        }
        SubEntity.prototype.clickDownOnEntity.apply(this, arguments);
    };
    return entity;
}

try { module.exports = constructor; } catch(e) { constructor; }

