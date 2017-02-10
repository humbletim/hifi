if (typeof Script !== 'object') {
    print = console.log.bind(console);
    Script = { require: require };
}

// test catching require exceptions
try {
  print("exception: ", Script.require("./exception.js"));
} catch(e) {
  print("caught require exception: ", e);
}

try {
  print("unresolvable exception: ", Script.require("about:404"));
} catch(e) {
  print("caught require exception: ", e);
}

// test uncaught require exceptions
print("exception: ", Script.require("./exception.js"));
