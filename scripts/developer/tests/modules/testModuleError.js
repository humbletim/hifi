if (typeof Script !== 'object') {
    print = console.log.bind(console);
    Script = { require: require };
}
print("exported (but not thrown) 'Error' object: ", JSON.stringify(Script.require("./error.js")));
