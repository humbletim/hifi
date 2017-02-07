if (typeof Script !== 'object') {
    print = console.log.bind(console);
    Script = { require: require };
}
print("error: ", Script.require("./error.js"));
