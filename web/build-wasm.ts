const proc = Bun.spawnSync([
  "clang",
  "--target=wasm32",
  "-std=c99",
  "-Wall",
  "-Wextra",
  "-Wpedantic",
  "-Wshadow",
  "-Wconversion",
  "-Wdouble-promotion",
  "-Wformat=2",
  "-Wnull-dereference",
  "-Wstrict-prototypes",
  "-flto",
  "-O3",
  "-nostdlib",
  "-Wl,--no-entry",
  "-Wl,--export-all",
  "-Wl,--lto-O3",
  "-Wl,-z,stack-size=8388608",
  "-I../src",
  "-o",
  "parser.wasm",
  "../src/web.c",
]);

const stderr = new TextDecoder().decode(proc.stderr);
if (stderr) console.error(stderr);
if (proc.exitCode !== 0) process.exit(proc.exitCode);
console.log("parser.wasm built successfully");
