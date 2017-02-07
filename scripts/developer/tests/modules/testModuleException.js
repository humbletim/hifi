if (typeof Script !== 'object') {
    print = console.log.bind(console);
    Script = { require: require };
}

// test catching require exceptions
try {
  print("exception: ", Script.require("./exception.js"));
} catch(e) {
  print("caught exception: ", e);
}

// test uncaught require exceptions
print("exception: ", Script.require("./exception.js"));
