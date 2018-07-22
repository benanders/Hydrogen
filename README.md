
The Hydrogen Programming Language
---------------------------------

Hydrogen is a toy programming language I'm writing mostly for my own edification. It's an **interpreted**, **dynamically typed** language written in C and hand-coded assembly. It comes with an **easy-to-use C API** for embedding within your own programs!

The core is written in C and hand-coded assembly, with no dependencies beyond the C standard library. The tests are written in C++ using the [Google Test](https://github.com/google/googletest) framework.

### Code Sample

Here's some Hydrogen code:

```rust
import "io"

struct Node {
	name, child
}

fn (Node) new(name, child) {
	self.name = name
	self.child = nil
}

fn (Node) print() {
	io.print("[" .. self.name)
	if self.child {
		io.print(", ")
		self.child.print()
	}
	io.print("]")
}

let root = new Node("1", new Node("2", new Node("3", nil)))
root.print() // Prints [1, [2, [3]]]
```

### Building

Here's a guide on how to **build Hydrogen** from its source. This guide uses [CMake](https://cmake.org/) and Unix Makefiles.

Clone the repository, create a build folder, generate the Makefile, and build the source:

```bash
$ git clone https://github.com/benanders/Hydrogen
$ cd Hydrogen
$ mkdir build
$ cd build
$ cmake ..
$ make
```

You can then use the Hydrogen **command line interface** by:

```bash
$ ./hydrogen
```

You can run the **tests** by:

```bash
$ make test
```

### License

Hydrogen is licensed under the MIT license. This means you can do basically whatever you want with the code. See the `LICENSE` file for more details.
