import std.io.print
import std.io.eprint
import std.text.string.String
import std.core.error.Exception

struct HelloException : Exception {}

@throws(HelloException)
fn throws_hello_exception() -> never {
  throw HelloException {}
}

enum Option<T> {
  None
  Some { value: T }
}

var mut count = 0

struct Container<T> {
  private:
    var value: T
    var count: i32
  
  public:
    fn Container(value: T) -> Container<T> {
      count = count + 1

      print("Container constructor called ")
      print(String::from(count))
      print("\n")

      return Container {
        value: value,
        count: count
      }
    }

    fn ~Container()-> void {
      print("Container destructor called ")
      print(String::from(self->count))
      print("\n")
    }
}

public fn main() -> i32 {
  print("Hi\n")
  defer print("defer after print hi\n")

  var container = Container(1)
  do {
    var container = Container(1)
  }

  var empty_value = Option<i32>::None
  when (empty_value) {
    Option::None -> print("empty value is none\n"),
    Option::Some { value } -> eprint("empty value is full\n")
  }

  var full_value = Option::Some { value: 1 }
  when (full_value) {
    Option::None -> eprint("full value is none\n"),
    Option::Some { value } -> {
      print("full value is ")
      print(String::from(value))
      print("\n")
    }
  }

  throws_hello_exception() catch {
    error: HelloException -> {
      eprint("Caught exception\n")
    }
  }

  try {
    throws_hello_exception()
  } catch {
    error: HelloException -> {
      eprint("Caught exception in try block\n")
    }
  }

  return 0
}
