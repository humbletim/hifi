function constructor() {
    print("===========================================================");
    print("entityConstructorNested");
    var MyEntity = Script.require('./entityConstructor.js');
    print("===== MyEntity", Object.keys(new MyEntity));
    return new MyEntity(Script.resolvePath('#viarequirex')+'\n' + (new Date).toJSON());
}

try { module.exports = constructor; } catch(e) { constructor; }

